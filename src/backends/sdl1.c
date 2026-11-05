#include <ctype.h>
#include "string_compat.h"
#include "stdio_compat.h"
#include <ctype.h>
#include <stdlib.h>

#include <SDL/SDL.h>

#include "common.h"
#include "input_recording.h"
#include "platformdefs.h"
#include "gettime.h"

#ifndef SDL_BUTTON_WHEELUP
#define SDL_BUTTON_WHEELUP 4
#endif
#ifndef SDL_BUTTON_WHEELDOWN
#define SDL_BUTTON_WHEELDOWN 5
#endif
#include "runner_mouse.h"

static Runner *g_runner;
static int32_t fbWidth, fbHeight;
static SDL_Surface* scr;
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

#ifdef TARGET_RG35XX
// By default the game sees a 320x240 window, and platformSwapBuffers doubles it to the 640x480 screen.
// RG35XX_FULL_RES gives it the whole screen instead.
static int rgGameW = 320;
static int rgGameH = 240;
#define RG35XX_SCREEN_W 640
#define RG35XX_SCREEN_H 480
#define RG35XX_BTN_START 8
#define RG35XX_BTN_MENU 9
#define RG35XX_BTN_VOLUME_UP 10
#define RG35XX_BTN_VOLUME_DOWN 11
#define RG35XX_VOLUME_STEPS 10
#define RG35XX_VOLUME_FILE "volume.txt"
#define RG35XX_AXIS_L2 2
#define RG35XX_AXIS_R2 5

typedef enum { RG_BUTTON, RG_HAT, RG_AXIS } RgInputKind;

typedef struct {
    RgInputKind kind;
    int index; // button number, hat mask or axis number
    int32_t gmlKey; // AM2R's default key for this control, as in its config.ini
    bool down;
} RgInputBinding;

static RgInputBinding rgBindings[] = {
    { RG_HAT, SDL_HAT_UP, VK_UP, false },
    { RG_HAT, SDL_HAT_DOWN, VK_DOWN, false },
    { RG_HAT, SDL_HAT_LEFT, VK_LEFT, false },
    { RG_HAT, SDL_HAT_RIGHT, VK_RIGHT, false },
    { RG_BUTTON, 0, 'Z', false },         // A: jump, menu OK
    { RG_BUTTON, 1, 'X', false },         // B: fire, menu cancel
    { RG_BUTTON, 3, 'X', false },         // Y: fire
    { RG_BUTTON, 2, VK_CONTROL, false },  // X: morph
    { RG_BUTTON, 5, VK_SHIFT, false },    // L1: hold to aim diagonally, with up or down
    { RG_BUTTON, 6, 'A', false },         // R1: hold to arm missiles
    { RG_AXIS, RG35XX_AXIS_L2, 'C', false },         // L2: second aim button, unused in one-button aiming
    { RG_AXIS, RG35XX_AXIS_R2, VK_SPACE, false },    // R2: aim lock
    { RG_BUTTON, 7, 'S', false },         // Select: switch weapon
    { RG_BUTTON, RG35XX_BTN_START, VK_ENTER, false },
    { RG_BUTTON, RG35XX_BTN_MENU, VK_ESCAPE, false },
};
#endif

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
                const char* jname = SDL_JoystickName(i);
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
    char windowTitle[256];
    snprintf(windowTitle, sizeof(windowTitle), "Butterscotch - %s", title);
    SDL_WM_SetCaption(windowTitle, NULL);
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
#ifdef TARGET_RG35XX
    (void) width; (void) height;
    return;
#endif
    if (width <= 0 || height <= 0) return;
    fbWidth = width;
    fbHeight = height;
    scr = SDL_SetVideoMode(width, height, 0, (gfx == SOFTWARE ? 0 : SDL_OPENGL) | SDL_RESIZABLE);
}

void platformGetMousePos(double *xPos, double *yPos) {
    if (!xPos || !yPos) return;
    int mx = 0, my = 0;
    SDL_GetMouseState(&mx, &my);

    *xPos = (double)mx;
    *yPos = (double)my;
}

static bool platformGetWindowFocus(void) {
    return SDL_GetAppState() & SDL_APPINPUTFOCUS;
}

