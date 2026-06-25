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
    planes[0] = glm::vec4(vp[0][3] + vp[0][0], vp[1][3] + vp[1][0], vp[2][3] + vp[2][0], vp[3][3] + vp[3][0]);
    planes[1] = glm::vec4(vp[0][3] - vp[0][0], vp[1][3] - vp[1][0], vp[2][3] - vp[2][0], vp[3][3] - vp[3][0]);
    planes[2] = glm::vec4(vp[0][3] + vp[0][1], vp[1][3] + vp[1][1], vp[2][3] + vp[2][1], vp[3][3] + vp[3][1]);
    planes[3] = glm::vec4(vp[0][3] - vp[0][1], vp[1][3] - vp[1][1], vp[2][3] - vp[2][1], vp[3][3] - vp[3][1]);
    planes[4] = glm::vec4(vp[0][3] + vp[0][2], vp[1][3] + vp[1][2], vp[2][3] + vp[2][2], vp[3][3] + vp[3][2]);
    planes[5] = glm::vec4(vp[0][3] - vp[0][2], vp[1][3] - vp[1][2], vp[2][3] - vp[2][2], vp[3][3] - vp[3][2]);
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

void PlantManager::loadSpecies(const std::string& name, const std::string& maskPath, const std::string& tuftJsonPath, const std::string& texPath, const std::vector<std::string>& variantPaths, float scaleMult,
                               const std::vector<std::string>& flatModelPaths, const std::vector<std::string>& flatTexPaths, bool flipMainTex) {
    std::cout << "Ladowanie gatunku: " << name << std::endl;
    PlantSpecies species;
    species.name = name;
    species.textureDiffuse = loadTexture(findAssetPath(texPath).c_str(), flipMainTex, true); // clamp=true

    // Wczytaj szczegółowe warianty modeli
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

    // Wczytaj flat modele jeśli podano (jako fallback dla wszystkich, lub per-wariant)
    for (int i = 0; i < (int)species.variants.size(); ++i) {
        std::string fp = "";
        std::string ftp = "";
        
        if (flatModelPaths.size() == 1) fp = flatModelPaths[0];
        else if (i < (int)flatModelPaths.size()) fp = flatModelPaths[i];
        
        if (flatTexPaths.size() == 1) ftp = flatTexPaths[0];
        else if (i < (int)flatTexPaths.size()) ftp = flatTexPaths[i];
        
        if (!fp.empty()) {
            std::vector<Vertex> flatVerts;
            if (loadOBJ(findAssetPath(fp), flatVerts)) {
                setupBaseVBO(flatVerts, species.variants[i].flatVBO);
                species.variants[i].flatVertexCount = flatVerts.size();
                if (!ftp.empty()) {
                    species.variants[i].flatTexture = loadTexture(findAssetPath(ftp).c_str(), true, true); // flip=true, clamp=true
                } else {
                    species.variants[i].flatTexture = species.textureDiffuse;
                }
                species.variants[i].hasFlatLOD = true;
                std::cout << "  -> Flat LOD loaded for variant " << i << ": " << fp << " (" << flatVerts.size() / 3 << " tris)" << std::endl;
            }
        }
    }

    int w = 1, h = 1, channels = 1;
    unsigned char* maskData = nullptr;
    if (!maskPath.empty()) {
        stbi_set_flip_vertically_on_load(false);
        maskData = stbi_load(findAssetPath(maskPath).c_str(), &w, &h, &channels, 1);
        if (!maskData) { std::cerr << "Nie udalo sie wczytac maski: " << maskPath << std::endl; }
    }

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
        std::vector<std::map<std::pair<int, int>, std::vector<glm::mat4>>> variantFlatChunkedMatrices(species.variants.size());

    // Gęsta dżungla podwodna — dużo prób spawnowania
    int numSpawnAttempts = hasTuft ? 80000 : 400000;
    if (name == "Tatarak") numSpawnAttempts = 6000000;
    
    // Znacznie zmniejszona skala, aby zrealizować prośbę o mniejsze wodorosty
    std::uniform_real_distribution<float> randomScaleXZ(0.4f, 0.85f); 
    std::uniform_real_distribution<float> randomScaleY(0.35f, 1.0f); 
    
    // Bierzemy co 4-ty element kępki (balans gęstość / wydajność)
    int tuftStride = 4;
    
    for (int i = 0; i < numSpawnAttempts; ++i) {
        float x = distX(engine);
        float z = distZ(engine);
        TerrainData td = getTerrainData(x, z);
        if (td.height < -900.0f) continue;
        
        // Drzewa rosną tylko na suchym lądzie (brzeg > 64.5m)
        if (maskPath.empty() && (td.height < 64.5f || td.height > 90.0f)) continue;
        
        int px = 0;
        int py = 0;
        unsigned char maskVal = 255;
        
        if (maskData) {
            px = glm::clamp((int)(td.uv.x * w), 0, w - 1);
            py = glm::clamp((int)((1.0f - td.uv.y) * h), 0, h - 1);
            maskVal = maskData[py * w + px];
        }
        
        if (maskVal > 20) {
            float prob = maskVal / 255.0f;
            if (maskPath.empty()) prob = 0.1f; 
            
            if (name == "Tatarak") prob = 1.0f;
            
            std::uniform_real_distribution<float> probDist(0.0f, 1.0f);
            if (probDist(engine) > prob) continue;

            if (hasTuft) {
                float tuftRot = rotDist(engine);
                int tuftIdx = 0;
                for (auto& item : tuftJson) {
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

                    float cx = ox * cos(glm::radians(tuftRot)) - oz * sin(glm::radians(tuftRot));
                    float cz = ox * sin(glm::radians(tuftRot)) + oz * cos(glm::radians(tuftRot));

                    float worldX = x + cx;
                    float worldZ = z + cz;
                    TerrainData itd = getTerrainData(worldX, worldZ);
                    if (itd.height < -900.0f) continue;

                    glm::vec3 currentScaleVec = glm::vec3(randomScaleXZ(engine), randomScaleY(engine), randomScaleXZ(engine)) * scaleMult;
                    
                    float currentY = itd.height + oy;
                    
                    if (name != "Tatarak") {
                        // Ograniczenie wysokości pod powierzchnią wody (dla elastyczności uwzględniamy scaleY)
                        float topY = currentY + currentScaleVec.y * 1.5f;
                        if (topY > 63.5f) {
                            currentY -= (topY - 63.5f);
                        }
                    }

                    glm::mat4 m = glm::translate(glm::mat4(1.0f), glm::vec3(worldX, currentY, worldZ));
                    m = glm::rotate(m, glm::radians(tuftRot), glm::vec3(0,1,0));
                    m = glm::rotate(m, glm::radians(rx), glm::vec3(1,0,0));
                    m = glm::rotate(m, glm::radians(ry), glm::vec3(0,1,0));
                    m = glm::rotate(m, glm::radians(rz), glm::vec3(0,0,1));
                    m = glm::scale(m, glm::vec3(sx, sy, sz) * currentScaleVec);

                    int chunkX = std::floor(worldX / 10.0f); int chunkZ = std::floor(worldZ / 10.0f);
                    variantChunkedMatrices[varIdx][{chunkX, chunkZ}].push_back(m);
                    
                    if (species.variants[varIdx].hasFlatLOD) {
                        glm::mat4 flatM = glm::translate(glm::mat4(1.0f), glm::vec3(worldX, currentY, worldZ));
                        flatM = glm::rotate(flatM, glm::radians(tuftRot), glm::vec3(0,1,0));
                        flatM = glm::scale(flatM, currentScaleVec);
                        variantFlatChunkedMatrices[varIdx][{chunkX, chunkZ}].push_back(flatM);
                    }
                }
            } else {
                int varIdx = engine() % species.variants.size();
                glm::vec3 currentScaleVec = glm::vec3(randomScaleXZ(engine), randomScaleY(engine), randomScaleXZ(engine)) * scaleMult;
                float currentY = td.height;
                if (name != "Tatarak" && name != "Drzewo (Sosna)" && name != "Osoka Brzeg" && (currentY + currentScaleVec.y * 1.5f) > 63.5f) currentY -= ((currentY + currentScaleVec.y * 1.5f) - 63.5f);
                if (name == "Drzewo (Sosna)") currentY -= 1.0f;

                glm::mat4 m = glm::translate(glm::mat4(1.0f), glm::vec3(x, currentY, z));
                m = glm::rotate(m, glm::radians(rotDist(engine)), glm::vec3(0,1,0));
                m = glm::scale(m, currentScaleVec);
                
                int chunkX = std::floor(x / 10.0f); int chunkZ = std::floor(z / 10.0f);
                variantChunkedMatrices[varIdx][{chunkX, chunkZ}].push_back(m);
                
                if (species.variants[varIdx].hasFlatLOD) {
                    variantFlatChunkedMatrices[varIdx][{chunkX, chunkZ}].push_back(m);
                }
            }
        }
    }

    // Zbuduj chunki dla szczegółowych wariantów i ich odpowiedniki flatVAO
    for (size_t i = 0; i < species.variants.size(); ++i) {
        for (const auto& pair : variantChunkedMatrices[i]) {
            PlantChunk chunk;
            chunk.instanceCount = pair.second.size();
            float cx = pair.first.first * 10.0f + 5.0f;
            float cz = pair.first.second * 10.0f + 5.0f;
            float cy = getTerrainHeight(cx, cz);
            if (cy < -900.0f) cy = -10.0f;
            chunk.center = glm::vec3(cx, cy, cz);
            
            // Detail VAO
            setupChunkVAO(species.variants[i].baseVBO, pair.second, chunk.vao, chunk.instVBO);
            
            // Flat VAO
            if (species.variants[i].hasFlatLOD) {
                const auto& flatMats = variantFlatChunkedMatrices[i].at(pair.first);
                unsigned int flatInstVBO;
                setupChunkVAO(species.variants[i].flatVBO, flatMats, chunk.flatVAO, flatInstVBO);
            }
            
            species.variants[i].chunks.push_back(chunk);
        }
        if (species.variants[i].hasFlatLOD) {
            std::cout << "  -> Flat LOD chunks for variant " << i << ": " << species.variants[i].chunks.size() << std::endl;
        }
    }

    if (maskData) stbi_image_free(maskData);
    speciesList.push_back(species);
    std::cout << "Zaladowano gatunek " << name << " z sukcesem." << std::endl;
}

