// MiaUI backend for the RG35XX (github.com/miaui-cfw/MiaUI-GPU-RG35XX): SDL2 with OpenGL ES 2 on the
// PowerVR SGX544. Input, memory logging and the generic joystick path follow sdl1.c's GarlicOS code.
// The software renderer's frame is converted to RGB565 at the game's own size and uploaded as a texture;
// the GPU scales it to the 640x480 screen. Going through sdl12-compat cost about 12 ms a frame instead.
// With --renderer modern-gl the game draws on the GPU itself, into an offscreen target of the same size the
// software renderer gets (320x240 for AM2R), which is then scaled to the screen the same way. The game sees
// the same window size with either renderer: AM2R sizes its HUD from it.
#include <ctype.h>
#include "string_compat.h"
#include "stdio_compat.h"
#include <stdlib.h>

#include <SDL2/SDL.h>
#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>

#include "common.h"
#include "input_recording.h"
#include "platformdefs.h"
#include "gettime.h"
#include "runner_mouse.h"

static Runner *g_runner;
static int32_t fbWidth, fbHeight;
static SDL_Window* window;
static SDL_GLContext glContext;
static SDL_Joystick* openJoysticks[MAX_GAMEPADS];

typedef struct {
    bool valid;
    int max_button;
    int max_axis;
    int max_hat;

    int button_map[GP_BUTTON_COUNT];
    int axis_map[GP_AXIS_COUNT];
    int hat_map[GP_BUTTON_COUNT];
    int hat_mask[GP_BUTTON_COUNT];
    int axis_button_map[GP_BUTTON_COUNT];
    float axis_button_sign[GP_BUTTON_COUNT];
} GamepadMapping;

static GamepadMapping joystickMappings[MAX_GAMEPADS];


// By default the game sees a 320x240 window, which the GPU doubles to the 640x480 screen.
// RG35XX_FULL_RES gives it the whole screen instead.
static int rgGameW = 320;
static int rgGameH = 240;
#define RG35XX_SCREEN_W 640
#define RG35XX_SCREEN_H 480
// MiaUI's gamepad driver (its gamecontrollerdb.txt): the D-pad and L2/R2 are plain buttons.
// MiaUI's keymon handles the volume keys system-wide.
#define RG35XX_BTN_START 9
#define RG35XX_BTN_MENU 10

typedef struct {
    int button;
    int32_t gmlKey; // AM2R's default key for this control, as in its config.ini
    const char* label; // the button's name on the device, shown in AM2R's hints and keyboard settings
    bool down;
} RgInputBinding;

static RgInputBinding rgBindings[] = {
    { 13, VK_UP, "Up", false },
    { 14, VK_DOWN, "Down", false },
    { 15, VK_LEFT, "Left", false },
    { 16, VK_RIGHT, "Right", false },
    { 0, 'Z', "A", false },          // A: jump, menu OK
    { 1, 'X', "B", false },          // B: fire, menu cancel
    { 3, 'X', "Y", false },          // Y: fire (same key as B, so hints say B)
    { 2, VK_CONTROL, "X", false },   // X: morph
    { 4, VK_SHIFT, "L1", false },    // L1: hold to aim diagonally, with up or down
    { 5, 'A', "R1", false },         // R1: hold to arm missiles
    { 6, 'C', "L2", false },         // L2: second aim button, unused in one-button aiming
    { 7, VK_SPACE, "R2", false },    // R2: aim lock
    { 8, 'S', "Select", false },     // Select: switch weapon
    { RG35XX_BTN_START, VK_ENTER, "Start", false },
    { RG35XX_BTN_MENU, VK_ESCAPE, "Menu", false },
};

const char* platformGetButtonLabel(int32_t gmlKey) {
    for (size_t i = 0; i < sizeof(rgBindings) / sizeof(rgBindings[0]); i++) {
        if (rgBindings[i].gmlKey == gmlKey) return rgBindings[i].label;
    }
    return NULL;
}

