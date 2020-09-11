#include <stdio.h>
#include <assert.h>
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <tl/fmt.hpp>
#include <tl/basic.hpp>
#include <tg/shader_utils.hpp>
#include <glm/glm.hpp>
#include "utils.hpp"

using glm::vec3;
using glm::vec4;

char g_scratch[10*1024];
GLFWwindow* window;

const int k_numSamples = 1000;
constexpr int k_numBounces = 2;

static const char* getGlErrorStr(GLenum e)
{
    switch(e) {
        case GL_NO_ERROR: return "GL_NO_ERROR";
        case GL_INVALID_ENUM: return "GL_INVALID_ENUM";
        case GL_INVALID_VALUE: return "GL_INVALID_VALUE";
        case GL_INVALID_OPERATION: return "GL_INVALID_OPERATION";
        case GL_INVALID_FRAMEBUFFER_OPERATION: return "GL_INVALID_FRAMEBUFFER_OPERATION";
        case GL_OUT_OF_MEMORY: return "GL_OUT_OF_MEMORY";
        case GL_STACK_UNDERFLOW: return "GL_STACK_UNDERFLOW";
        case GL_STACK_OVERFLOW: return "GL_STACK_OVERFLOW";
    }
    return "[UNKNOWN]";
}

static const float k_quadVerts[] = {
    -1, -1,  +1, -1,  +1, +1,
    -1, -1,  +1, +1,  -1, +1
};

struct SphereObj {
    vec4 pos_rad;
    vec4 emitColor_metallic;
    vec4 albedo_rough2;
    SphereObj() {}
    SphereObj(vec3 pos, float rad, vec3 emitColor, vec3 albedo,
            float metallic, float rough2)
        : pos_rad(pos, rad)
        , emitColor_metallic(emitColor, metallic)
        , albedo_rough2(albedo, rough2) {}
};

SphereObj sceneSpheres[] = {
    SphereObj(
        {0, 0, 0}, // pos
        2, // rad
        {0.0f, 0.0f, 0}, // emit
        {0.6, 0.9f, 0.6}, // albedo
        1, 0 // metallic, rough2
    ),
    SphereObj(
        {-4, 0, 0},
        2,
        {0, 0, 0},
        {1.f, 0.f, 0.f},
        0, 0
    ),
    SphereObj(
        {+4, 0, 0},
        2,
        {0, 0, 0},
        {0.f, 0.f, 1.f},
        0, 0
    ),
    SphereObj(
        {0, -1002, 0},
        1000,
        {0, 0, 0},
        {1.f, 1.f, 1.f},
        0, 0
    ),
    SphereObj(
        {0, 0, 0},
        1000,
        0.6f*vec3{1.0f, 1.0f, 1.2f},
        {0.0f, 0.0f, 0.0f},
        0, 0
    ),
};

struct {
    u32 prog;
    struct {
        i32 fovFactor;
        i32 viewMtx;
        i32 resolution;
        i32 sampleInd;
        i32 numSamples;
    } unifLocs;
} rayShad;

u32 postproProg;

u32 splatTexProg;
u32 quadVbo, quadVao;
u32 fbo;
u32 spheresSsbo;

struct Textures {
    u32 accum;
    u32 tonemapped;
    void init();
    void resize(int w, int h);
} textures;

void Textures::init()
{
    u32* tex = &accum;
    glGenTextures(2, tex);
    for(int i = 0; i < 2; i++) {
        glBindTexture(GL_TEXTURE_2D, tex[i]);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    }
}

void Textures::resize(int w, int h)
{
    u32* tex = &accum;
    for(int i = 0; i < 2; i++) {
        glBindTexture(GL_TEXTURE_2D, tex[i]);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB32F, w, h, 0, GL_RGB, GL_FLOAT, nullptr);
    }
}

template <i32 N>
static void uploadSrcs(u32 shad, const char* (&srcs)[N])
{
    i32 lens[N];
    for(i32 i = 0; i < N; i++) {
        lens[i] = strlen(srcs[i]);
        //printf("(%d)\n%s\n", lens[i], srcs[i]);
    }
    glShaderSource(shad, N, srcs, lens);
}

static const char* s_glslVersion = "#version 460\n";
static const char* s_glslUtilSrc = nullptr;

u32 makeShader(GLenum type, const char* fileName)
{
    const u32 shad = glCreateShader(type);
    const char* src = loadStr(fileName);
    defer(delete[] src);
    const char* srcs[] = { s_glslVersion, s_glslUtilSrc, src };
    uploadSrcs(shad, srcs);
    glCompileShader(shad);
    if(const char* errMsg = tg::checkCompileErrors(shad, g_scratch)) {
        tl::eprintln("Error compiling: ", fileName);
        tl::eprintln(errMsg);
        assert(false);
    }
    return shad;
}

