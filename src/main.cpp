#include <stdio.h>
#include <assert.h>
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <tl/fmt.hpp>
#include <tl/basic.hpp>
#include <tg/shader_utils.hpp>

char g_scratch[10*1024];
GLFWwindow* window;

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

struct {
    u32 prog;
    struct {
        i32 fov;
    } unifLocs;
} shader;

u32 quadVbo, quadVao;

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

static void compileShaders()
{
    static const char* glslVersion = "#version 460\n";

    const u32 vertShad = glCreateShader(GL_VERTEX_SHADER);
    defer(glDeleteShader(vertShad));
    const char* vertShadSrc = loadStr("src/shaders/vert.glsl");
    defer(delete[] vertShadSrc);
    const char* vertSrcs[] = { glslVersion, vertShadSrc };
    uploadSrcs(vertShad, vertSrcs);
    glCompileShader(vertShad);
    if(const char* errMsg = tg::checkCompileErrors(vertShad, g_scratch)) {
        tl::eprintln(errMsg);
        assert(false);
    }

    const u32 fragShad = glCreateShader(GL_FRAGMENT_SHADER);
    defer(glDeleteShader(fragShad));
    const char* fragShadSrc = loadStr("src/shaders/frag.glsl");
    defer(delete[] fragShadSrc);
    const char* fragSrcs[] = { glslVersion, fragShadSrc };
    uploadSrcs(fragShad, fragSrcs);
    glCompileShader(fragShad);
    if(const char* errMsg = tg::checkCompileErrors(fragShad, g_scratch)) {
        tl::eprintln(errMsg);
        assert(false);
    }

    shader.prog = glCreateProgram();
    glAttachShader(shader.prog, vertShad);
    glAttachShader(shader.prog, fragShad);
    glLinkProgram(shader.prog);
    if(const char* errMsg = tg::checkLinkErrors(shader.prog, g_scratch)) {
        tl::eprintln(errMsg);
        assert(false);
    }

    glUseProgram(shader.prog);

    shader.unifLocs.fov = glGetUniformLocation(shader.prog, "u_fov");
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

int main()
{
    glfwSetErrorCallback(+[](int error, const char* description) {
        fprintf(stderr, "Glfw Error %d: %s\n", error, description);
    });
    if (!glfwInit())
        return 1;

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_MAXIMIZED, GLFW_TRUE);

    window = glfwCreateWindow(1280, 720, "raygl", nullptr, nullptr);
    if (window == nullptr)
        return 1;

    glfwMakeContextCurrent(window);
    glfwSwapInterval(0); // Enable vsync

    glfwSetKeyCallback(window, keyCallback);

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

    while (!glfwWindowShouldClose(window))
    {
        glfwPollEvents();

        // draw scene
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        int w, h;
        glfwGetFramebufferSize(window, &w, &h);
        glViewport(0, 0, w, h);
        glScissor(0, 0, w, h);
        glClearColor(1,0,0,1);
        glClear(GL_COLOR_BUFFER_BIT);

        glUseProgram(shader.prog);
        glUniform2f(shader.unifLocs.fov, 1, 1);

        glBindVertexArray(quadVao);
        glDrawArrays(GL_TRIANGLES, 0, 6);

        glfwSwapBuffers(window);
    }
}