static void parseMappingField(GamepadMapping* mapping, const char* key, const char* val) {
    int gml_btn = -1;
    int gml_axis = -1;

    if (strcmp(key, "a") == 0) gml_btn = 0;
    else if (strcmp(key, "b") == 0) gml_btn = 1;
    else if (strcmp(key, "x") == 0) gml_btn = 2;
    else if (strcmp(key, "y") == 0) gml_btn = 3;
    else if (strcmp(key, "leftshoulder") == 0) gml_btn = 4;
    else if (strcmp(key, "rightshoulder") == 0) gml_btn = 5;
    else if (strcmp(key, "lefttrigger") == 0) gml_btn = 6;
    else if (strcmp(key, "righttrigger") == 0) gml_btn = 7;
    else if (strcmp(key, "back") == 0) gml_btn = 8;
    else if (strcmp(key, "start") == 0) gml_btn = 9;
    else if (strcmp(key, "leftstick") == 0) gml_btn = 10;
    else if (strcmp(key, "rightstick") == 0) gml_btn = 11;
    else if (strcmp(key, "dpup") == 0) gml_btn = 12;
    else if (strcmp(key, "dpdown") == 0) gml_btn = 13;
    else if (strcmp(key, "dpleft") == 0) gml_btn = 14;
    else if (strcmp(key, "dpright") == 0) gml_btn = 15;
    else if (strcmp(key, "guide") == 0) gml_btn = 16;
    else if (strcmp(key, "leftx") == 0) gml_axis = 0;
    else if (strcmp(key, "lefty") == 0) gml_axis = 1;
    else if (strcmp(key, "rightx") == 0) gml_axis = 2;
    else if (strcmp(key, "righty") == 0) gml_axis = 3;

    if (gml_btn == -1 && gml_axis == -1) return;

    if (val[0] == 'b') {
        int b = atoi(val + 1);
        if (gml_btn != -1) {
            mapping->button_map[gml_btn] = b;
            if (b > mapping->max_button) mapping->max_button = b;
        }
    } else if (val[0] == 'a') {
        int a = atoi(val + 1);
        if (gml_axis != -1) {
            mapping->axis_map[gml_axis] = a;
            if (a > mapping->max_axis) mapping->max_axis = a;
        } else if (gml_btn != -1) {
            mapping->axis_button_map[gml_btn] = a;
            mapping->axis_button_sign[gml_btn] = 1.0f;
            if (a > mapping->max_axis) mapping->max_axis = a;
        }
    } else if (val[0] == '+' && val[1] == 'a') {
        int a = atoi(val + 2);
        if (gml_btn != -1) {
            mapping->axis_button_map[gml_btn] = a;
            mapping->axis_button_sign[gml_btn] = 1.0f;
            if (a > mapping->max_axis) mapping->max_axis = a;
        } else if (gml_axis != -1) {
            mapping->axis_map[gml_axis] = a;
            if (a > mapping->max_axis) mapping->max_axis = a;
        }
    } else if (val[0] == '-' && val[1] == 'a') {
        int a = atoi(val + 2);
        if (gml_btn != -1) {
            mapping->axis_button_map[gml_btn] = a;
            mapping->axis_button_sign[gml_btn] = -1.0f;
            if (a > mapping->max_axis) mapping->max_axis = a;
        } else if (gml_axis != -1) {
            mapping->axis_map[gml_axis] = a;
            if (a > mapping->max_axis) mapping->max_axis = a;
        }
    } else if (val[0] == 'h') {
        int h = atoi(val + 1);
        const char* dot = strchr(val, '.');
        if (dot && gml_btn != -1) {
            int dir = atoi(dot + 1);
            mapping->hat_map[gml_btn] = h;
            mapping->hat_mask[gml_btn] = dir;
            if (h > mapping->max_hat) mapping->max_hat = h;
        }
    }
}

