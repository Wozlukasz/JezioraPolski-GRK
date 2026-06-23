#include "PlantManager.h"
#include "Model.h"
#include "Texture.h"
#include "Utils.h"
#include "Camera.h"
#include "json.hpp"
#include "stb_image.h"
#include <glad/glad.h>
#include <fstream>
#include <iostream>
#include <random>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <map>
#include <array>

using json = nlohmann::json;

// ---- Frustum Culling Helpers ----
static std::array<glm::vec4, 6> extractFrustumPlanes(const glm::mat4& vp) {
    std::array<glm::vec4, 6> planes;
    // Left
    planes[0] = glm::vec4(vp[0][3] + vp[0][0], vp[1][3] + vp[1][0], vp[2][3] + vp[2][0], vp[3][3] + vp[3][0]);
    // Right
    planes[1] = glm::vec4(vp[0][3] - vp[0][0], vp[1][3] - vp[1][0], vp[2][3] - vp[2][0], vp[3][3] - vp[3][0]);
    // Bottom
    planes[2] = glm::vec4(vp[0][3] + vp[0][1], vp[1][3] + vp[1][1], vp[2][3] + vp[2][1], vp[3][3] + vp[3][1]);
    // Top
    planes[3] = glm::vec4(vp[0][3] - vp[0][1], vp[1][3] - vp[1][1], vp[2][3] - vp[2][1], vp[3][3] - vp[3][1]);
    // Near
    planes[4] = glm::vec4(vp[0][3] + vp[0][2], vp[1][3] + vp[1][2], vp[2][3] + vp[2][2], vp[3][3] + vp[3][2]);
    // Far
    planes[5] = glm::vec4(vp[0][3] - vp[0][2], vp[1][3] - vp[1][2], vp[2][3] - vp[2][2], vp[3][3] - vp[3][2]);
    // Normalize planes
    for (auto& p : planes) {
        float len = glm::length(glm::vec3(p));
        if (len > 0.0f) p /= len;
    }
    return planes;
}

static bool sphereInFrustum(const std::array<glm::vec4, 6>& planes, const glm::vec3& center, float radius) {
    for (const auto& p : planes) {
        float dist = glm::dot(glm::vec3(p), center) + p.w;
        if (dist < -radius) return false;
    }
    return true;
}

void PlantManager::setupBaseVBO(const std::vector<Vertex>& vertices, unsigned int& vbo) {
    glGenBuffers(1, &vbo);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(Vertex), vertices.data(), GL_STATIC_DRAW);
}

void PlantManager::setupChunkVAO(unsigned int baseVBO, const std::vector<glm::mat4>& matrices, unsigned int& vao, unsigned int& instVBO) {
    glGenVertexArrays(1, &vao);
    glGenBuffers(1, &instVBO);

    glBindVertexArray(vao);

    glBindBuffer(GL_ARRAY_BUFFER, baseVBO);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, position));
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, normal));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, texCoords));
    glEnableVertexAttribArray(2);

    glBindBuffer(GL_ARRAY_BUFFER, instVBO);
    glBufferData(GL_ARRAY_BUFFER, matrices.size() * sizeof(glm::mat4), matrices.data(), GL_STATIC_DRAW);
    
    std::size_t vec4Size = sizeof(glm::vec4);
    glEnableVertexAttribArray(5);
    glVertexAttribPointer(5, 4, GL_FLOAT, GL_FALSE, 4 * vec4Size, (void*)0);
    glEnableVertexAttribArray(6);
    glVertexAttribPointer(6, 4, GL_FLOAT, GL_FALSE, 4 * vec4Size, (void*)(1 * vec4Size));
    glEnableVertexAttribArray(7);
    glVertexAttribPointer(7, 4, GL_FLOAT, GL_FALSE, 4 * vec4Size, (void*)(2 * vec4Size));
    glEnableVertexAttribArray(8);
    glVertexAttribPointer(8, 4, GL_FLOAT, GL_FALSE, 4 * vec4Size, (void*)(3 * vec4Size));

    glVertexAttribDivisor(5, 1);
    glVertexAttribDivisor(6, 1);
    glVertexAttribDivisor(7, 1);
    glVertexAttribDivisor(8, 1);
    
    glBindVertexArray(0);
}