bool platformInit(int32_t reqW, int32_t reqH, const char *title, bool headless) {
    if (headless && gfx != SOFTWARE) {
        logError("Headless mode on SDL 1.2 requires the software renderer!\n");
        return false;
    }

    // Init SDL
    if (SDL_Init(SDL_INIT_VIDEO|SDL_INIT_TIMER|SDL_INIT_JOYSTICK)) {
        logError("Failed to initialize SDL\n");
        return false;
    }

    SDL_JoystickEventState(SDL_IGNORE);
    int numJoysticks = SDL_NumJoysticks();
    for (int i = 0; i < MAX_GAMEPADS; i++) {
        memset(&joystickMappings[i], 0, sizeof(GamepadMapping));
        joystickMappings[i].valid = false;

        if (i < numJoysticks) {
            openJoysticks[i] = SDL_JoystickOpen(i);
        } else {
            openJoysticks[i] = NULL;
        }
    }
    loadGamepadMappings();

#ifdef TARGET_RG35XX
    if (getenv("RG35XX_FULL_RES") != NULL) {
        rgGameW = RG35XX_SCREEN_W;
        rgGameH = RG35XX_SCREEN_H;
    }
    reqW = rgGameW;
    reqH = rgGameH;
#endif
    fbWidth = reqW;
    fbHeight = reqH;
    if(!headless) {
#ifdef TARGET_RG35XX
#ifndef SDL_TRIPLEBUF
#define SDL_TRIPLEBUF 0x40000100 // GarlicOS's SDL accepts this, but it isn't in the 1.2 headers
#endif
        // Test hook: with double buffering, missing one screen refresh drops straight to 30 fps.
        scr = SDL_SetVideoMode(RG35XX_SCREEN_W, RG35XX_SCREEN_H, 16,
                               SDL_HWSURFACE | (getenv("AM2R_TRIPLEBUF") != NULL ? SDL_TRIPLEBUF : SDL_DOUBLEBUF));
        if (!scr || scr->format->BytesPerPixel != 2) {
            logError("Fatal: Could not set the 16-bit video mode: %s\n", SDL_GetError());
            return false;
        }
        SDL_ShowCursor(SDL_DISABLE);
#else
        scr = SDL_SetVideoMode(fbWidth, fbHeight, 0, (gfx == SOFTWARE ? 0 : SDL_OPENGL) | SDL_RESIZABLE);
        if (!scr && gfx == SOFTWARE) {
            SDL_Rect** modes = SDL_ListModes(NULL, SDL_FULLSCREEN);
            if (modes && modes != (SDL_Rect**) -1 && modes[0]) {
                logWarn("%dx%d unavailable, falling back to %dx%d: %s\n",
                        reqW, reqH, modes[0]->w, modes[0]->h, SDL_GetError());
                scr = SDL_SetVideoMode(modes[0]->w, modes[0]->h, 0, 0);
                fbWidth = modes[0]->w;
                fbHeight = modes[0]->h;
            }
        }
        if (!scr) {
            logError("Fatal: Could not set any video mode: %s\n", SDL_GetError());
            return false;
        }
#endif
    }

    SDL_WM_SetCaption(title, NULL);

    SDL_EnableKeyRepeat(0, 0);

    return true;
}

void platformExit(void) {
    for (int i = 0; i < MAX_GAMEPADS; i++) {
        if (openJoysticks[i]) {
            SDL_JoystickClose(openJoysticks[i]);
            openJoysticks[i] = NULL;
        }
    }
    SDL_Quit();
}

static void platformSetCursor(int32_t cursorType) {
    // SDL1.2 only supports showing/hiding
    SDL_ShowCursor(cursorType == GML_CR_NONE ? SDL_DISABLE : SDL_ENABLE);
}

void platformInitFunctions(Runner *runner) {
    g_runner = runner;
    runner->windowHasFocus = platformGetWindowFocus;
    runner->setCursor = platformSetCursor;
    runner->currentCursor = GML_CR_DEFAULT;
}

#ifdef ENABLE_SW_RENDERER

#ifdef TARGET_RG35XX

static const uint32_t* nextFbPixels = NULL;

void platformSetNextFramebuffer(uint32_t* framebuffer, int width, int height) {
    (void) width; (void) height;
    nextFbPixels = framebuffer;
}

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