#if defined(_WIN32) || defined(__WIN32__) || defined(__WINDOWS__)
#define MAPPING_PLATFORM_STR "platform:Windows"
#elif defined(__APPLE__)
    #include <TargetConditionals.h>
    #if TARGET_OS_IPHONE || TARGET_IPHONE_SIMULATOR
        #define MAPPING_PLATFORM_STR "platform:iOS"
    #else
        #define MAPPING_PLATFORM_STR "platform:Mac OS X"
    #endif
#elif defined(__ANDROID__)
#define MAPPING_PLATFORM_STR "platform:Android"
#elif defined(__linux__) || defined(__gnu_linux__)
#define MAPPING_PLATFORM_STR "platform:Linux"
#else
#define MAPPING_PLATFORM_STR "platform:Unknown" //Gamepad wont work at all here
#endif

static void loadGamepadMappings(void) {
    const char* dbPath = "gamecontrollerdb.txt";
    FILE* f = fopen(dbPath, "rb");
    if (!f) {
        logWarn("Gamepad: SDL gamecontrollerdb.txt not found at %s, ignoring mappings\n", dbPath);
        return;
    }

    char line[1024];
    while (fgets(line, sizeof(line), f)) {
        size_t len = strlen(line);
        while (len > 0 && (line[len-1] == '\n' || line[len-1] == '\r')) {
            line[--len] = '\0';
        }
        if (len == 0 || line[0] == '#') continue;

        char* guid = line;
        char* name = strchr(guid, ',');
        if (!name) continue;
        *name++ = '\0';

        char* mapping_str = strchr(name, ',');
        if (!mapping_str) continue;
        *mapping_str++ = '\0';

        if (strstr(mapping_str, "platform:") != NULL && strstr(mapping_str, MAPPING_PLATFORM_STR) == NULL) {
            continue;
        }

        GamepadMapping temp;
        temp.valid = true;
        temp.max_button = -1;
        temp.max_axis = -1;
        temp.max_hat = -1;
        for (int i = 0; i < GP_BUTTON_COUNT; i++) {
            temp.button_map[i] = -1;
            temp.hat_map[i] = -1;
            temp.hat_mask[i] = -1;
            temp.axis_button_map[i] = -1;
            temp.axis_button_sign[i] = 1.0f;
        }
        for (int i = 0; i < GP_AXIS_COUNT; i++) {
            temp.axis_map[i] = -1;
        }

        char* token = strtok(mapping_str, ",");
        while (token) {
            char* colon = strchr(token, ':');
            if (colon) {
                *colon = '\0';
                parseMappingField(&temp, token, colon + 1);
            }
            token = strtok(NULL, ",");
        }

        for (int i = 0; i < MAX_GAMEPADS; i++) {
            SDL_Joystick* joy = openJoysticks[i];
            if (joy && !joystickMappings[i].valid) {
                const char* jname = SDL_JoystickNameForIndex(i);
                if (jname && strcasecmp(jname, name) == 0) {
                    joystickMappings[i] = temp;
                    logInfo("Gamepad: Mapped '%s' (slot %d)\n", jname, i);
                }
            }
        }
    }
    fclose(f);
}

void platformSetWindowTitle(const char* title) {
    (void) title;
}

bool platformGetWindowSize(int32_t* outW, int32_t* outH) {
    if (!outW || !outH) return false;
    *outW = fbWidth;
    *outH = fbHeight;
    return true;
}

bool platformGetScaledWindowSize(int32_t* outW, int32_t* outH) {
    return platformGetWindowSize(outW, outH);
}

void platformSetWindowSize(int32_t width, int32_t height) {
    (void) width; (void) height;
}

void platformGetMousePos(double *xPos, double *yPos) {
    if (!xPos || !yPos) return;
    *xPos = 0;
    *yPos = 0;
}

static bool platformGetWindowFocus(void) {
    return true;
}

