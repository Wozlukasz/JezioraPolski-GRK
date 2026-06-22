#include "Model.h"
#include <iostream>
#include <fstream>
#include <sstream>

bool loadOBJ(const std::string& path, std::vector<Vertex>& out_vertices) {
    std::vector<glm::vec3> temp_positions;
    std::vector<glm::vec3> temp_normals;
    std::vector<glm::vec2> temp_texcoords;
    
    std::ifstream file(path);
    if (!file.is_open()) {
        std::cerr << "Failed to open OBJ file: " << path << std::endl;
        return false;
    }
    
    std::string line;
    while (std::getline(file, line)) {
        if (line.empty() || line[0] == '#') continue;
        
        std::istringstream ss(line);
        std::string type;
        ss >> type;
        
        if (type == "v") {
            float x, y, z;
            ss >> x >> y >> z;
            temp_positions.push_back(glm::vec3(x, y, z));
        } else if (type == "vn") {
            float x, y, z;
            ss >> x >> y >> z;
            temp_normals.push_back(glm::vec3(x, y, z));
        } else if (type == "vt") {
            float u, v;
            ss >> u >> v;
            temp_texcoords.push_back(glm::vec2(u, v));
        } else if (type == "f") {
            std::vector<std::string> faceTokens;
            std::string token;
            while (ss >> token) {
                faceTokens.push_back(token);
            }
            
            auto parseVertex = [&](const std::string& vertexStr) -> Vertex {
                int v_idx = 0, vt_idx = 0, vn_idx = 0;
                size_t first_slash = vertexStr.find('/');
                if (first_slash == std::string::npos) {
                    v_idx = std::stoi(vertexStr);
                } else {
                    v_idx = std::stoi(vertexStr.substr(0, first_slash));
                    size_t second_slash = vertexStr.find('/', first_slash + 1);
                    if (second_slash == std::string::npos) {
                        std::string vt_str = vertexStr.substr(first_slash + 1);
                        if (!vt_str.empty()) vt_idx = std::stoi(vt_str);
                    } else {
                        std::string vt_str = vertexStr.substr(first_slash + 1, second_slash - first_slash - 1);
                        if (!vt_str.empty()) vt_idx = std::stoi(vt_str);
                        std::string vn_str = vertexStr.substr(second_slash + 1);
                        if (!vn_str.empty()) vn_idx = std::stoi(vn_str);
                    }
                }
                
                Vertex v;
                if (v_idx > 0) {
                    v.position = temp_positions[v_idx - 1];
                } else if (v_idx < 0) {
                    v.position = temp_positions[temp_positions.size() + v_idx];
                } else {
                    v.position = glm::vec3(0.0f);
                }
                
                if (vt_idx > 0 && !temp_texcoords.empty()) {
                    v.texCoords = temp_texcoords[vt_idx - 1];
                } else if (vt_idx < 0 && !temp_texcoords.empty()) {
                    v.texCoords = temp_texcoords[temp_texcoords.size() + vt_idx];
                } else {
                    v.texCoords = glm::vec2(0.0f);
                }
                
                if (vn_idx > 0 && !temp_normals.empty()) {
                    v.normal = temp_normals[vn_idx - 1];
                } else if (vn_idx < 0 && !temp_normals.empty()) {
                    v.normal = temp_normals[temp_normals.size() + vn_idx];
                } else {
                    v.normal = glm::vec3(0.0f, 1.0f, 0.0f);
                }
                return v;
            };

            for (size_t i = 1; i < faceTokens.size() - 1; ++i) {
                out_vertices.push_back(parseVertex(faceTokens[0]));
                out_vertices.push_back(parseVertex(faceTokens[i]));
                out_vertices.push_back(parseVertex(faceTokens[i + 1]));
            }
        }
    }
    return true;
}