void PlantManager::loadSpecies(const std::string& name, const std::string& maskPath, const std::string& tuftJsonPath, const std::string& texPath, const std::vector<std::string>& variantPaths, float scaleMult) {
    std::cout << "Ladowanie gatunku: " << name << std::endl;
    PlantSpecies species;
    species.name = name;
    species.textureDiffuse = loadTexture(findAssetPath(texPath).c_str());

    for (const auto& vp : variantPaths) {
        PlantVariant pv;
        std::vector<Vertex> verts;
        if (loadOBJ(findAssetPath(vp), verts)) {
            setupBaseVBO(verts, pv.baseVBO);
            pv.vertexCount = verts.size();
            species.variants.push_back(pv);
        } else {
            std::cerr << "Nie udalo sie wczytac modelu wariantu: " << vp << std::endl;
        }
    }

    int w, h, channels;
    stbi_set_flip_vertically_on_load(false); // Maski próbkowane z Top-Left dla wygody, upewnijmy sie
    unsigned char* maskData = stbi_load(findAssetPath(maskPath).c_str(), &w, &h, &channels, 1);
    if (!maskData) { std::cerr << "Nie udalo sie wczytac maski: " << maskPath << std::endl; return; }

    json tuftJson;
    bool hasTuft = false;
    if (!tuftJsonPath.empty()) {
        std::ifstream f(findAssetPath(tuftJsonPath));
        if (f.is_open()) {
            f >> tuftJson;
            hasTuft = true;
        }
    }

    std::mt19937 engine(std::hash<std::string>{}(name));
    std::uniform_real_distribution<float> distX(-400.0f, 400.0f);
    std::uniform_real_distribution<float> distZ(-400.0f, 400.0f);
    std::uniform_real_distribution<float> rotDist(0.0f, 360.0f);

    std::vector<std::map<std::pair<int, int>, std::vector<glm::mat4>>> variantChunkedMatrices(species.variants.size());

    // Zoptymalizowana liczba prób. Kluczowa optymalizacja: kępki mają 67-165 elementów każda!
    // Gęstsza "dżungla" zgodnie z prośbą, gigantyczna ilość roślin
    int numSpawnAttempts = hasTuft ? 50000 : 250000;
    
    // Rośliny mają mieć do 1.5 metra wysokości.
    // Oryginalny model ma ~1.2m, więc max skala to 1.5/1.2 = 1.25.
    std::uniform_real_distribution<float> randomScale(0.8f, 1.25f); 
    
    // Ile elementów kępki pomijamy.
    // Ustawienie na 3 daje bardzo gęste, puszyste krzaki, jak na zdjęciu referencyjnym.
    int tuftStride = 3;
    
    for (int i = 0; i < numSpawnAttempts; ++i) {
        float x = distX(engine);
        float z = distZ(engine);
        TerrainData td = getTerrainData(x, z);
        if (td.height < -900.0f) continue;
        
        int px = glm::clamp((int)(td.uv.x * w), 0, w - 1);
        int py = glm::clamp((int)((1.0f - td.uv.y) * h), 0, h - 1);
        
        unsigned char maskVal = maskData[py * w + px];
        if (maskVal > 20) {
            float prob = maskVal / 255.0f;
            std::uniform_real_distribution<float> probDist(0.0f, 1.0f);
            if (probDist(engine) > prob) continue;

            if (hasTuft) {
                float tuftRot = rotDist(engine);
                int tuftIdx = 0;
                for (auto& item : tuftJson) {
                    // Deterministyczne przerzedzanie: bierzemy co tuftStride-ty element.
                    // To zachowuje strukturę modelu (równomiernie z całej kępki)
                    // zamiast losowego kasowania które niszczyło modele.
                    if (tuftIdx++ % tuftStride != 0) continue;
                    
                    int varIdx = item["variant"];
                    if (varIdx >= species.variants.size()) varIdx = species.variants.size() - 1;
                    
                    float ox = item["position"][0];
                    float oy = item["position"][1];
                    float oz = item["position"][2];
                    
                    float rx = item["rotation"][0];
                    float ry = item["rotation"][1];
                    float rz = item["rotation"][2];
                    
                    float sx = item["scale"][0];
                    float sy = item["scale"][1];
                    float sz = item["scale"][2];

                    // Przekształcenie punktów kępki (obrót kępki dookoła własnej osi)
                    float cx = ox * cos(glm::radians(tuftRot)) - oz * sin(glm::radians(tuftRot));
                    float cz = ox * sin(glm::radians(tuftRot)) + oz * cos(glm::radians(tuftRot));

                    float worldX = x + cx;
                    float worldZ = z + cz;
                    TerrainData itd = getTerrainData(worldX, worldZ);
                    if (itd.height < -900.0f) continue;

                    float currentScale = scaleMult * randomScale(engine);
                    
                    // Pozwalamy roślinom rosnąć potężnie. Jeśli będą za duże,
                    // po prostu zanurzymy je głębiej w mule na dnie.
                    
                    float currentY = itd.height + oy;
                    
                    // Żadna roślina (oprócz tataraku przy brzegu) nie może wystawać ponad wodę (Y = 64.0)
                    // Rzeczywista wysokość modelu w pliku .obj to około 1.2 jednostki
                    if (name != "Tatarak") {
                        float topY = currentY + currentScale * 1.5f; // upewniamy się, że góra jest poprawnie liczona
                        if (topY > 63.5f) {
                            currentY -= (topY - 63.5f); // Przesuń w dół, aby góra wynosiła 63.5
                        }
                    }

                    glm::mat4 m = glm::translate(glm::mat4(1.0f), glm::vec3(worldX, currentY, worldZ));
                    m = glm::rotate(m, glm::radians(tuftRot), glm::vec3(0,1,0));
                    m = glm::rotate(m, glm::radians(rx), glm::vec3(1,0,0));
                    m = glm::rotate(m, glm::radians(ry), glm::vec3(0,1,0));
                    m = glm::rotate(m, glm::radians(rz), glm::vec3(0,0,1));
                    m = glm::scale(m, glm::vec3(sx, sy, sz) * currentScale);

                    int chunkX = std::floor(worldX / 10.0f);
                    int chunkZ = std::floor(worldZ / 10.0f);
                    variantChunkedMatrices[varIdx][{chunkX, chunkZ}].push_back(m);
                }
            } else {
                int varIdx = engine() % species.variants.size();
                
                float currentScale = scaleMult * randomScale(engine);

                float currentY = td.height;
                
                if (name != "Tatarak") {
                    float topY = currentY + currentScale * 1.5f;
                    if (topY > 63.5f) {
                        currentY -= (topY - 63.5f);
                    }
                }

                glm::mat4 m = glm::translate(glm::mat4(1.0f), glm::vec3(x, currentY, z));
                m = glm::rotate(m, glm::radians(rotDist(engine)), glm::vec3(0,1,0));
                m = glm::scale(m, glm::vec3(currentScale));
                
                int cx = std::floor(x / 10.0f);
                int cz = std::floor(z / 10.0f);
                variantChunkedMatrices[varIdx][{cx, cz}].push_back(m);
            }
        }
    }

    for (size_t i = 0; i < species.variants.size(); ++i) {
        for (const auto& pair : variantChunkedMatrices[i]) {
            PlantChunk chunk;
            chunk.instanceCount = pair.second.size();
            float cx = pair.first.first * 10.0f + 5.0f;
            float cz = pair.first.second * 10.0f + 5.0f;
            float cy = getTerrainHeight(cx, cz);
            if (cy < -900.0f) cy = -10.0f; // fallback
            chunk.center = glm::vec3(cx, cy, cz);
            setupChunkVAO(species.variants[i].baseVBO, pair.second, chunk.vao, chunk.instVBO);
            species.variants[i].chunks.push_back(chunk);
        }
    }

    stbi_image_free(maskData);
    speciesList.push_back(species);
    std::cout << "Zaladowano gatunek " << name << " z sukcesem." << std::endl;
}

