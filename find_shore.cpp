#include <iostream>
#include <fstream>
#include <vector>
#include <sstream>
#include <cmath>

struct Vertex { float x, y, z; };
std::vector<Vertex> verts;

void loadOBJ(const std::string& path) {
    std::ifstream file(path);
    std::string line;
    while (std::getline(file, line)) {
        if (line.substr(0, 2) == "v ") {
            std::istringstream s(line.substr(2));
            Vertex v; s >> v.x >> v.y >> v.z;
            verts.push_back(v);
        }
    }
}

float getTerrainHeight(float x, float z) {
    for (size_t i = 0; i < verts.size(); i += 3) {
        if (i + 2 >= verts.size()) break;
        Vertex v0 = verts[i], v1 = verts[i+1], v2 = verts[i+2];
        float minX = std::min(v0.x, std::min(v1.x, v2.x));
        float maxX = std::max(v0.x, std::max(v1.x, v2.x));
        float minZ = std::min(v0.z, std::min(v1.z, v2.z));
        float maxZ = std::max(v0.z, std::max(v1.z, v2.z));
        if (x >= minX && x <= maxX && z >= minZ && z <= maxZ) {
            float det = (v1.z - v2.z) * (v0.x - v2.x) + (v2.x - v1.x) * (v0.z - v2.z);
            float l1 = ((v1.z - v2.z) * (x - v2.x) + (v2.x - v1.x) * (z - v2.z)) / det;
            float l2 = ((v2.z - v0.z) * (x - v2.x) + (v0.x - v2.x) * (z - v2.z)) / det;
            float l3 = 1.0f - l1 - l2;
            if (l1 >= 0 && l2 >= 0 && l3 >= 0) return l1 * v0.y + l2 * v1.y + l3 * v2.y;
        }
    }
    return -1000.0f;
}

int main() {
    loadOBJ("Assets/models/strzeszynek/teren-jezioro.obj");
    for (float r = 50; r < 400; r += 20) {
        for (float a = 0; a < 360; a += 10) {
            float rad = a * M_PI / 180.0f;
            float x = r * cos(rad);
            float z = r * sin(rad);
            float h = getTerrainHeight(x, z);
            if (h >= 64.0f && h <= 66.0f) {
                std::cout << "Shore: " << x << ", " << h << ", " << z << "\n";
                return 0;
            }
        }
    }
}
