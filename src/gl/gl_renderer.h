#ifndef _BS_GL_RENDERER_H_
#define _BS_GL_RENDERER_H_

#include "common.h"
#include "gl_common.h"
#include "renderer.h"
#include "runner.h"
#if defined(__EMSCRIPTEN__) || defined(__ANDROID__) || defined(__SWITCH__)
#include <GLES3/gl3.h>
#elif PLATFORM_VITA
#include <vitaGL.h>
#define GL_BOOL 0x8B56
#else
#include <glad/glad.h>
#endif

typedef enum {
    BATCHTYPE_QUAD,
    BATCHTYPE_TRIANGLE
} BatchType;

// ===[ GLRenderer Struct ]===
typedef struct {
    char* name; // owned
    int32_t location;
    GLenum type;
    uint32_t samplerSlot;
} GLShaderUniform;

typedef struct {
    GLuint shaderId;
    bool compiled;
    uint32_t uniformCount;
    GLShaderUniform* uniforms;

    // cached uniforms
    GLShaderUniform* gmBaseTexture;
    GLShaderUniform* gmMatrices;
    GLShaderUniform* gmFogColour;
    GLShaderUniform* gmAlphaTestEnabled;
    GLShaderUniform* gmAlphaRefValue;
} GMLShader;

typedef struct GLModernRenderer {
    GLRenderer base; // Must be first field for struct embedding
    
    GMLShader* defaultShaderProgram;
    GMLShader* gmlShaders;
    uint32_t gmlShaderCount;

    bool fogEnable;
    uint32_t fogColor; // BGR

    GLuint vao, vertexBufferVao, vbo, ebo;
    // Memory limits (see GLRenderer_ensureTextureLoaded and dropUnusedSurfaces).
    int32_t* pageLastUsed;      // frame each original TXTR page was last drawn from
    uint32_t pageLastUsedCount;
    size_t pageBytesLoaded;
    size_t pageBudgetBytes;     // GL_TEXTURE_BUDGET_MB; 0 = no limit
    int32_t* surfaceLastUsed;   // frame each surface was last used, by ID
    uint32_t surfaceLastUsedCount;
#ifdef TARGET_RG35XX
#define STREAM_VBO_COUNT 64
    // Batches cycle through these instead of orphaning vbo (see flushBatch).
    GLuint streamVbo[STREAM_VBO_COUNT];
    GLsizeiptr streamVboSize[STREAM_VBO_COUNT];
    int streamVboNext;
#endif

    BatchType batchType;
    int32_t batchCount;
    GLuint currentTextureId;

    GLuint hostFramebuffer; // present target for the composited frame, where 0 == the window

    bool isGL3; // TRUE if running on OpenGL (ES) 3.x+
    bool isGLES;  // TRUE if running on OpenGL ES (GLES)

    // Cached default shader uniforms
    GLShaderUniform* uWorldViewProjection;
    GLShaderUniform* uFogColor;
    GLShaderUniform* uAlphaTestRef;
    GLShaderUniform* uAlphaTestEnabled;
    GLShaderUniform* uTexture;
} GLModernRenderer;

bool GLRenderer_ensureTextureLoaded(GLRenderer* gl, uint32_t pageId);
Renderer* GLRenderer_create(void);

#endif /* _BS_GL_RENDERER_H_ */