// ===[ Presenting ]===
static GLuint fbTexture;
static GLuint fbProgram;
static GLint fbFlipLocation;
static GLuint hostFbo; // the GPU renderer's target, at the game's size
static PFNGLBINDVERTEXARRAYOESPROC bindVertexArrayOES;
static uint16_t* fbPixels565;
static const uint32_t* nextFbPixels = NULL;

static GLuint compileShader(GLenum type, const char* source) {
    GLuint s = glCreateShader(type);
    glShaderSource(s, 1, &source, NULL);
    glCompileShader(s);
    GLint ok = 0;
    glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char log[512];
        glGetShaderInfoLog(s, sizeof(log), NULL, log);
        logError("MiaUI: shader compile failed: %s\n", log);
    }
    return s;
}

static bool setupPresent(void) {
    fbProgram = glCreateProgram();
    glAttachShader(fbProgram, compileShader(GL_VERTEX_SHADER,
        "attribute vec2 pos;\n"
        "uniform float flipY;\n"
        "varying vec2 uv;\n"
        "void main() { uv = vec2(pos.x * 0.5 + 0.5, 0.5 + flipY * pos.y * 0.5); gl_Position = vec4(pos, 0.0, 1.0); }\n"));
    glAttachShader(fbProgram, compileShader(GL_FRAGMENT_SHADER,
        "precision mediump float;\n"
        "varying vec2 uv;\n"
        "uniform sampler2D tex;\n"
        "void main() { gl_FragColor = texture2D(tex, uv); }\n"));
    glBindAttribLocation(fbProgram, 0, "pos");
    glLinkProgram(fbProgram);
    GLint ok = 0;
    glGetProgramiv(fbProgram, GL_LINK_STATUS, &ok);
    if (!ok) {
        logError("MiaUI: present shader didn't link\n");
        return false;
    }
    glUseProgram(fbProgram);
    glUniform1i(glGetUniformLocation(fbProgram, "tex"), 0);
    fbFlipLocation = glGetUniformLocation(fbProgram, "flipY");
    // The software frame is uploaded top row first; an FBO's texture has its bottom row first.
    glUniform1f(fbFlipLocation, gfx == SOFTWARE ? -1.0f : 1.0f);

    glGenTextures(1, &fbTexture);
    glBindTexture(GL_TEXTURE_2D, fbTexture);
    // Nearest: the 2x doubling stays as sharp as the CPU doubling it replaces.
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    if (gfx != SOFTWARE) {
        // The GPU renderer draws the frame into this, then platformSwapBuffers scales it to the screen.
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, rgGameW, rgGameH, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
        glGenFramebuffers(1, &hostFbo);
        glBindFramebuffer(GL_FRAMEBUFFER, hostFbo);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, fbTexture, 0);
        GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        if (status != GL_FRAMEBUFFER_COMPLETE) {
            logError("MiaUI: the offscreen target isn't complete (0x%x)\n", status);
            return false;
        }
        bindVertexArrayOES = (PFNGLBINDVERTEXARRAYOESPROC) SDL_GL_GetProcAddress("glBindVertexArrayOES");
        return true;
    }

    static const float quad[] = { -1, -1, 1, -1, -1, 1, 1, 1 };
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, quad);
    glEnableVertexAttribArray(0);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 2);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, rgGameW, rgGameH, 0, GL_RGB, GL_UNSIGNED_SHORT_5_6_5, NULL);
    glViewport(0, 0, RG35XX_SCREEN_W, RG35XX_SCREEN_H);
    glDisable(GL_BLEND);
    glDisable(GL_DEPTH_TEST);

    fbPixels565 = malloc((size_t) rgGameW * rgGameH * sizeof(uint16_t));
    return fbPixels565 != NULL;
}

unsigned int platformGetHostFramebuffer(void) {
    return hostFbo;
}

