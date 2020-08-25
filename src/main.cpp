#include <stdio.h>
#include <assert.h>
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <tl/fmt.hpp>
#include <tl/basic.hpp>
#include <tg/shader_utils.hpp>
#include <glm/glm.hpp>

char g_scratch[10*1024];
GLFWwindow* window;

const u32 k_numSamples = 1;

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

char* loadStr(const char* fileName)
{
    FILE* f = fopen(fileName, "r");
    if(!f) {
        tl::eprintln("error loading: ", fileName);
        return nullptr;
    }
    fseek(f, 0, SEEK_END);
    const size_t len = ftell(f);
    fseek(f, 0, SEEK_SET);
    char* str = new char[len+1];
    fread(str, len, 1, f);
    str[len] = 0;
    fclose(f);
    return str;
}

struct CommonUnifLocs {
    i32 sampleInd;
    i32 numSamples;
};

struct {
    u32 prog;
    struct : CommonUnifLocs{
        i32 fovFactor;
        i32 viewMtx;
        i32 resolution;
    } unifLocs;
} camRaysShad;

struct {
    u32 prog;
    struct : CommonUnifLocs{
        i32 rayOri;
        i32 rayDir;
        i32 spherePos;
        i32 sphereRad;
        i32 emitColor;
        i32 albedo;
    } unifLocs;
} sphereShad;

struct {
    u32 prog;
    struct {
        i32 atten;
        i32 emitColor;
    } unifLocs;
} accumShad;

u32 splatTexProg;

u32 quadVbo, quadVao;

u32 fbo;

constexpr int numBounces = 4;
struct Textures {
    u32 ori[2];
    u32 dir[2];
    u32 atten;
    u32 emit;
    u32 accum;
    u32 depth;
    void init();
    void resize(int w, int h);
} textures;

void Textures::init()
{
    const int numColorTextures = sizeof(Textures) / 4 - 1;
    u32* allTextures = (u32*)this;

    glGenTextures(numColorTextures, allTextures);
    for(int i = 0; i < 4; i++) {
        glBindTexture(GL_TEXTURE_2D, allTextures[i]);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    }

    for(int i = 0; i < 2; i++) {
        glBindTexture(GL_TEXTURE_2D_ARRAY, (&atten)[i]);
        glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    }

    glBindTexture(GL_TEXTURE_2D, accum);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

    glGenRenderbuffers(1, &depth);
}

void Textures::resize(int w, int h)
{
    u32* allTextures = (u32*)this;

    for(int i = 0; i < 4; i++) {
        glBindTexture(GL_TEXTURE_2D, allTextures[i]);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB32F, w, h, 0, GL_RGB, GL_FLOAT, nullptr);
    }

    for(int i = 0; i < 2; i++) {
        glBindTexture(GL_TEXTURE_2D_ARRAY, (&atten)[i]);
        glTexStorage3D(GL_TEXTURE_2D_ARRAY, 1, GL_RGB32F, w, h, numBounces);
    }

    glBindTexture(GL_TEXTURE_2D, accum);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB32F, w, h, 0, GL_RGB, GL_FLOAT, nullptr);

    glBindRenderbuffer(GL_RENDERBUFFER, depth);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT, w, h);
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

static void getCommonUnifLocs(u32 prog, CommonUnifLocs& locs)
{
    locs.sampleInd = glGetUniformLocation(prog, "u_sampleInd");
    locs.numSamples = glGetUniformLocation(prog, "u_numSamples");
}

