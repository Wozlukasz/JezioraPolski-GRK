#pragma once
#include <glm/glm.hpp>
#include <vector>
#include <string>

struct Vertex {
    glm::vec3 position;
    glm::vec3 normal;
    glm::vec2 texCoords;
};

bool loadOBJ(const std::string& path, std::vector<Vertex>& out_vertices);