// Scales the GPU renderer's frame to the screen, leaving the GL state as the renderer had it.
static void presentHostFramebuffer(void) {
    GLint program, activeTexture, texture0, arrayBuffer, framebuffer, vertexArray = 0, viewport[4];
    GLint attribEnabled, attribSize, attribType, attribNormalized, attribStride, attribBuffer;
    GLvoid* attribPointer;
    glGetIntegerv(GL_CURRENT_PROGRAM, &program);
    glGetIntegerv(GL_ACTIVE_TEXTURE, &activeTexture);
    glActiveTexture(GL_TEXTURE0);
    glGetIntegerv(GL_TEXTURE_BINDING_2D, &texture0);
    glGetIntegerv(GL_ARRAY_BUFFER_BINDING, &arrayBuffer);
    glGetIntegerv(GL_FRAMEBUFFER_BINDING, &framebuffer);
    glGetIntegerv(GL_VIEWPORT, viewport);
    if (bindVertexArrayOES != NULL) {
        glGetIntegerv(GL_VERTEX_ARRAY_BINDING_OES, &vertexArray);
        bindVertexArrayOES(0);
    }
    // Attribute 0 of the default vertex array, which the renderer uses when it has no VAOs.
    glGetVertexAttribiv(0, GL_VERTEX_ATTRIB_ARRAY_ENABLED, &attribEnabled);
    glGetVertexAttribiv(0, GL_VERTEX_ATTRIB_ARRAY_SIZE, &attribSize);
    glGetVertexAttribiv(0, GL_VERTEX_ATTRIB_ARRAY_TYPE, &attribType);
    glGetVertexAttribiv(0, GL_VERTEX_ATTRIB_ARRAY_NORMALIZED, &attribNormalized);
    glGetVertexAttribiv(0, GL_VERTEX_ATTRIB_ARRAY_STRIDE, &attribStride);
    glGetVertexAttribiv(0, GL_VERTEX_ATTRIB_ARRAY_BUFFER_BINDING, &attribBuffer);
    glGetVertexAttribPointerv(0, GL_VERTEX_ATTRIB_ARRAY_POINTER, &attribPointer);
    GLboolean blend = glIsEnabled(GL_BLEND);
    GLboolean scissor = glIsEnabled(GL_SCISSOR_TEST);
    GLboolean colorMask[4];
    glGetBooleanv(GL_COLOR_WRITEMASK, colorMask);

    static const float quad[] = { -1, -1, 1, -1, -1, 1, 1, 1 };
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glViewport(0, 0, RG35XX_SCREEN_W, RG35XX_SCREEN_H);
    glDisable(GL_BLEND);
    glDisable(GL_SCISSOR_TEST);
    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    glUseProgram(fbProgram);
    glBindTexture(GL_TEXTURE_2D, fbTexture);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, quad);
    glEnableVertexAttribArray(0);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

    // Put back what the renderer had.
    glBindBuffer(GL_ARRAY_BUFFER, (GLuint) attribBuffer);
    glVertexAttribPointer(0, attribSize, (GLenum) attribType, (GLboolean) attribNormalized, attribStride, attribPointer);
    if (!attribEnabled) glDisableVertexAttribArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, (GLuint) arrayBuffer);
    if (bindVertexArrayOES != NULL) bindVertexArrayOES((GLuint) vertexArray);
    glBindTexture(GL_TEXTURE_2D, (GLuint) texture0);
    glActiveTexture((GLenum) activeTexture);
    glUseProgram((GLuint) program);
    glColorMask(colorMask[0], colorMask[1], colorMask[2], colorMask[3]);
    if (blend) glEnable(GL_BLEND);
    if (scissor) glEnable(GL_SCISSOR_TEST);
    glViewport(viewport[0], viewport[1], viewport[2], viewport[3]);
    glBindFramebuffer(GL_FRAMEBUFFER, (GLuint) framebuffer);
}

