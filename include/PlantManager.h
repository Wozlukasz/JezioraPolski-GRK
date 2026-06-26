#pragma once
#include <vector>
#include <string>
#include <glm/glm.hpp>

struct PlantChunk {
    unsigned int vao = 0, instVBO = 0;
    unsigned int flatVAO = 0; // VAO for flat model
    int instanceCount = 0;
    glm::vec3 center;
    std::vector<glm::vec3> instancePositions;
};

struct PlantVariant {
    unsigned int baseVBO = 0;
    int vertexCount = 0;
    std::vector<PlantChunk> chunks;
    
    // Flat LOD fields
    bool hasFlatLOD = false;
    unsigned int flatVBO = 0;
    int flatVertexCount = 0;
    unsigned int flatTexture = 0;
};

struct PlantSpecies {
    std::string name;
    unsigned int textureDiffuse;
    std::vector<PlantVariant> variants;
};

class PlantManager {
public:
    void init();
    void render(unsigned int shader, const glm::vec3& cameraPos, const glm::mat4& vpMatrix, bool isReflection = false);
    void renderShadow(unsigned int shadowShader, const glm::vec3& cameraPos, const glm::mat4& vpMatrix);

    // Zwraca nazwę gatunku rośliny, na którą patrzy kamera (ray-sphere casting po chunkach).
    // Jeśli żadna roślina nie jest w zasięgu maxDist, zwraca pusty string.
    std::string getPlantAtCrosshair(const glm::vec3& camPos, const glm::vec3& camFront, float maxDist = 8.0f) const;

private:
    std::vector<PlantSpecies> speciesList;
    void loadSpecies(const std::string& name, const std::string& maskPath, const std::string& tuftJsonPath, const std::string& texPath, const std::vector<std::string>& variantPaths, float scaleMult = 1.0f,
                     const std::vector<std::string>& flatModelPaths = {}, const std::vector<std::string>& flatTexPaths = {}, bool flipMainTex = false);
    void setupBaseVBO(const std::vector<class Vertex>& vertices, unsigned int& vbo);
    void setupChunkVAO(unsigned int baseVBO, const std::vector<glm::mat4>& matrices, unsigned int& vao, unsigned int& instVBO);
};