void PlantManager::init() {
    loadSpecies("Moczarka Delikatna", 
                "Assets/distribution-masks/maska-moczarki-delikatne.png",
                "Assets/tufts/moczarka-delikatna-kępka.json",
                "Assets/flat-models/renders/moczarka-delikatna/moczarka-delikatna-flat0.png",
                {"Assets/flat-models/objects/moczarka-delikatna-flat.obj"}, 2.5f,
                {"Assets/flat-models/objects/moczarka-delikatna-flat.obj"},
                {"Assets/flat-models/renders/moczarka-delikatna/moczarka-delikatna-flat0.png"}, true);

    loadSpecies("Mech Zdrojek", 
                "Assets/distribution-masks/maska-mech-zdrojek.png",
                "Assets/tufts/mech-zdrojek-kępka.json",
                "Assets/flat-models/renders/mech-zdrojek/mech-zdrojek-flat0.png",
                {"Assets/flat-models/objects/mech-zdrojek-flat.obj"}, 2.5f,
                {"Assets/flat-models/objects/mech-zdrojek-flat.obj"},
                {"Assets/flat-models/renders/mech-zdrojek/mech-zdrojek-flat0.png"}, true);

    loadSpecies("Moczarka Kanadyjska", 
                "Assets/distribution-masks/maska-moczarki-kanadyjskie.png",
                "Assets/tufts/moczarka-kanadyjska-kępka.json",
                "Assets/flat-models/renders/moczarka-kanadyjska/moczarka-kanadyjska-flat0.png",
                {"Assets/flat-models/objects/moczarka-kanadyjska-flat.obj"}, 2.5f,
                {"Assets/flat-models/objects/moczarka-kanadyjska-flat.obj"},
                {"Assets/flat-models/renders/moczarka-kanadyjska/moczarka-kanadyjska-flat0.png"}, true);

    loadSpecies("Rogatek Sztywny", 
                "Assets/distribution-masks/maska-rogatek-sztywny.png",
                "Assets/tufts/rogatek-sztywny-kępka.json",
                "Assets/flat-models/renders/rogatek-sztywny/rogatek-sztywny-flat0.png",
                {"Assets/flat-models/objects/rogatek-sztywny-flat.obj"}, 2.5f,
                {"Assets/flat-models/objects/rogatek-sztywny-flat.obj"},
                {"Assets/flat-models/renders/rogatek-sztywny/rogatek-sztywny-flat0.png"}, true);

    loadSpecies("Rogatek Krotkoszyjkowy", 
                "Assets/distribution-masks/maska-rogatek-krotkoszyjkowy.png",
                "Assets/tufts/rogatek-krotkoszyjkowy-kepka.json",
                "Assets/flat-models/renders/rogatek-krotkoszyjkowy/rogatek-krotkoszyjkowy-flat0.png",
                {"Assets/flat-models/objects/rogatek-krotkoszyjkowy-flat.obj"}, 2.5f,
                {"Assets/flat-models/objects/rogatek-krotkoszyjkowy-flat.obj"},
                {"Assets/flat-models/renders/rogatek-krotkoszyjkowy/rogatek-krotkoszyjkowy-flat0.png"}, true);

    // Tatarak posiada teraz płaskie modele (LOD)
    loadSpecies("Tatarak", 
                "Assets/distribution-masks/maska-tatarak.png",
                "",
                "Assets/materials/tatarak/tatarak1/tatarak1-diffuse.png",
                {"Assets/models/tatarak/tatarak-1.obj", "Assets/models/tatarak/tatarak-6.obj", "Assets/models/tatarak/tatarak-7.obj"}, 1.5f,
                {"Assets/flat-models/objects/tatarak1-flat.obj", "Assets/flat-models/objects/tatarak6-flat.obj", "Assets/flat-models/objects/tatarak7-flat.obj"},
                {"Assets/flat-models/renders/tatarak/tatarak1-0.png", "Assets/flat-models/renders/tatarak/tatarak6-0.png", "Assets/flat-models/renders/tatarak/tatarak7-0.png"});
    
    loadSpecies("Osoka Woda", 
                "Assets/distribution-masks/maska-osoka-aloesowata-woda.png",
                "",
                "Assets/materials/osoka-aloesowata/osoka-aloesowata-diffuse.png",
                {"Assets/models/osoka-aloesowata/osoka-aloesowata-11.obj", "Assets/models/osoka-aloesowata/osoka-aloesowata-12.obj"}, 2.5f);
                
    loadSpecies("Osoka Brzeg", 
                "Assets/distribution-masks/maska-osoka-aloesowata-brzeg.png",
                "",
                "Assets/materials/osoka-aloesowata/osoka-aloesowata-diffuse.png",
                {"Assets/models/osoka-aloesowata/osoka-aloesowata-21.obj", "Assets/models/osoka-aloesowata/osoka-aloesowata-22.obj", "Assets/models/osoka-aloesowata/osoka-aloesowata-23.obj"}, 2.5f);
                
    // Proste drzewa lądowe (sosny) bez maski (spawnowanie zależy od wysokości terenu)
    loadSpecies("Drzewo (Sosna)", 
                "", 
                "",
                "Assets/materials/drzewo/drzewo-diffuse.png",
                {"Assets/models/drzewo/drzewo.obj"}, 22.0f, {}, {}, true); // flipMainTex = true!
}