static void rg35xxPresent(void) {
    if (!nextFbPixels) return;

    const SDL_PixelFormat* f = scr->format;
    // One row at a time, and doubled rows written twice: reading back from the framebuffer is slow.
    static uint16_t line[RG35XX_SCREEN_W];
    const bool doubled = rgGameW != RG35XX_SCREEN_W;
    const size_t lineSize = sizeof(line);
    const uint32_t* src = nextFbPixels;

    if (SDL_MUSTLOCK(scr)) SDL_LockSurface(scr);
    uint8_t* dst = (uint8_t*) scr->pixels;
    for (int y = 0; y < rgGameH; y++) {
        for (int x = 0; x < rgGameW; x++) {
            uint32_t c = src[x];
            uint16_t p = (uint16_t) ((((c >> 16) & 0xff) >> f->Rloss) << f->Rshift
                                   | (((c >> 8) & 0xff) >> f->Gloss) << f->Gshift
                                   | ((c & 0xff) >> f->Bloss) << f->Bshift);
            if (doubled) {
                line[x * 2] = p;
                line[x * 2 + 1] = p;
            } else {
                line[x] = p;
            }
        }
        memcpy(dst, line, lineSize);
        dst += scr->pitch;
        if (doubled) {
            memcpy(dst, line, lineSize);
            dst += scr->pitch;
        }
        src += rgGameW;
    }
    if (SDL_MUSTLOCK(scr)) SDL_UnlockSurface(scr);
    SDL_Flip(scr);
}

#else

static SDL_Surface* nextFb = NULL;

void platformSetNextFramebuffer(uint32_t* framebuffer, int width, int height) {
    if (nextFb) {
        SDL_FreeSurface(nextFb);
        nextFb = NULL;
    }

    nextFb = SDL_CreateRGBSurfaceFrom(
        framebuffer,
        width,
        height,
        32,
        width * 4,
        0x00ff0000, // Rmask
        0x0000ff00, // Gmask
        0x000000ff, // Bmask
        0x00000000  // Amask
    );
}

#endif

#endif

void platformSwapBuffers(void) {
#ifdef ENABLE_SW_RENDERER
    if(gfx == SOFTWARE) {
        if (!scr)
            return;
#ifdef TARGET_RG35XX
        rg35xxPresent();
#else
        SDL_BlitSurface(nextFb, NULL, scr, NULL);
        SDL_Flip(scr);
#endif
    }
#endif
#if defined(ENABLE_LEGACY_GL) || defined(ENABLE_MODERN_GL)
    if (gfx == LEGACY_GL || gfx == MODERN_GL)
        SDL_GL_SwapBuffers();
#endif
}

#if defined(ENABLE_MODERN_GL) || defined(ENABLE_LEGACY_GL)

void *platformGetProcAddress(const char *name) {
    return SDL_GL_GetProcAddress(name);
}

#endif

static int32_t SDLKeyToGml(int sdlkey) {
    // miyoomini controls for undertale hacky wacky
    switch (sdlkey) {
        case SDLK_SPACE: return toupper('z');
        case SDLK_LCTRL: return toupper('x');
        case SDLK_LSHIFT: return toupper('c');
    }
    // Letters and numbers are the same as GML
    if (sdlkey >= 'a' && sdlkey <= 'z') return toupper(sdlkey);
    if (sdlkey >= '0' && sdlkey <= '9') return sdlkey;
    // Special keys need mapping
    switch (sdlkey) {
        case SDLK_ESCAPE:    return VK_ESCAPE;
        case SDLK_RETURN:    return VK_ENTER;
        case SDLK_TAB:       return VK_TAB;
        case SDLK_BACKSPACE: return VK_BACKSPACE;
        case SDLK_SPACE:     return VK_SPACE;
        case SDLK_LSHIFT:
        case SDLK_RSHIFT:    return VK_SHIFT;
        case SDLK_LCTRL:
        case SDLK_RCTRL:     return VK_CONTROL;
        case SDLK_LALT:
        case SDLK_RALT:      return VK_ALT;
case SDLK_KP8:        case SDLK_UP:        return VK_UP;
case SDLK_KP5:case SDLK_KP2:        case SDLK_DOWN:      return VK_DOWN;
case SDLK_KP4:        case SDLK_LEFT:      return VK_LEFT;
case SDLK_KP6:        case SDLK_RIGHT:     return VK_RIGHT;
        case SDLK_F1:        return VK_F1;
        case SDLK_F2:        return VK_F2;
        case SDLK_F3:        return VK_F3;
        case SDLK_F4:        return VK_F4;
        case SDLK_F5:        return VK_F5;
        case SDLK_F6:        return VK_F6;
        case SDLK_F7:        return VK_F7;
        case SDLK_F8:        return VK_F8;
        case SDLK_F9:        return VK_F9;
        case SDLK_F10:       return VK_F10;
        case SDLK_F11:       return VK_F11;
        case SDLK_F12:       return VK_F12;
        case SDLK_INSERT:    return VK_INSERT;
        case SDLK_DELETE:    return VK_DELETE;
        case SDLK_HOME:      return VK_HOME;
        case SDLK_END:       return VK_END;
        case SDLK_PAGEUP:    return VK_PAGEUP;
        case SDLK_PAGEDOWN:  return VK_PAGEDOWN;
        default:             return -1; // Unknown
    }
}

