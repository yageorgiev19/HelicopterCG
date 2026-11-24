// Shader.cpp
#include "Shader.h"

#include <cstdio>
#include <cstdlib>

static GLuint compile(GLenum type, const char* src, const char* name)
{
    GLuint s = glCreateShader(type);
    glShaderSource(s, 1, &src, nullptr);
    glCompileShader(s);
    GLint ok = 0;
    glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char log[2048];
        glGetShaderInfoLog(s, sizeof(log), nullptr, log);
        std::fprintf(stderr, "Грешка в шейдър %s (%s):\n%s\n", name,
                     type == GL_VERTEX_SHADER ? "vertex" : "fragment", log);
        std::exit(EXIT_FAILURE);
    }
    return s;
}

Shader::Shader(const char* vertexSrc, const char* fragmentSrc, const char* n) : name(n)
{
    GLuint vs = compile(GL_VERTEX_SHADER, vertexSrc, n);
    GLuint fs = compile(GL_FRAGMENT_SHADER, fragmentSrc, n);
    program = glCreateProgram();
    glAttachShader(program, vs);
    glAttachShader(program, fs);
    glLinkProgram(program);
    GLint ok = 0;
    glGetProgramiv(program, GL_LINK_STATUS, &ok);
    if (!ok) {
        char log[2048];
        glGetProgramInfoLog(program, sizeof(log), nullptr, log);
        std::fprintf(stderr, "Грешка при свързване на %s:\n%s\n", n, log);
        std::exit(EXIT_FAILURE);
    }
    // След свързването отделните обекти вече не са нужни.
    glDeleteShader(vs);
    glDeleteShader(fs);
}

Shader::~Shader() { glDeleteProgram(program); }

GLint Shader::loc(const char* n)
{
    auto it = cache.find(n);
    if (it != cache.end()) return it->second;
    GLint l = glGetUniformLocation(program, n);   // -1, ако не се използва
    cache.emplace(n, l);
    return l;
}