void PlantManager::render(unsigned int shader, const glm::vec3& camPos, const glm::mat4& vpMatrix) {
    glUseProgram(shader);
    glm::vec2 camPos2D(camPos.x, camPos.z);
    auto frustumPlanes = extractFrustumPlanes(vpMatrix);
    
    // Próg LOD: poniżej tego dystansu → szczegółowe, powyżej → flat.
    // Chunk jest 10x10m, więc decyzja jest per-chunk: albo detail, albo flat.
    // Zmniejszamy próg, aby bardzo drastycznie poprawić FPS (2 FPS to tragedia)
    const float LOD_THRESHOLD = 12.0f; 
    const float FADE_BAND = 5.0f; // Szerokość strefy płynnego przenikania
    const float RENDER_DIST = (camPos.y > 64.0f) ? 45.0f : 35.0f;
    const float CHUNK_RADIUS = 7.07f;
    
    GLint loc_lodFadeMode = glGetUniformLocation(shader, "lodFadeMode");
    GLint loc_lodThreshold = glGetUniformLocation(shader, "lodThreshold");
    GLint loc_lodFadeBand = glGetUniformLocation(shader, "lodFadeBand");
    GLint loc_isTree = glGetUniformLocation(shader, "isTree");
    
    glUniform1f(loc_lodThreshold, LOD_THRESHOLD);
    glUniform1f(loc_lodFadeBand, FADE_BAND);
    
    for (auto& species : speciesList) {
        glUniform1i(loc_isTree, (species.name == "Drzewo (Sosna)") ? 1 : 0);
        
        // === Zbierz zbiór chunków blisko (detail) ===
        // Używamy chunk center distance do podjęcia decyzji: detail czy flat
        
        // PASS 1: Szczegółowe modele (chunki bliskie kamery)
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, species.textureDiffuse);
        
        for (auto& var : species.variants) {
            for (const auto& chunk : var.chunks) {
                glm::vec2 chunkPos2D(chunk.center.x, chunk.center.z);
                float dist = glm::distance(chunkPos2D, camPos2D);
                
                float currentChunkRadius = CHUNK_RADIUS;
                if (species.name == "Drzewo (Sosna)") currentChunkRadius = 45.0f; // Drzewa są potężne, frustum culling nie może ucinać ich po liściach!

                bool inFrustum = sphereInFrustum(frustumPlanes, chunk.center, currentChunkRadius);
                
                // Detail mode: lodFadeMode = 1
                float currentRenderDist = RENDER_DIST;
                // Drzewa nie mają flat LOD, i muszą być widoczne zawsze (nie znikają bez względu na dystans)
                if (species.name == "Drzewo (Sosna)") currentRenderDist = 999999.0f;
                
                bool useDetail = var.hasFlatLOD ? (dist < LOD_THRESHOLD) : (dist < currentRenderDist + currentChunkRadius);
                
                if (useDetail && inFrustum) {
                    // Jeśli mamy model flat, robimy twarde cięcie bez kropek. Jeśli nie, renderujemy normalnie (0).
                    glUniform1i(loc_lodFadeMode, 0); 
                    glBindVertexArray(chunk.vao);
                    glDrawArraysInstanced(GL_TRIANGLES, 0, var.vertexCount, chunk.instanceCount);
                }
            }
        }
        
        // PASS 2: Flat billboardy (chunki dalsze, ale w zasięgu mgły)
        // PASS 2: Flat billboardy (chunki dalsze, ale w zasięgu mgły)
        glActiveTexture(GL_TEXTURE0);
        for (auto& var : species.variants) {
            if (!var.hasFlatLOD) continue;
            
            glBindTexture(GL_TEXTURE_2D, var.flatTexture);
            for (const auto& chunk : var.chunks) {
                glm::vec2 chunkPos2D(chunk.center.x, chunk.center.z);
                float dist = glm::distance(chunkPos2D, camPos2D);
                
                bool inFrustum = sphereInFrustum(frustumPlanes, chunk.center, CHUNK_RADIUS);
                // Flat LOD: render od progu LOD do końca widoczności
                // Ponieważ zrezygnowaliśmy z ditheringu (kropek), robimy twardy przeskok na tańsze oświetlenie
                float currentFlatRenderDist = RENDER_DIST;
                if (species.name == "Tatarak" || species.name == "Drzewo (Sosna)") currentFlatRenderDist = 999999.0f;
                
                bool useFlat = (dist >= LOD_THRESHOLD) && (dist < currentFlatRenderDist + CHUNK_RADIUS);
                
                if (useFlat && inFrustum) {
                    // lodFadeMode = 3 dla flat modeli (zmienione dla naprawy przezroczystości)
                    glUniform1i(loc_lodFadeMode, 3); 
                    glBindVertexArray(chunk.flatVAO);
                    glDrawArraysInstanced(GL_TRIANGLES, 0, var.flatVertexCount, chunk.instanceCount);
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
    const float SHADOW_DIST = 8.0f; // Cienie tylko na bliskich roślinach
    const float CHUNK_RADIUS = 7.07f;
    
    for (auto& species : speciesList) {
        for (auto& var : species.variants) {
            if (var.hasFlatLOD) {
                // Używamy zoptymalizowanego płaskiego modelu do rzucania cieni (uproszczony cień)
                for (const auto& chunk : var.chunks) {
                    glm::vec2 chunkPos2D(chunk.center.x, chunk.center.z);
                    float dist = glm::distance(chunkPos2D, camPos2D);
                    if (dist > SHADOW_DIST + CHUNK_RADIUS) continue;
                    
                    if (sphereInFrustum(frustumPlanes, chunk.center, CHUNK_RADIUS)) {
                        glBindVertexArray(chunk.flatVAO);
                        glDrawArraysInstanced(GL_TRIANGLES, 0, var.flatVertexCount, chunk.instanceCount);
                    }
                }
            } else {
                // Brak płaskiego modelu, używamy modelu pełnego
                for (const auto& chunk : var.chunks) {
                    glm::vec2 chunkPos2D(chunk.center.x, chunk.center.z);
                    float dist = glm::distance(chunkPos2D, camPos2D);
                    
                    float currentChunkRadius = CHUNK_RADIUS;
                    if (species.name == "Drzewo (Sosna)") currentChunkRadius = 45.0f;
                    
                    if (dist > SHADOW_DIST + currentChunkRadius) continue;
                    
                    if (sphereInFrustum(frustumPlanes, chunk.center, currentChunkRadius)) {
                        glBindVertexArray(chunk.vao);
                        glDrawArraysInstanced(GL_TRIANGLES, 0, var.vertexCount, chunk.instanceCount);
                    }
                }
            }
        }
    }
    glEnable(GL_CULL_FACE);
}