static void platformResetJoysticks(void) {
    char oldNames[MAX_GAMEPADS][256] = {0};
    for (int i = 0; i < MAX_GAMEPADS; i++) {
        if (openJoysticks[i]) {
            const char* name = SDL_JoystickName(i);
            if (name) {
                strncpy(oldNames[i], name, sizeof(oldNames[i]) - 1);
            }
            SDL_JoystickClose(openJoysticks[i]);
            openJoysticks[i] = NULL;
        }
    }

    SDL_QuitSubSystem(SDL_INIT_JOYSTICK);
    SDL_InitSubSystem(SDL_INIT_JOYSTICK);
    SDL_JoystickEventState(SDL_IGNORE);

    int numJoysticks = SDL_NumJoysticks();
    bool needsRemap = false;
    for (int i = 0; i < numJoysticks && i < MAX_GAMEPADS; i++) {
        openJoysticks[i] = SDL_JoystickOpen(i);
        const char* newName = SDL_JoystickName(i);
        if (!newName) newName = "";
        if (strcmp(oldNames[i], newName) != 0) {
            joystickMappings[i].valid = false;
            needsRemap = true;
        }
    }

    for (int i = numJoysticks; i < MAX_GAMEPADS; i++) {
        joystickMappings[i].valid = false;
    }

    if (needsRemap) {
        loadGamepadMappings();
    }
}

static int32_t SDLMouseButtonToGml(int sdlButton) {
    switch (sdlButton) {
        case SDL_BUTTON_LEFT: return GML_MB_LEFT;
        case SDL_BUTTON_RIGHT: return GML_MB_RIGHT;
        case SDL_BUTTON_MIDDLE: return GML_MB_MIDDLE;
        default: return -1;
    }
}

#ifdef TARGET_RG35XX
static int rgVolumeLevel = RG35XX_VOLUME_STEPS;

// The volume keys set the master gain, on top of the game's own music and sound settings.
static void rg35xxApplyVolume(void) {
    if (g_runner->audioSystem == NULL) return;
    // Squared, because ears hear loudness roughly logarithmically.
    float gain = (float) (rgVolumeLevel * rgVolumeLevel) / (RG35XX_VOLUME_STEPS * RG35XX_VOLUME_STEPS);
    g_runner->audioSystem->vtable->setMasterGain(g_runner->audioSystem, gain);
}

static void rg35xxLoadVolume(void) {
    FILE* f = fopen(RG35XX_VOLUME_FILE, "r");
    if (f == NULL) return;
    int level;
    if (fscanf(f, "%d", &level) == 1 && level >= 0 && level <= RG35XX_VOLUME_STEPS) rgVolumeLevel = level;
    fclose(f);
}

static void rg35xxStepVolume(int delta) {
    int level = rgVolumeLevel + delta;
    if (level < 0 || level > RG35XX_VOLUME_STEPS) return;
    rgVolumeLevel = level;
    rg35xxApplyVolume();
    FILE* f = fopen(RG35XX_VOLUME_FILE, "w");
    if (f == NULL) return;
    fprintf(f, "%d\n", level);
    fclose(f);
}

