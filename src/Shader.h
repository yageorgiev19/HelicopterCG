// Shader.h - компилиране на GLSL шейдъри и задаване на uniform променливи.
#pragma once

#include <glad/gl.h>
#include <glm/glm.hpp>
#include <string>
#include <unordered_map>

class Shader {
public:
    // Компилира vertex + fragment шейдър и ги свързва в програма.
    // При грешка извежда лога на компилатора и спира програмата.
    Shader(const char* vertexSrc, const char* fragmentSrc, const char* name);
    ~Shader();

    void use() const { glUseProgram(program); }

    void set(const char* n, int v)              { glUniform1i(loc(n), v); }
    void set(const char* n, float v)            { glUniform1f(loc(n), v); }
    void set(const char* n, const glm::vec2& v) { glUniform2fv(loc(n), 1, &v[0]); }
    void set(const char* n, const glm::vec3& v) { glUniform3fv(loc(n), 1, &v[0]); }
    void set(const char* n, const glm::vec4& v) { glUniform4fv(loc(n), 1, &v[0]); }
    void set(const char* n, const glm::mat3& m) { glUniformMatrix3fv(loc(n), 1, GL_FALSE, &m[0][0]); }
    void set(const char* n, const glm::mat4& m) { glUniformMatrix4fv(loc(n), 1, GL_FALSE, &m[0][0]); }

private:
    GLint loc(const char* name);   // кешира местоположенията на uniform-ите

    GLuint program = 0;
    std::string name;
    std::unordered_map<std::string, GLint> cache;
};