bool platformInit(int32_t reqW, int32_t reqH, const char *title, bool headless) {
    (void) reqW; (void) reqH; (void) title;
    if (getenv("RG35XX_FULL_RES") != NULL) {
        rgGameW = RG35XX_SCREEN_W;
        rgGameH = RG35XX_SCREEN_H;
    }
    fbWidth = rgGameW;
    fbHeight = rgGameH;

    if (SDL_Init((headless ? 0 : SDL_INIT_VIDEO) | SDL_INIT_TIMER | SDL_INIT_JOYSTICK)) {
        logError("Failed to initialize SDL: %s\n", SDL_GetError());
        return false;
    }

    SDL_JoystickEventState(SDL_IGNORE);
    int numJoysticks = SDL_NumJoysticks();
    for (int i = 0; i < MAX_GAMEPADS; i++) {
        memset(&joystickMappings[i], 0, sizeof(GamepadMapping));
        joystickMappings[i].valid = false;
        openJoysticks[i] = i < numJoysticks ? SDL_JoystickOpen(i) : NULL;
    }
    loadGamepadMappings();

    if (headless) return true;

    // A 16-bit screen, like MiaUI's own UI: the game's frame is 16-bit anyway.
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_ES);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
    SDL_GL_SetAttribute(SDL_GL_RED_SIZE, 5);
    SDL_GL_SetAttribute(SDL_GL_GREEN_SIZE, 6);
    SDL_GL_SetAttribute(SDL_GL_BLUE_SIZE, 5);
    SDL_GL_SetAttribute(SDL_GL_ALPHA_SIZE, 0);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 0);
    SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 0);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);

    window = SDL_CreateWindow("Butterscotch", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
                              RG35XX_SCREEN_W, RG35XX_SCREEN_H, SDL_WINDOW_OPENGL);
    if (!window) {
        logError("Fatal: Could not open the window: %s\n", SDL_GetError());
        return false;
    }
    glContext = SDL_GL_CreateContext(window);
    if (!glContext) {
        logError("Fatal: Could not create the GLES2 context: %s\n", SDL_GetError());
        return false;
    }
    // Test hook: MIAUI_VSYNC=0 presents without waiting for the screen refresh (may tear).
    const char* vsync = getenv("MIAUI_VSYNC");
    int interval = (vsync != NULL && vsync[0] == '0') ? 0 : 1;
    SDL_GL_SetSwapInterval(interval);
    logInfo("MiaUI: GLES2 %s, %s renderer, game %dx%d, vsync %d\n", (const char*) glGetString(GL_RENDERER),
            gfx == SOFTWARE ? "software" : "GPU", rgGameW, rgGameH, interval);
    SDL_ShowCursor(SDL_DISABLE);

    return setupPresent();
}

void platformExit(void) {
    for (int i = 0; i < MAX_GAMEPADS; i++) {
        if (openJoysticks[i]) {
            SDL_JoystickClose(openJoysticks[i]);
            openJoysticks[i] = NULL;
        }
    }
    if (glContext) SDL_GL_DeleteContext(glContext);
    if (window) SDL_DestroyWindow(window);
    free(fbPixels565);
    SDL_Quit();
}

static void platformSetCursor(int32_t cursorType) {
    (void) cursorType;
}

void platformInitFunctions(Runner *runner) {
    g_runner = runner;
    runner->windowHasFocus = platformGetWindowFocus;
    runner->setCursor = platformSetCursor;
    runner->currentCursor = GML_CR_DEFAULT;
}

#ifdef ENABLE_SW_RENDERER
void platformSetNextFramebuffer(uint32_t* framebuffer, int width, int height) {
    (void) width; (void) height;
    nextFbPixels = framebuffer;
}
#endif

