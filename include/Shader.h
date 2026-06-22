#pragma once
#include <string>

unsigned int compileShader(unsigned int type, const std::string& source);
unsigned int createShaderProgram(const std::string& vertexSource, const std::string& fragmentSource);
unsigned int createShaderProgramFromFiles(const std::string& vertexPath, const std::string& fragmentPath);