u32 makeShaderProg(const char* vertFileName, const char* fragFileName)
{
    const u32 vertShad = makeShader(GL_VERTEX_SHADER, vertFileName);
    defer(glDeleteShader(vertShad));

    const u32 fragShad = makeShader(GL_FRAGMENT_SHADER, fragFileName);
    defer(glDeleteShader(fragShad));

    const u32 prog = glCreateProgram();
    glAttachShader(prog, vertShad);
    glAttachShader(prog, fragShad);
    glLinkProgram(prog);
    if(const char* errMsg = tg::checkLinkErrors(prog, g_scratch)) {
        tl::eprintln("Error linking: ", vertFileName, " + ", fragFileName);
        tl::eprintln(errMsg);
        assert(false);
    }
    return prog;
}

u32 makeShaderProg(u32 vertShad, const char* fragFileName)
{
    const u32 fragShad = makeShader(GL_FRAGMENT_SHADER, fragFileName);
    defer(glDeleteShader(fragShad));

    const u32 prog = glCreateProgram();
    glAttachShader(prog, vertShad);
    glAttachShader(prog, fragShad);
    glLinkProgram(prog);
    if(const char* errMsg = tg::checkLinkErrors(prog, g_scratch)) {
        tl::eprintln("Error linking: ", fragFileName);
        tl::eprintln(errMsg);
        assert(false);
    }
    return prog;
}

static void compileShaders()
{
    s_glslUtilSrc = loadStr("src/shaders/util.glsl");
    defer(delete[] s_glslUtilSrc);

    const u32 vertShad = makeShader(GL_VERTEX_SHADER, "src/shaders/screen_tc.glsl");

    // --- splat texture ---
    splatTexProg = makeShaderProg(vertShad, "src/shaders/splat_tex.glsl");

    // --- postpro ---
    postproProg = makeShaderProg(vertShad, "src/shaders/postpro.glsl");

    // --- main ---
    {
        const char* vertFileName = "src/shaders/main_vert.glsl";
        const u32 vertShad = makeShader(GL_VERTEX_SHADER, vertFileName);

        const char* fragFileName = "src/shaders/main_frag.glsl";
        const u32 fragShad = glCreateShader(GL_FRAGMENT_SHADER);
        const char* src = loadStr(fragFileName);
        defer(delete[] src);
        tl::toStringBuffer(g_scratch,
            "const int k_numBounces = ", k_numBounces, ";\n");
        const char* srcs[] = {s_glslVersion, s_glslUtilSrc, g_scratch, src};
        uploadSrcs(fragShad, srcs);
        glCompileShader(fragShad);
        if(const char* errMsg = tg::checkCompileErrors(fragShad, g_scratch)) {
            tl::eprintln("Error compiling: ", fragFileName);
            tl::eprintln(errMsg);
            assert(false);
        }

        rayShad.prog = glCreateProgram();
        glAttachShader(rayShad.prog, vertShad);
        glAttachShader(rayShad.prog, fragShad);
        glLinkProgram(rayShad.prog);
        if(const char* errMsg = tg::checkLinkErrors(rayShad.prog, g_scratch)) {
            tl::eprintln("Error linking: ", vertFileName, " + ", fragFileName);
            tl::eprintln(errMsg);
            assert(false);
        }

        rayShad.unifLocs.fovFactor =
            glGetUniformLocation(rayShad.prog, "u_fovFactor");
        assert(rayShad.unifLocs.fovFactor != -1);
        rayShad.unifLocs.viewMtx =
            glGetUniformLocation(rayShad.prog, "u_viewMtx");
        assert(rayShad.unifLocs.viewMtx != -1);
        rayShad.unifLocs.resolution =
            glGetUniformLocation(rayShad.prog, "u_resolution");
        assert(rayShad.unifLocs.resolution != -1);
        rayShad.unifLocs.sampleInd =
            glGetUniformLocation(rayShad.prog, "u_sampleInd");
        assert(rayShad.unifLocs.sampleInd != -1);
        rayShad.unifLocs.numSamples =
            glGetUniformLocation(rayShad.prog, "u_numSamples");
        assert(rayShad.unifLocs.numSamples != -1);
    }
}

static void glErrorCallback(const char *name, void *funcptr, int len_args, ...) {
    GLenum error_code;
    error_code = glad_glGetError();
    if (error_code != GL_NO_ERROR) {
        fprintf(stderr, "ERROR %s in %s\n", getGlErrorStr(error_code), name);
        assert(false);
    }
}