void PlantManager::init() {
    loadSpecies("Moczarka Delikatna", 
                "Assets/distribution-masks/maska-moczarki-delikatne.png",
                "Assets/tufts/moczarka-delikatna-kępka.json",
                "Assets/materials/moczarka-delikatna/moczarka-delikatna-diffuse.png",
                {"Assets/models/moczarka-delikatna/moczarka-delikatna-1.obj", "Assets/models/moczarka-delikatna/moczarka-delikatna-2.obj"}, 2.5f);

    loadSpecies("Mech Zdrojek", 
                "Assets/distribution-masks/maska-mech-zdrojek.png",
                "Assets/tufts/mech-zdrojek-kępka.json",
                "Assets/materials/mech-zdrojek/mech-zdrojek-diffuse.png",
                {"Assets/models/mech-zdrojek/mech-zdrojek-1.obj", "Assets/models/mech-zdrojek/mech-zdrojek-2.obj", "Assets/models/mech-zdrojek/mech-zdrojek-3.obj"}, 2.5f);

    loadSpecies("Moczarka Kanadyjska", 
                "Assets/distribution-masks/maska-moczarki-kanadyjskie.png",
                "Assets/tufts/moczarka-kanadyjska-kępka.json",
                "Assets/materials/moczarka-kanadyjska/moczarka-kanadyjska-diffuse.png",
                {"Assets/models/moczarka-kanadyjska/moczarka-kanadyjska-1.obj", "Assets/models/moczarka-kanadyjska/moczarka-kanadyjska-2.obj", "Assets/models/moczarka-kanadyjska/moczarka-kanadyjska-3.obj"}, 2.5f);

    loadSpecies("Rogatek Sztywny", 
                "Assets/distribution-masks/maska-rogatek-sztywny.png",
                "Assets/tufts/rogatek-sztywny-kępka.json",
                "Assets/materials/rogatek-sztywny/rogatek-sztywny-diffuse.png",
                {"Assets/models/rogatek-sztywny/rogatek-sztywny-1.obj", "Assets/models/rogatek-sztywny/rogatek-sztywny-2.obj"}, 2.5f);

    loadSpecies("Rogatek Krotkoszyjkowy", 
                "Assets/distribution-masks/maska-rogatek-krótkoszyjkowy.png",
                "Assets/tufts/rogatek-krótkoszyjkowy-kępka.json",
                "Assets/materials/rogatek-krótkoszyjkowy/rogatek-krótkoszyjkowy-diffuse.png",
                {"Assets/models/rogatek-krótkoszyjkowy/rogatek-krótkoszyjkowy-1.obj", "Assets/models/rogatek-krótkoszyjkowy/rogatek-krótkoszyjkowy-2.obj"}, 2.5f);

    // Brak json kępek dla tataraku i osoki
    loadSpecies("Tatarak", 
                "Assets/distribution-masks/maska-tatarak.png",
                "",
                "Assets/materials/tatarak/tatarak1/tatarak1-diffuse.png",
                {"Assets/models/tatarak/tatarak-1.obj", "Assets/models/tatarak/tatarak-6.obj", "Assets/models/tatarak/tatarak-7.obj"}, 1.5f);
    
    loadSpecies("Osoka Woda", 
                "Assets/distribution-masks/maska-osoka-aloesowata-woda.png",
                "",
                "Assets/materials/osoka-aloesowata/osoka-aloesowata-diffuse.png",
                {"Assets/models/osoka-aloesowata/osoka-aloesowata-11.obj", "Assets/models/osoka-aloesowata/osoka-aloesowata-12.obj"}, 2.5f);
}

