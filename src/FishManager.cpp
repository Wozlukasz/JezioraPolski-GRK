#include "FishManager.h"
#include "Model.h"
#include "Texture.h"
#include "Utils.h"
#include "Camera.h"

#include <glad/glad.h>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <iostream>
#include <random>
#include "stb_image.h"

void FishManager::loadSpecies(const std::string& name, const std::string& objPath,
                               const std::string& diffusePath, const std::string& normalPath,
                               const std::string& roughnessPath) {
    FishSpecies sp;
    sp.name = name;

    std::vector<Vertex> verts;
    if (!loadOBJ(findAssetPath(objPath), verts)) {
        std::cerr << "Nie udalo sie wczytac modelu ryby: " << objPath << std::endl;
        return;
    }
    sp.vertexCount = verts.size();

    // Create VAO/VBO for this species
    glGenVertexArrays(1, &sp.vao);
    glGenBuffers(1, &sp.vbo);
    glBindVertexArray(sp.vao);
    glBindBuffer(GL_ARRAY_BUFFER, sp.vbo);
    glBufferData(GL_ARRAY_BUFFER, verts.size() * sizeof(Vertex), verts.data(), GL_STATIC_DRAW);

    // Position
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, position));
    glEnableVertexAttribArray(0);
    // Normal
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, normal));
    glEnableVertexAttribArray(1);
    // TexCoords
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, texCoords));
    glEnableVertexAttribArray(2);
    // Tangent
    glVertexAttribPointer(3, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, tangent));
    glEnableVertexAttribArray(3);
    // Bitangent
    glVertexAttribPointer(4, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, bitangent));
    glEnableVertexAttribArray(4);

    glBindVertexArray(0);

    // Load PBR textures
    stbi_set_flip_vertically_on_load(true);
    sp.texDiffuse   = loadTexture(findAssetPath(diffusePath).c_str());
    sp.texNormal    = loadTexture(findAssetPath(normalPath).c_str());
    sp.texRoughness = loadTexture(findAssetPath(roughnessPath).c_str());

    species.push_back(sp);
    std::cout << "Zaladowano gatunek ryby: " << name << " (" << verts.size() / 3 << " tris)" << std::endl;
}

std::vector<glm::vec3> FishManager::generateSwimPath(glm::vec3 center, float radius,
                                                      float yMin, float yMax) {
    std::mt19937 rng(std::random_device{}());
    std::uniform_real_distribution<float> angleDist(0.0f, 6.2832f);
    std::uniform_real_distribution<float> radiusDist(radius * 0.5f, radius);
    std::uniform_real_distribution<float> yDist(yMin, yMax);

    // Generate 10 control points in a roughly circular path
    int numPts = 10;
    std::vector<glm::vec3> cp;
    cp.reserve(numPts + 3); // +3 for wrapping (closed loop)

    for (int i = 0; i < numPts; ++i) {
        float angle = (float)i / numPts * 6.2832f + angleDist(rng) * 0.3f;
        float r = radiusDist(rng);
        float x = center.x + r * cos(angle);
        float z = center.z + r * sin(angle);
        float y = yDist(rng);
        cp.push_back(glm::vec3(x, y, z));
    }

    // Close the loop: duplicate first 3 points at the end for smooth wrapping
    cp.push_back(cp[0]);
    cp.push_back(cp[1]);
    cp.push_back(cp[2]);

    return cp;
}