static void keyCallback(GLFWwindow* window, int key, int scancode, int action, int mods)
{
    if(key == GLFW_KEY_ESCAPE && action == GLFW_PRESS)
        glfwSetWindowShouldClose(window, 1);
}

static bool needToRedraw = true;
int sampleInd = 0;
static void windowResizeCallback(GLFWwindow* window, int w, int h)
{
    sampleInd = 0;
}

static void draw(int w, int h)
{
    if(sampleInd == k_numSamples)
        return;
    //printf("%d\n", sampleInd);
    textures.resize(w, h);
    float aspectRatio = float(w) / h;
    const float fovY = 1.2;
    const float fovFactorY = tan(0.5f * fovY);
    const float fovFactorX = aspectRatio * fovFactorY;
    const glm::mat4 viewMtx(
        1, 0, 0, 0,
        0, 1, 0, 0,
        0, 0, 1, 0,
        0, 0, 10, 0);

    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
        GL_TEXTURE_2D, textures.accum, 0);
    assert(glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE);
    const GLenum mainPassDrawBuffers[] = {GL_COLOR_ATTACHMENT0};
    glDrawBuffers(tl::size(mainPassDrawBuffers), mainPassDrawBuffers);
    //glClearColor(0, 0, 0, 0);
    //glClear(GL_COLOR_BUFFER_BIT);

    glEnable(GL_BLEND);
    //glBlendFunc(GL_ONE, GL_ONE); // add
    glBlendFunc(GL_ONE_MINUS_CONSTANT_ALPHA, GL_CONSTANT_ALPHA);

    glUseProgram(rayShad.prog);
    glUniform2f(rayShad.unifLocs.fovFactor, fovFactorX, fovFactorY);
    glUniformMatrix4fv(rayShad.unifLocs.viewMtx, 1, GL_FALSE, &viewMtx[0][0]);
    glUniform1i(rayShad.unifLocs.numSamples, k_numSamples);
    glUniform2i(rayShad.unifLocs.resolution, w, h);

    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, spheresSsbo);

    glBindVertexArray(quadVao);

    //for(int sampleInd = 0; sampleInd < k_numSamples; sampleInd++)
    {
        glBlendColor(0, 0, 0, float(sampleInd) / (sampleInd + 1));
        glUniform1i(rayShad.unifLocs.sampleInd, sampleInd);
        glDrawArrays(GL_TRIANGLES, 0, 6);
        sampleInd++;
    }
}

int main()
{
    glfwSetErrorCallback(+[](int error, const char* description) {
        fprintf(stderr, "Glfw Error %d: %s\n", error, description);
    });
    if (!glfwInit())
        return 1;

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 6);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_MAXIMIZED, GLFW_TRUE);

    window = glfwCreateWindow(1280, 720, "raygl", nullptr, nullptr);
    if (window == nullptr)
        return 1;

    glfwMakeContextCurrent(window);
    glfwSwapInterval(0); // Enable vsync

    glfwSetKeyCallback(window, keyCallback);
    glfwSetWindowSizeCallback(window, windowResizeCallback);

    if (gladLoadGL() == 0) {
        fprintf(stderr, "Failed to initialize OpenGL loader!\n");
        return 1;
    }
    glad_set_post_callback(glErrorCallback);

    compileShaders();

    glGenVertexArrays(1, &quadVao);
    glBindVertexArray(quadVao);
    glGenBuffers(1, &quadVbo);
    glBindBuffer(GL_ARRAY_BUFFER, quadVbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(k_quadVerts), k_quadVerts, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, nullptr);

    glGenBuffers(1, &spheresSsbo);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, spheresSsbo);
    glBufferData(GL_SHADER_STORAGE_BUFFER,
        sizeof(sceneSpheres), sceneSpheres, GL_STATIC_DRAW);

    glGenFramebuffers(1, &fbo);
    textures.init();

    int srcTexNdx = 0;

    while (!glfwWindowShouldClose(window))
    {
        glfwPollEvents();

        int w, h;
        glfwGetFramebufferSize(window, &w, &h);

        // draw scene
        glViewport(0, 0, w, h);
        glScissor(0, 0, w, h);
        //if(needToRedraw) {
            draw(w, h);
          //  needToRedraw = false;
        //}

        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glDisable(GL_BLEND);
        glDisable(GL_DEPTH_TEST);
        glClearColor(1,1,1,1);
        glClear(GL_COLOR_BUFFER_BIT);

        glUseProgram(postproProg);
        glUniform1i(0, 0);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, textures.accum);
        glBindVertexArray(quadVao);
        glDrawArrays(GL_TRIANGLES, 0, 6);

        glfwSwapBuffers(window);
    }
}