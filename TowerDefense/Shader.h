#pragma once
#include <glad/glad.h>
#include <string>

class Shader {
public:
    Shader(const char* vs, const char* fs);
    ~Shader();
    void use() const;
    GLint loc(const char* name) const;
    GLuint id() const { return program_; }
private:
    GLuint program_{ 0 };
    static GLuint compile(GLenum type, const char* src);
};
