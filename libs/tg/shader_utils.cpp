#include "shader_utils.hpp"

#include <stdio.h>
#include <assert.h>
#ifdef __EMSCRIPTEN__
    #include <GLES2/gl2.h>
#else
    #include <glad/glad.h>
#endif
#include <tl/basic.hpp>

namespace tg
{

char* checkCompileErrors(u32 shader, tl::Span<char> buffer)
{
    i32 ok;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &ok);
    if(!ok) {
        GLsizei outSize;
        glGetShaderInfoLog(shader, buffer.size(), &outSize, buffer.begin());
        return buffer.begin();
    }
    return nullptr;
}

char* checkLinkErrors(u32 prog, tl::Span<char> buffer)
{
    GLint success;
    glGetProgramiv(prog, GL_LINK_STATUS, &success);
    if(!success) {
        glGetProgramInfoLog(prog, buffer.size(), nullptr, buffer.begin());
        return buffer.begin();
    }
    return nullptr;
}

}
