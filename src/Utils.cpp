#include "Utils.h"
#include <fstream>
#include <sstream>
#include <iostream>

std::string findAssetPath(const std::string& relativePath) {
    std::ifstream f(relativePath);
    if (f.good()) return relativePath;
    
    std::string upPath = "../" + relativePath;
    std::ifstream fUp(upPath);
    if (fUp.good()) return upPath;
    
    std::string upUpPath = "../../" + relativePath;
    std::ifstream fUpUp(upUpPath);
    if (fUpUp.good()) return upUpPath;

    return relativePath;
}

std::string loadFileString(const std::string& filePath) {
    std::ifstream file(filePath);
    if (!file.is_open()) {
        std::cerr << "Failed to open file: " << filePath << std::endl;
        return "";
    }
    std::stringstream buffer;
    buffer << file.rdbuf();
    return buffer.str();
}
