#include "Shader.h"
#include <vector>
#include <cstring>
#include <iostream>

GLuint Shader::compile(GLenum type, const char* src) {
    GLuint s = glCreateShader(type);
    GLint len = (GLint)std::strlen(src);
    glShaderSource(s, 1, &src, &len);
    glCompileShader(s);
    GLint ok = 0; glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
    if (!ok) { GLint L = 0; glGetShaderiv(s, GL_INFO_LOG_LENGTH, &L); std::string log(L, '\0'); glGetShaderInfoLog(s, L, nullptr, (GLchar*)log.data()); std::cerr << log << '\n'; std::exit(1); }
    return s;
}

Shader::Shader(const char* vs, const char* fs) {
    GLuint v = compile(GL_VERTEX_SHADER, vs);
    GLuint f = compile(GL_FRAGMENT_SHADER, fs);
    program_ = glCreateProgram();
    glAttachShader(program_, v);
    glAttachShader(program_, f);
    glLinkProgram(program_);
    GLint ok = 0; glGetProgramiv(program_, GL_LINK_STATUS, &ok);
    if (!ok) { GLint L = 0; glGetProgramiv(program_, GL_INFO_LOG_LENGTH, &L); std::string log(L, '\0'); glGetProgramInfoLog(program_, L, nullptr, (GLchar*)log.data()); std::cerr << log << '\n'; std::exit(1); }
    glDeleteShader(v);
    glDeleteShader(f);
}

Shader::~Shader() {
    if (program_) glDeleteProgram(program_);
}

void Shader::use() const {
    glUseProgram(program_);
}

GLint Shader::loc(const char* name) const {
    return glGetUniformLocation(program_, name);
}