static void compileShaders()
{
    s_glslUtilSrc = loadStr("src/shaders/util.glsl");
    defer(delete[] s_glslUtilSrc);

    // --- cam_rays ---
    camRaysShad.prog = makeShaderProg(
        "src/shaders/cam_rays_vert.glsl",
        "src/shaders/cam_rays_frag.glsl");
    getCommonUnifLocs(camRaysShad.prog, camRaysShad.unifLocs);
    camRaysShad.unifLocs.fovFactor =
        glGetUniformLocation(camRaysShad.prog, "u_fovFactor");
    camRaysShad.unifLocs.viewMtx =
        glGetUniformLocation(camRaysShad.prog, "u_viewMtx");
    camRaysShad.unifLocs.resolution =
        glGetUniformLocation(camRaysShad.prog, "u_resolution");

    const u32 vertShad = makeShader(GL_VERTEX_SHADER, "src/shaders/screen_tc.glsl");

    // --- splat texture ---
    splatTexProg = makeShaderProg(vertShad, "src/shaders/splat_tex.glsl");

    // --- sphere ---
    sphereShad.prog = makeShaderProg(vertShad, "src/shaders/sphere.glsl");
    getCommonUnifLocs(sphereShad.prog, sphereShad.unifLocs);
    sphereShad.unifLocs.rayOri =
        glGetUniformLocation(sphereShad.prog, "u_rayOri");
    sphereShad.unifLocs.rayDir =
        glGetUniformLocation(sphereShad.prog, "u_rayDir");
    sphereShad.unifLocs.spherePos =
        glGetUniformLocation(sphereShad.prog, "u_spherePos");
    sphereShad.unifLocs.sphereRad =
        glGetUniformLocation(sphereShad.prog, "u_sphereRad");
    sphereShad.unifLocs.emitColor =
        glGetUniformLocation(sphereShad.prog, "u_emitColor");
    sphereShad.unifLocs.albedo =
        glGetUniformLocation(sphereShad.prog, "u_albedo");

    // --- accumulate ---
    {
        const char* fileName = "src/shaders/accum.glsl";
        const char* src = loadStr(fileName);
        defer(delete[] src);
        const u32 fragShad = glCreateShader(GL_FRAGMENT_SHADER);
        defer(glDeleteShader(fragShad));
        tl::toStringBuffer(g_scratch, "const int k_numBounces = ", numBounces, ";\n");
        const char* srcs[] = {s_glslVersion, s_glslUtilSrc, g_scratch, src};
        uploadSrcs(fragShad, srcs);
        glCompileShader(fragShad);
        if(const char* errMsg = tg::checkCompileErrors(fragShad, g_scratch)) {
            tl::eprintln("Error compiling: ", fileName);
            tl::eprintln(errMsg);
            assert(false);
        }

        accumShad.prog = glCreateProgram();
        glAttachShader(accumShad.prog, vertShad);
        glAttachShader(accumShad.prog, fragShad);
        glLinkProgram(accumShad.prog);
        if(const char* errMsg = tg::checkLinkErrors(accumShad.prog, g_scratch)) {
            tl::eprintln("Error linking: ", fileName);
            tl::eprintln(errMsg);
            assert(false);
        }
        accumShad.unifLocs.atten =
            glGetUniformLocation(accumShad.prog, "u_atten");
        accumShad.unifLocs.emitColor =
            glGetUniformLocation(accumShad.prog, "u_emitColor");
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
static void windowResizeCallback(GLFWwindow* window, int w, int h)
{
    needToRedraw = true;
}

static void draw(int w, int h)
{
    textures.resize(w, h);
    float aspectRatio = float(w) / h;
    const float fovY = 1.2;
    const float fovFactorY = tan(fovY);
    const float fovFactorX = aspectRatio * fovFactorY;

    glBindFramebuffer(GL_FRAMEBUFFER, fbo);

    // --- initialization pass - draw camera rays ---
    glDisable(GL_DEPTH_TEST);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
        GL_TEXTURE_2D, textures.ori[0], 0);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1,
        GL_TEXTURE_2D, textures.dir[0], 0);
    assert(glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE);
    const GLenum initPassDrawBuffers[] = {GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1};
    glDrawBuffers(2, initPassDrawBuffers);
    glUseProgram(camRaysShad.prog);
    glUniform2f(camRaysShad.unifLocs.fovFactor, fovFactorX, fovFactorY);
    glm::mat4 viewMtx =
        glm::mat4(1, 0, 0, 0,
                  0, 1, 0, 0,
                  0, 0, 1, 0,
                  0, 0, 5, 0);
    glUniformMatrix4fv(camRaysShad.unifLocs.viewMtx, 1, GL_FALSE, &viewMtx[0][0]);
    glUniform1ui(camRaysShad.unifLocs.sampleInd, 0);
    glUniform1ui(camRaysShad.unifLocs.numSamples, k_numSamples);
    glUniform2ui(camRaysShad.unifLocs.resolution, w, h);
    glBindVertexArray(quadVao);
    glDrawArrays(GL_TRIANGLES, 0, 6);

    // --- bounce passes ---
    //glEnable(GL_DEPTH_TEST);
    const GLenum bouncePassDrawBuffers[] = {GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1, GL_COLOR_ATTACHMENT2, GL_COLOR_ATTACHMENT3};
    glDrawBuffers(4, bouncePassDrawBuffers);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT,
                GL_RENDERBUFFER, textures.depth);
    int rayTexTarget = 1;
    for(int bounce = 0; bounce < numBounces; bounce++)
    {
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
            GL_TEXTURE_2D, textures.ori[rayTexTarget], 0);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1,
            GL_TEXTURE_2D, textures.dir[rayTexTarget], 0);
        glFramebufferTextureLayer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT2,
            textures.atten, 0, bounce);
        glFramebufferTextureLayer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT3,
            textures.emit, 0, bounce);
        
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        glUseProgram(sphereShad.prog);
        const int rayTexSrc = rayTexTarget ^ 1;
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, textures.ori[rayTexSrc]);
        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, textures.dir[rayTexSrc]);
        glUniform1ui(sphereShad.unifLocs.sampleInd, 0);
        glUniform1ui(sphereShad.unifLocs.numSamples, k_numSamples);
        glUniform1i(sphereShad.unifLocs.rayOri, 0);
        glUniform1i(sphereShad.unifLocs.rayDir, 1);
        glUniform3f(sphereShad.unifLocs.spherePos, 0, 0, 0);
        glUniform1f(sphereShad.unifLocs.sphereRad, 2);
        glUniform3f(sphereShad.unifLocs.emitColor, 0.5, 0.5, 0.0);
        glUniform3f(sphereShad.unifLocs.albedo, 0, 0.5, 0);

        glDrawArrays(GL_TRIANGLES, 0, 6);

        rayTexTarget ^= 1;
    }

    // --- accum bounces pass ---
    glDisable(GL_DEPTH_TEST);

    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
    GL_TEXTURE_2D, textures.accum, 0);
    const GLenum accumDrawBuffers[] = {GL_COLOR_ATTACHMENT0};
    glDrawBuffers(1, accumDrawBuffers);

    glUseProgram(accumShad.prog);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D_ARRAY, textures.atten);
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D_ARRAY, textures.emit);
    glUniform1i(accumShad.unifLocs.atten, 0);
    glUniform1i(accumShad.unifLocs.emitColor, 1);

    glDrawArrays(GL_TRIANGLES, 0, 6);
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
        if(needToRedraw) {
            draw(w, h);
            needToRedraw = false;
        }

        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glClearColor(1,1,1,1);
        glClear(GL_COLOR_BUFFER_BIT);

        glUseProgram(splatTexProg);
        glUniform1i(0, 0);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, textures.accum);
        glBindVertexArray(quadVao);
        glDrawArrays(GL_TRIANGLES, 0, 6);

        glfwSwapBuffers(window);
    }
}