void FishManager::spawnSchool(int speciesIdx, glm::vec3 center, float radius, int count,
                               float baseSpeed, float yMin, float yMax) {
    std::mt19937 rng(std::random_device{}());
    std::uniform_real_distribution<float> speedVar(0.8f, 1.2f);
    std::uniform_real_distribution<float> phaseDist(0.0f, 1.0f);
    std::uniform_real_distribution<float> offsetDist(-3.0f, 3.0f);

    for (int i = 0; i < count; ++i) {
        Fish fish;
        fish.speciesIndex = speciesIdx;
        fish.speed = baseSpeed * speedVar(rng);
        fish.t = phaseDist(rng);  // Start at random position on path
        fish.phase = phaseDist(rng);
        fish.baseCenter = center;

        // Each fish gets a slightly different path
        glm::vec3 fishCenter = center + glm::vec3(offsetDist(rng), offsetDist(rng) * 0.3f, offsetDist(rng));
        fish.controlPoints = generateSwimPath(fishCenter, radius, yMin, yMax);

        // Sample the spline and compute PTF
        auto sampled = sampleSpline(fish.controlPoints, 200);
        fish.frames = computePTFrames(sampled);

        fishes.push_back(fish);
    }
}

void FishManager::init() {
    // Load fish species
    loadSpecies("Płoć",
                "Assets/models/płoć/płoć.obj",
                "Assets/materials/płoć/płoć-diffuse.png",
                "Assets/materials/płoć/płoć-normal.png",
                "Assets/materials/płoć/płoć-roughness.png");

    loadSpecies("Ukleja",
                "Assets/models/ukleja/ukleja.obj",
                "Assets/materials/ukleja/ukleja-diffuse.png",
                "Assets/materials/ukleja/ukleja-normal.png",
                "Assets/materials/ukleja/ukleja-roughness.png");

    // Spawn schools of fish in the lake (underwater areas, y < 64)
    // School 1: Płocie tuż przed graczem, by od razu je zauważył
    spawnSchool(0, glm::vec3(0.0f, 54.0f, 6.0f), 8.0f, 6, 0.04f, 52.0f, 58.0f);

    // School 2: Ukleje blisko gracza (mniejsze, szybsze)
    spawnSchool(1, glm::vec3(8.0f, 55.0f, -5.0f), 10.0f, 10, 0.07f, 53.0f, 59.0f);

    // School 3: Więcej Płoci nieopodal
    spawnSchool(0, glm::vec3(-12.0f, 53.0f, 10.0f), 10.0f, 5, 0.035f, 50.0f, 58.0f);

    std::cout << "FishManager: zainicjalizowano " << fishes.size() << " ryb" << std::endl;
}

void FishManager::update(float deltaTime, const glm::vec3& cameraPos, bool feedingMode) {
    for (auto& fish : fishes) {
        if (fish.frames.empty()) continue;

        float speedMult = 1.0f;

        // Feeding mode: attract fish toward camera
        if (feedingMode && cameraPos.y < 64.0f) {
            glm::vec3 currentPos = fish.frames[(int)(fish.t * (fish.frames.size() - 1))].position;
            float distToCamera = glm::distance(currentPos, cameraPos);

            if (distToCamera < 30.0f) {
                // Regenerate path toward camera (smooth attraction)
                // Shift control points slightly toward camera
                glm::vec3 attractDir = glm::normalize(cameraPos - fish.baseCenter);
                float attractStrength = deltaTime * 2.0f;

                for (auto& cp : fish.controlPoints) {
                    cp += attractDir * attractStrength;
                }

                // Re-sample and recompute PTF
                auto sampled = sampleSpline(fish.controlPoints, 200);
                fish.frames = computePTFrames(sampled);

                speedMult = 1.5f; // Fish swim faster when attracted
            }
        } else {
            // Zamiast przeliczać krzywe co klatkę (co było masakrycznym wąskim gardłem CPU),
            // po prostu zostawiamy ryby na ich nowej ścieżce po karmieniu.
            // Będą pływać wokół miejsca, w którym ostatnio był gracz.
        }

        // Advance along path
        fish.t += fish.speed * speedMult * deltaTime;
        if (fish.t >= 1.0f) fish.t -= 1.0f;
        if (fish.t < 0.0f) fish.t += 1.0f;
    }
}