// Test tool: the process's size and the memory the system could still give it.
void rg35xxLogMemory(const char* where) {
    long pages = 0, ignored = 0;
    FILE* statm = fopen("/proc/self/statm", "r");
    if (statm != NULL) {
        if (fscanf(statm, "%ld %ld", &ignored, &pages) != 2) pages = 0;
        fclose(statm);
    }
    long freeKb = 0, buffersKb = 0, cachedKb = 0;
    FILE* info = fopen("/proc/meminfo", "r");
    if (info != NULL) {
        char row[80];
        while (fgets(row, sizeof(row), info) != NULL) {
            sscanf(row, "MemFree: %ld", &freeKb);
            sscanf(row, "Buffers: %ld", &buffersKb);
            sscanf(row, "Cached: %ld", &cachedKb);
        }
        fclose(info);
    }
    logInfo("MEM %s: game %ld MB, free %ld MB, reclaimable %ld MB\n", where, pages * 4 / 1024, freeKb / 1024, (buffersKb + cachedKb) / 1024);
}

void platformSwapBuffers(void) {
    if (window == NULL) return;
    if (gfx != SOFTWARE) {
        presentHostFramebuffer();
        SDL_GL_SwapWindow(window);
        return;
    }
    if (nextFbPixels == NULL) return;
    {
        // Test hook: MIAUI_DUMP_FRAMES="400,850" saves those software frames as /tmp/swframe-N.ppm, to compare
        // with the GPU renderer's --screenshot of the same frame.
        static const char* dumpFrames = NULL;
        static bool dumpRead = false;
        if (!dumpRead) { dumpFrames = getenv("MIAUI_DUMP_FRAMES"); dumpRead = true; }
        if (dumpFrames != NULL) {
            char key[16];
            snprintf(key, sizeof(key), "%d", (int) g_runner->frameCount);
            const char* hit = strstr(dumpFrames, key);
            size_t len = strlen(key);
            if (hit != NULL && (hit == dumpFrames || hit[-1] == ',') && (hit[len] == ',' || hit[len] == '\0')) {
                char path[64];
                snprintf(path, sizeof(path), "/tmp/swframe-%s.ppm", key);
                FILE* f = fopen(path, "wb");
                if (f != NULL) {
                    fprintf(f, "P6\n%d %d\n255\n", rgGameW, rgGameH);
                    for (size_t i = 0; i < (size_t) rgGameW * rgGameH; i++) {
                        uint32_t c = nextFbPixels[i];
                        unsigned char rgb[3] = { (unsigned char) (c >> 16), (unsigned char) (c >> 8), (unsigned char) c };
                        fwrite(rgb, 1, 3, f);
                    }
                    fclose(f);
                }
            }
        }
    }
    // The software renderer's frame is 0xXXRRGGBB.
    const uint32_t* src = nextFbPixels;
    uint16_t* dst = fbPixels565;
    const size_t count = (size_t) rgGameW * rgGameH;
    for (size_t i = 0; i < count; i++) {
        uint32_t c = src[i];
        dst[i] = (uint16_t) (((c >> 8) & 0xf800) | ((c >> 5) & 0x07e0) | ((c >> 3) & 0x001f));
    }
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, rgGameW, rgGameH, GL_RGB, GL_UNSIGNED_SHORT_5_6_5, fbPixels565);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    SDL_GL_SwapWindow(window);
}

void *platformGetProcAddress(const char *name) {
    return SDL_GL_GetProcAddress(name);
}

// Feeds the buttons in as AM2R's default keys. Returns true on Menu + Start, which quits.
static bool rg35xxPollInput(void) {
    SDL_Joystick* joy = openJoysticks[0];
    if (joy == NULL) return false;

    if (SDL_JoystickGetButton(joy, RG35XX_BTN_MENU) && SDL_JoystickGetButton(joy, RG35XX_BTN_START))
        return true;

    for (size_t i = 0; i < sizeof(rgBindings) / sizeof(rgBindings[0]); i++) {
        RgInputBinding* b = &rgBindings[i];
        bool down = SDL_JoystickGetButton(joy, b->button) != 0;
        if (down == b->down) continue;
        b->down = down;
        if (down) RunnerKeyboard_onKeyDown(g_runner->keyboard, b->gmlKey);
        else RunnerKeyboard_onKeyUp(g_runner->keyboard, b->gmlKey);
    }
    return false;
}