// Feeds the buttons in as AM2R's default keys. Returns true on Menu + Start, which quits.
static bool rg35xxPollInput(void) {
    SDL_Joystick* joy = openJoysticks[0];
    if (joy == NULL) return false;

    static bool volumeLoaded = false;
    if (!volumeLoaded && g_runner->audioSystem != NULL) {
        volumeLoaded = true;
        rg35xxLoadVolume();
        rg35xxApplyVolume();
    }

    static bool volumeUpDown = false;
    static bool volumeDownDown = false;
    bool volumeUp = SDL_JoystickGetButton(joy, RG35XX_BTN_VOLUME_UP) != 0;
    bool volumeDown = SDL_JoystickGetButton(joy, RG35XX_BTN_VOLUME_DOWN) != 0;
    if (volumeUp && !volumeUpDown) rg35xxStepVolume(1);
    if (volumeDown && !volumeDownDown) rg35xxStepVolume(-1);
    volumeUpDown = volumeUp;
    volumeDownDown = volumeDown;

    if (SDL_JoystickGetButton(joy, RG35XX_BTN_MENU) && SDL_JoystickGetButton(joy, RG35XX_BTN_START))
        return true;

    Uint8 hat = SDL_JoystickGetHat(joy, 0);
    for (size_t i = 0; i < sizeof(rgBindings) / sizeof(rgBindings[0]); i++) {
        RgInputBinding* b = &rgBindings[i];
        bool down = false;
        switch (b->kind) {
            case RG_BUTTON: down = SDL_JoystickGetButton(joy, b->index) != 0; break;
            case RG_HAT: down = (hat & b->index) != 0; break;
            case RG_AXIS: down = SDL_JoystickGetAxis(joy, b->index) > 0; break;
        }
        if (down == b->down) continue;
        b->down = down;
        if (down) RunnerKeyboard_onKeyDown(g_runner->keyboard, b->gmlKey);
        else RunnerKeyboard_onKeyUp(g_runner->keyboard, b->gmlKey);
    }
    return false;
}
#endif

bool platformHandleEvents(void) {
    SDL_JoystickUpdate();
#ifdef TARGET_RG35XX
    if (rg35xxPollInput()) return true;
#endif

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

            // SDL_JoystickName reopens the device on Linux, which can block for tens of ms.
            if (slot->description[0] == '\0') {
                const char* name = SDL_JoystickName(slotIdx);
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
        switch(e.type) {
            case SDL_ACTIVEEVENT:
                if ((e.active.state & SDL_APPINPUTFOCUS) && e.active.gain) {
                    platformResetJoysticks();
                }
                break;
            case SDL_KEYDOWN:
                // SDL1.2 needs to manually intercept Alt+F4 to exit properly
                if (e.key.keysym.sym == SDLK_F4 && (e.key.keysym.mod & KMOD_ALT)) {
                    return true;
                }
                // During playback, suppress real keyboard input
                if (InputRecording_isPlaybackActive(globalInputRecording)) break;
                RunnerKeyboard_onKeyDown(g_runner->keyboard, SDLKeyToGml(e.key.keysym.sym));
                if (e.key.keysym.unicode != 0)
                    RunnerKeyboard_onCharacter(g_runner->keyboard, e.key.keysym.unicode);
                break;
            case SDL_KEYUP:
                // During playback, suppress real keyboard input
                if (InputRecording_isPlaybackActive(globalInputRecording)) break;
                RunnerKeyboard_onKeyUp(g_runner->keyboard, SDLKeyToGml(e.key.keysym.sym));
                break;
            case SDL_MOUSEBUTTONDOWN:
                if (InputRecording_isPlaybackActive(globalInputRecording)) break;
                if (e.button.button == SDL_BUTTON_WHEELUP) {
                    RunnerMouse_onWheel(g_runner->mouse, 1.0);
                } else if (e.button.button == SDL_BUTTON_WHEELDOWN) {
                    RunnerMouse_onWheel(g_runner->mouse, -1.0);
                } else {
                    int32_t gmlBtn = SDLMouseButtonToGml(e.button.button);
                    if (gmlBtn >= 0) RunnerMouse_onButtonDown(g_runner->mouse, gmlBtn);
                }
                break;
            case SDL_MOUSEBUTTONUP:
                if (InputRecording_isPlaybackActive(globalInputRecording)) break;
                if (e.button.button != SDL_BUTTON_WHEELUP && e.button.button != SDL_BUTTON_WHEELDOWN) {
                    int32_t gmlBtn = SDLMouseButtonToGml(e.button.button);
                    if (gmlBtn >= 0) RunnerMouse_onButtonUp(g_runner->mouse, gmlBtn);
                }
                break;
            case SDL_VIDEORESIZE:
                fbWidth = e.resize.w;
                fbHeight = e.resize.h;
                scr = SDL_SetVideoMode(fbWidth, fbHeight, 0, (gfx == SOFTWARE ? 0 : SDL_OPENGL) | SDL_RESIZABLE);
                break;
            case SDL_QUIT:
                return true;
        }
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