void PlantManager::render(unsigned int shader, const glm::vec3& camPos, const glm::mat4& vpMatrix) {
    glUseProgram(shader);
    glm::vec2 camPos2D(camPos.x, camPos.z);
    auto frustumPlanes = extractFrustumPlanes(vpMatrix);
    // Pod wodą renderujemy z większym promieniem, żeby rośliny nie wyskakiwały przed nosem
    float RENDER_DIST = (camPos.y > 64.0f) ? 40.0f : 20.0f; 
    const float CHUNK_RADIUS = 7.07f;
    
    for (auto& species : speciesList) {
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, species.textureDiffuse);
        
        for (auto& var : species.variants) {
            for (const auto& chunk : var.chunks) {
                glm::vec2 chunkPos2D(chunk.center.x, chunk.center.z);
                float dist = glm::distance(chunkPos2D, camPos2D);
                if (dist < RENDER_DIST + CHUNK_RADIUS &&
                    sphereInFrustum(frustumPlanes, chunk.center, CHUNK_RADIUS)) {
                    
                    int drawInstanceCount = chunk.instanceCount;
                    // Poprawny system LOD - zamiast ciąć modele w połowie (dziury w łodydze),
                    // po prostu zmniejszamy liczbę rysowanych całych roślin w dalekich chunkach!
                    // Agresywniejszy LOD — kluczowe dla wydajności
                    if (dist > 8.0f) {
                        drawInstanceCount = chunk.instanceCount / 6; // 16% instancji
                    } else if (dist > 5.0f) {
                        drawInstanceCount = chunk.instanceCount / 3; // 33%
                    }
                    if (drawInstanceCount < 1) drawInstanceCount = 1;

                    glBindVertexArray(chunk.vao);
                    glDrawArraysInstanced(GL_TRIANGLES, 0, var.vertexCount, drawInstanceCount);
                }
            }
        }
    }
}

void PlantManager::renderShadow(unsigned int shadowShader, const glm::vec3& camPos, const glm::mat4& vpMatrix) {
    glUseProgram(shadowShader);
    glDisable(GL_CULL_FACE);
    
    glm::vec2 camPos2D(camPos.x, camPos.z);
    auto frustumPlanes = extractFrustumPlanes(vpMatrix);
    const float SHADOW_DIST = 15.0f; // Cienie na 15m
    const float CHUNK_RADIUS = 7.07f;
    
    for (auto& species : speciesList) {
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, species.textureDiffuse);
        
        for (auto& var : species.variants) {
            for (const auto& chunk : var.chunks) {
                glm::vec2 chunkPos2D(chunk.center.x, chunk.center.z);
                float dist = glm::distance(chunkPos2D, camPos2D);
                if (dist < SHADOW_DIST + CHUNK_RADIUS &&
                    sphereInFrustum(frustumPlanes, chunk.center, CHUNK_RADIUS * 2.0f)) {
                    
                    int drawInstanceCount = chunk.instanceCount;
                    // Bardzo agresywny LOD cieni
                    drawInstanceCount = chunk.instanceCount / 4;
                    if (drawInstanceCount < 1) drawInstanceCount = 1;

                    glBindVertexArray(chunk.vao);
                    glDrawArraysInstanced(GL_TRIANGLES, 0, var.vertexCount, drawInstanceCount);
                }
            }
        }
    }
    glEnable(GL_CULL_FACE);
}