bool platformHandleEvents(void) {
    SDL_JoystickUpdate();
    if (rg35xxPollInput()) return true;

    g_runner->gamepads->connectedCount = 0;
    for (int slotIdx = 0; slotIdx < MAX_GAMEPADS; slotIdx++) {
        GamepadSlot* slot = g_runner->gamepads->slots + slotIdx;
        SDL_Joystick* joy = openJoysticks[slotIdx];

        memcpy(slot->buttonDownPrev, slot->buttonDown, sizeof(slot->buttonDown));
        memset(slot->buttonDown, 0, sizeof(slot->buttonDown));
        memset(slot->buttonPressed, 0, sizeof(slot->buttonPressed));
        memset(slot->buttonReleased, 0, sizeof(slot->buttonReleased));
        memset(slot->buttonValue, 0, sizeof(slot->buttonValue));
        memset(slot->axisValue, 0, sizeof(slot->axisValue));

        if (joy != NULL) {
            slot->connected = true;
            slot->jid = slotIdx;

            if (slot->description[0] == '\0') {
                const char* name = SDL_JoystickName(joy);
                if (name != NULL) {
                    strncpy(slot->description, name, sizeof(slot->description) - 1);
                    slot->description[sizeof(slot->description) - 1] = '\0';
                }
            }

            slot->guid[0] = '\0';

            GamepadMapping* map = &joystickMappings[slotIdx];

            if (map->valid) {
                for (int btn = 0; btn < GP_BUTTON_COUNT; btn++) {
                    if (map->button_map[btn] != -1) {
                        if (SDL_JoystickGetButton(joy, map->button_map[btn])) {
                            slot->buttonDown[btn] = true;
                            slot->buttonValue[btn] = 1.0f;
                        }
                    }
                    if (map->hat_map[btn] != -1) {
                        Uint8 hat = SDL_JoystickGetHat(joy, map->hat_map[btn]);
                        if (hat & map->hat_mask[btn]) {
                            slot->buttonDown[btn] = true;
                            slot->buttonValue[btn] = 1.0f;
                        }
                    }
                    if (map->axis_button_map[btn] != -1) {
                        Sint16 val = SDL_JoystickGetAxis(joy, map->axis_button_map[btn]);
                        float norm = val / 32767.0f;
                        if (map->axis_button_sign[btn] < 0) norm = -norm;
                        if (norm < 0.0f) norm = 0.0f;

                        if (norm > slot->buttonValue[btn]) {
                            slot->buttonValue[btn] = norm;
                        }
                        if (slot->buttonValue[btn] >= slot->triggerThreshold) {
                            slot->buttonDown[btn] = true;
                        }
                    }
                }

                for (int a = 0; a < GP_AXIS_COUNT; a++) {
                    if (map->axis_map[a] != -1) {
                        Sint16 val = SDL_JoystickGetAxis(joy, map->axis_map[a]);
                        slot->axisValue[a] = val / 32767.0f;
                    }
                }
            }

            for (int btn = 0; GP_BUTTON_COUNT > btn; btn++) {
                bool wasDown = slot->buttonDownPrev[btn];
                if (slot->buttonDown[btn] && !wasDown) slot->buttonPressed[btn] = true;
                if (!slot->buttonDown[btn] && wasDown) slot->buttonReleased[btn] = true;
            }
            g_runner->gamepads->connectedCount++;
        } else {
            slot->connected = false;
        }
    }

    SDL_Event e;
    while (SDL_PollEvent(&e)) {
        if (e.type == SDL_QUIT) return true;
    }
    return false;
}

void platformSleepUntil(uint64_t time) {
    int64_t remaining = time - nowNanos();
    if (remaining > 2000000)
        SDL_Delay((remaining - 1000000) / 1000000);

    while (nowNanos() < time) {
        // Spin-wait for the remaining sub-millisecond
        YIELD();
    }
}
