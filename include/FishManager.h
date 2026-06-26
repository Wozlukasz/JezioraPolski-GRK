#pragma once
#include <vector>
#include <string>
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include "ParallelTransport.h"

struct Fish {
    int speciesIndex;      // 0 = płoć, 1 = ukleja
    std::vector<glm::vec3> controlPoints;  // Catmull-Rom control points
    std::vector<PTFrame> frames;           // Precomputed PTF along sampled spline
    float t;               // Current parameter [0, 1]
    float speed;           // Movement speed (parameter units per second)
    float phase;           // Phase offset for schooling
    glm::vec3 baseCenter;  // Center of the swim path (for feeding attraction)
    glm::vec3 localOffset; // Fixed offset relative to the school's central path
    float panicLevel = 0.0f; // Smooth [0, 1] panic level
    glm::vec3 scatterDir;  // Unique random direction to scatter when panicked
    
    // --- Computed per-frame in update() ---
    glm::vec3 currentPos;
    glm::vec3 currentTan;
    glm::vec3 currentNorm;
    glm::vec3 currentBinorm;
};

struct FishSpecies {
    std::string name;
    unsigned int vao;
    unsigned int vbo;
    int vertexCount;
    unsigned int texDiffuse;
    unsigned int texNormal;
    unsigned int texRoughness;
};

class FishManager {
public:
    void init();
    void update(float deltaTime, const glm::vec3& cameraPos, bool feedingMode);
    void render(unsigned int shader, const glm::mat4& view, const glm::mat4& projection,
                const glm::mat4& lightSpaceMatrix, const glm::vec3& lightDir,
                const glm::vec3& viewPos, float time,
                bool flashlightOn, const glm::vec3& flashlightDir,
                unsigned int shadowMap);
    void renderShadow(unsigned int shadowShader, const glm::mat4& lightSpaceMatrix);

private:
    std::vector<FishSpecies> species;
    std::vector<Fish> fishes;

    void loadSpecies(const std::string& name, const std::string& objPath,
                     const std::string& diffusePath, const std::string& normalPath,
                     const std::string& roughnessPath);
    void spawnSchool(int speciesIdx, glm::vec3 center, float radius, int count,
                     float baseSpeed, float yMin, float yMax);
    std::vector<glm::vec3> generateSwimPath(glm::vec3 center, float radius,
                                             float yMin, float yMax);
};