void FishManager::render(unsigned int shader, const glm::mat4& view, const glm::mat4& projection,
                          const glm::mat4& lightSpaceMatrix, const glm::vec3& lightDir,
                          const glm::vec3& viewPos, float time,
                          bool flashlightOn, const glm::vec3& flashlightDir,
                          unsigned int shadowMap) {
    glUseProgram(shader);

    glUniformMatrix4fv(glGetUniformLocation(shader, "view"), 1, GL_FALSE, glm::value_ptr(view));
    glUniformMatrix4fv(glGetUniformLocation(shader, "projection"), 1, GL_FALSE, glm::value_ptr(projection));
    glUniformMatrix4fv(glGetUniformLocation(shader, "lightSpaceMatrix"), 1, GL_FALSE, glm::value_ptr(lightSpaceMatrix));
    glUniform3fv(glGetUniformLocation(shader, "lightDir"), 1, glm::value_ptr(lightDir));
    glUniform3fv(glGetUniformLocation(shader, "viewPos"), 1, glm::value_ptr(viewPos));
    glUniform1f(glGetUniformLocation(shader, "time"), time);
    glUniform1i(glGetUniformLocation(shader, "flashlightOn"), flashlightOn ? 1 : 0);
    if (flashlightOn) {
        glUniform3fv(glGetUniformLocation(shader, "flashlightPos"), 1, glm::value_ptr(viewPos));
        glUniform3fv(glGetUniformLocation(shader, "flashlightDir"), 1, glm::value_ptr(flashlightDir));
    }

    for (auto& fish : fishes) {
        if (fish.frames.empty()) continue;
        if (fish.speciesIndex < 0 || fish.speciesIndex >= (int)species.size()) continue;

        const auto& sp = species[fish.speciesIndex];

        // Get current frame from PTF
        int frameIdx = glm::clamp((int)(fish.t * (fish.frames.size() - 1)), 0, (int)fish.frames.size() - 1);
        const PTFrame& frame = fish.frames[frameIdx];

        // Build model matrix from PTF frame + scale
        float scale = (fish.speciesIndex == 0) ? 0.9f : 0.6f;  // Płoć i Ukleja znacznie powiększone
        glm::mat4 model = frame.toMatrix();
        model = glm::scale(model, glm::vec3(scale));

        glUniformMatrix4fv(glGetUniformLocation(shader, "model"), 1, GL_FALSE, glm::value_ptr(model));

        // Bind PBR textures
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, sp.texDiffuse);
        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, sp.texNormal);
        glActiveTexture(GL_TEXTURE2);
        glBindTexture(GL_TEXTURE_2D, sp.texRoughness);
        glActiveTexture(GL_TEXTURE3);
        glBindTexture(GL_TEXTURE_2D, shadowMap);

        glBindVertexArray(sp.vao);
        glDrawArrays(GL_TRIANGLES, 0, sp.vertexCount);
    }
}

void FishManager::renderShadow(unsigned int shadowShader, const glm::mat4& lightSpaceMatrix) {
    glUseProgram(shadowShader);
    glUniformMatrix4fv(glGetUniformLocation(shadowShader, "lightSpaceMatrix"), 1, GL_FALSE, glm::value_ptr(lightSpaceMatrix));

    for (auto& fish : fishes) {
        if (fish.frames.empty()) continue;
        if (fish.speciesIndex < 0 || fish.speciesIndex >= (int)species.size()) continue;

        const auto& sp = species[fish.speciesIndex];

        int frameIdx = glm::clamp((int)(fish.t * (fish.frames.size() - 1)), 0, (int)fish.frames.size() - 1);
        const PTFrame& frame = fish.frames[frameIdx];

        float scale = (fish.speciesIndex == 0) ? 0.9f : 0.6f;
        glm::mat4 model = frame.toMatrix();
        model = glm::scale(model, glm::vec3(scale));

        glUniformMatrix4fv(glGetUniformLocation(shadowShader, "model"), 1, GL_FALSE, glm::value_ptr(model));

        glBindVertexArray(sp.vao);
        glDrawArrays(GL_TRIANGLES, 0, sp.vertexCount);
    }
}
