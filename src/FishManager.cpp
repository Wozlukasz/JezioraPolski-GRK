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
    std::uniform_real_distribution<float> radiusVar(0.85f, 1.15f);
    std::uniform_real_distribution<float> yVar(yMin, yMax);
    std::uniform_real_distribution<float> angleVar(-0.15f, 0.15f); // Lekkie zaburzenie kąta

    // Generate 12 control points in a smooth roughly-circular path
    int numPts = 12;
    std::vector<glm::vec3> cp;
    cp.reserve(numPts + 3);

    for (int i = 0; i < numPts; ++i) {
        float baseAngle = (float)i / numPts * 6.2832f;
        float angle = baseAngle + angleVar(rng);
        float r = radius * radiusVar(rng);
        float x = center.x + r * cos(angle);
        float z = center.z + r * sin(angle);
        // Łagodna sinusoida po Y — ryba lekko faluje w pionie
        float yBase = (yMin + yMax) * 0.5f;
        float yAmplitude = (yMax - yMin) * 0.3f;
        float y = yBase + yAmplitude * sin(baseAngle * 1.5f) + (yVar(rng) - yBase) * 0.2f;
        y = glm::clamp(y, yMin, yMax);
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
    std::uniform_real_distribution<float> phaseDist(0.0f, 1.0f);
    
    // 1. Generate ONE central path and PTFrames for the entire school
    auto schoolControlPoints = generateSwimPath(center, radius, yMin, yMax);
    auto sampled = sampleSpline(schoolControlPoints, 400);
    auto schoolFrames = computePTFrames(sampled);
    
    float schoolBasePhase = phaseDist(rng);

    // Dystrybucje dla lokalnych offsetów względem ścieżki (szerokość, wysokość, przód-tył)
    std::uniform_real_distribution<float> offsetLeftRight(-4.0f, 4.0f); // Oś Z modelu (bok)
    std::uniform_real_distribution<float> offsetUpDown(-2.0f, 2.0f);    // Oś Y modelu (góra/dół)
    std::uniform_real_distribution<float> phaseOffset(-0.04f, 0.04f);   // Rozrzut wzdłuż ścieżki (T)
    std::uniform_real_distribution<float> speedVar(0.98f, 1.02f);       // Minimalne odchyłki prędkości
    std::uniform_real_distribution<float> scatterDist(-1.0f, 1.0f);     // Losowy kierunek ucieczki

    for (int i = 0; i < count; ++i) {
        Fish fish;
        fish.speciesIndex = speciesIdx;
        fish.speed = baseSpeed * speedVar(rng);
        
        fish.t = schoolBasePhase + phaseOffset(rng);
        if (fish.t < 0.0f) fish.t += 1.0f;
        if (fish.t >= 1.0f) fish.t -= 1.0f;
        
        fish.phase = phaseDist(rng);
        fish.baseCenter = center;

        // Share the same path data
        fish.controlPoints = schoolControlPoints;
        fish.frames = schoolFrames;
        
        // Z (left/right), Y (up/down), X (forward/backward local shift - set to 0 as phaseOffset handles forward/backward)
        fish.localOffset = glm::vec3(0.0f, offsetUpDown(rng), offsetLeftRight(rng));
        fish.panicLevel = 0.0f;
        
        // Unikalny kierunek rozpraszania się podczas paniki
        glm::vec3 scatter = glm::vec3(scatterDist(rng), scatterDist(rng) * 0.5f, scatterDist(rng));
        if (glm::length(scatter) > 0.001f) scatter = glm::normalize(scatter);
        fish.scatterDir = scatter;

        fishes.push_back(fish);
    }
}

void FishManager::init() {
    // Load fish species
    loadSpecies("Płoć",
                "Assets/models/ploc/ploc.obj",
                "Assets/materials/ploc/ploc-diffuse.png",
                "Assets/materials/ploc/ploc-normal.png",
                "Assets/materials/ploc/ploc-roughness.png");

    loadSpecies("Ukleja",
                "Assets/models/ukleja/ukleja.obj",
                "Assets/materials/ukleja/ukleja-diffuse.png",
                "Assets/materials/ukleja/ukleja-normal.png",
                "Assets/materials/ukleja/ukleja-roughness.png");

    // Spawn schools of fish in the lake (underwater, y < 64 = waterline)
    // Gracz startuje na (32, ~66, 38) — ławice muszą być pod nim i w zasięgu wzroku

    // === BLISKO GRACZA — od razu widoczne po wejściu do wody ===
    spawnSchool(0, glm::vec3(28.0f, 57.0f, 35.0f), 18.0f, 50, 0.007f, 52.0f, 61.0f);
    spawnSchool(1, glm::vec3(22.0f, 59.0f, 30.0f), 12.0f, 60, 0.009f, 55.0f, 62.0f);
    spawnSchool(0, glm::vec3(35.0f, 55.0f, 28.0f), 15.0f, 45, 0.008f, 50.0f, 59.0f);

    // === ŚRODEK JEZIORA — duże ławice ===
    spawnSchool(1, glm::vec3(10.0f, 57.0f, 10.0f), 25.0f, 80, 0.008f, 53.0f, 61.0f);
    spawnSchool(0, glm::vec3(0.0f, 54.0f, 0.0f),   30.0f, 60, 0.006f, 48.0f, 58.0f);
    spawnSchool(1, glm::vec3(-5.0f, 58.0f, 5.0f),  20.0f, 55, 0.007f, 54.0f, 62.0f);

    // === DALSZE CZĘŚCI JEZIORA ===
    spawnSchool(0, glm::vec3(-20.0f, 53.0f, 20.0f),  22.0f, 40, 0.005f, 48.0f, 57.0f);
    spawnSchool(1, glm::vec3(-15.0f, 56.0f, -10.0f), 18.0f, 45, 0.007f, 52.0f, 60.0f);
    spawnSchool(0, glm::vec3(15.0f, 52.0f, -15.0f),  20.0f, 35, 0.005f, 46.0f, 56.0f);

    // === NOWE DODATKOWE ŁAWICE ===
    spawnSchool(1, glm::vec3(40.0f, 54.0f, 10.0f),   15.0f, 40, 0.008f, 48.0f, 58.0f);
    spawnSchool(0, glm::vec3(-35.0f, 56.0f, -30.0f),  22.0f, 45, 0.006f, 50.0f, 60.0f);
    spawnSchool(1, glm::vec3(5.0f, 58.0f, 45.0f),    18.0f, 50, 0.007f, 53.0f, 62.0f);

    // === GŁĘBINY — wolniejsze, tajemnicze ===
    spawnSchool(0, glm::vec3(5.0f, 46.0f, -5.0f),   25.0f, 30, 0.003f, 42.0f, 50.0f);
    spawnSchool(0, glm::vec3(-10.0f, 44.0f, 0.0f),  20.0f, 25, 0.002f, 40.0f, 48.0f);

    std::cout << "FishManager: zainicjalizowano " << fishes.size() << " ryb" << std::endl;
}

void FishManager::update(float deltaTime, const glm::vec3& cameraPos, bool feedingMode) {
    const float FLEE_RADIUS   = 18.0f;  // Odległość paniki
    const float PANIC_BUILDUP = 4.0f;   // Szybkość narastania paniki (0 do 1 w 0.25s)
    const float PANIC_DECAY   = 0.5f;   // Szybkość opadania paniki (1 do 0 w 2s)

    for (auto& fish : fishes) {
        if (fish.frames.empty()) continue;

        int frameIdx = glm::clamp((int)(fish.t * (fish.frames.size() - 1)), 0, (int)fish.frames.size() - 1);
        const PTFrame& frame = fish.frames[frameIdx];
        
        glm::vec3 fishWorldPos = frame.position
            + (-frame.binormal) * fish.localOffset.y
            + (-frame.normal)   * fish.localOffset.z;
        
        float distToCamera = glm::distance(fishWorldPos, cameraPos);
        float targetPanic = 0.0f;

        if (cameraPos.y < 64.0f) {
            glm::vec3 toFish = fishWorldPos - cameraPos;
            if (distToCamera > 0.001f) {
                toFish /= distToCamera;
            }
            float dotProduct = glm::dot(cameraFront, toFish);
            
            // Ryby płoszą się na widok kamery (gdy są przed nią w odległości < 30m)
            // lub gdy kamera jest bardzo blisko (< 10m) z dowolnej strony
            const float sightDistance = 30.0f;
            const float proximityDistance = 10.0f;
            if ((dotProduct > 0.5f && distToCamera < sightDistance) || distToCamera < proximityDistance) {
                float maxDist = (dotProduct > 0.5f) ? sightDistance : proximityDistance;
                float basePanic = 1.0f - (distToCamera / maxDist);
                targetPanic = glm::clamp(basePanic, 0.0f, 1.0f);
            }
        }
        
        // Płynna interpolacja aktualnego poziomu paniki ryby
        if (targetPanic > fish.panicLevel) {
            fish.panicLevel += PANIC_BUILDUP * deltaTime;
            if (fish.panicLevel > targetPanic) fish.panicLevel = targetPanic;
        } else {
            fish.panicLevel -= PANIC_DECAY * deltaTime;
            if (fish.panicLevel < targetPanic) fish.panicLevel = targetPanic;
        }

        // Predkość zależy od paniki (max 6x szybciej)
        float currentFleeMult = 1.0f + (5.0f * fish.panicLevel);

        // Advance along path (globally slowed down to swim slower and more gracefully)
        fish.t += fish.speed * 0.25f * currentFleeMult * deltaTime;
        if (fish.t >= 1.0f) fish.t -= 1.0f;
        if (fish.t < 0.0f)  fish.t += 1.0f;
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

        // Get current frame with smooth interpolation
        float floatIdx = fish.t * (fish.frames.size() - 1);
        int frameIdx = glm::clamp((int)floatIdx, 0, (int)fish.frames.size() - 2);
        float frac = floatIdx - frameIdx;

        const PTFrame& f1 = fish.frames[frameIdx];
        const PTFrame& f2 = fish.frames[frameIdx + 1];

        // Spherical interpolation for vectors, linear for position
        glm::vec3 interpPos = glm::mix(f1.position, f2.position, frac);
        glm::vec3 interpTan = glm::normalize(glm::mix(f1.tangent, f2.tangent, frac));
        glm::vec3 interpNorm = glm::normalize(glm::mix(f1.normal, f2.normal, frac));
        glm::vec3 interpBinorm = glm::normalize(glm::mix(f1.binormal, f2.binormal, frac));

        // Build model matrix from interpolated frame + scale (increased size)
        float scale = (fish.speciesIndex == 0) ? 3.0f : 2.0f;
        
        glm::vec3 finalPos = interpPos;
        if (fish.panicLevel > 0.01f) {
            glm::vec3 prePanicWorldPos = interpPos 
                + (-interpBinorm) * fish.localOffset.y 
                + (-interpNorm) * fish.localOffset.z;
            
            glm::vec3 fleeDir = prePanicWorldPos - cameraPos;
            fleeDir.y *= 0.2f;
            if (glm::length(fleeDir) > 0.001f) {
                fleeDir = glm::normalize(fleeDir);
            }
            glm::vec3 escapeDir = glm::normalize(fleeDir * 0.7f + fish.scatterDir * 0.3f);
            finalPos += escapeDir * (fish.panicLevel * 25.0f); // Zwiększony odskok do 25m
        }

        // OBJ model: nose along -X (swims forward), dorsal fin along -Y (fix upside-down)
        // W PTF normal to wektor w poziomie, a binormal to wektor w pionie (w dół).
        // Aby ryba pływała pionowo (grzbietem do góry), oś Y (góra ryby) musi mapować się na -binormal.
        glm::mat4 model = glm::mat4(
            glm::vec4(-interpTan,    0.0f),  // X = -tangent (kierunek przodu)
            glm::vec4(-interpBinorm, 0.0f),  // Y = -binormal (kierunek góry)
            glm::vec4(-interpNorm,   0.0f),  // Z = -normal (prawy bok, dla zachowania prawoskrętności)
            glm::vec4(finalPos,      1.0f)
        );
        
        // Zastosuj lokalny offset (ryby trzymają się równolegle do głównej ścieżki ławicy)
        model = glm::translate(model, fish.localOffset);

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

        // Get current frame with smooth interpolation
        float floatIdx = fish.t * (fish.frames.size() - 1);
        int frameIdx = glm::clamp((int)floatIdx, 0, (int)fish.frames.size() - 2);
        float frac = floatIdx - frameIdx;

        const PTFrame& f1 = fish.frames[frameIdx];
        const PTFrame& f2 = fish.frames[frameIdx + 1];

        glm::vec3 interpPos = glm::mix(f1.position, f2.position, frac);
        glm::vec3 interpTan = glm::normalize(glm::mix(f1.tangent, f2.tangent, frac));
        glm::vec3 interpNorm = glm::normalize(glm::mix(f1.normal, f2.normal, frac));
        glm::vec3 interpBinorm = glm::normalize(glm::mix(f1.binormal, f2.binormal, frac));

        float scale = (fish.speciesIndex == 0) ? 3.0f : 2.0f;
        glm::vec3 finalPos = interpPos;
        if (fish.panicLevel > 0.01f) {
            glm::vec3 prePanicWorldPos = interpPos 
                + (-interpBinorm) * fish.localOffset.y 
                + (-interpNorm) * fish.localOffset.z;
            
            glm::vec3 fleeDir = prePanicWorldPos - cameraPos;
            fleeDir.y *= 0.2f;
            if (glm::length(fleeDir) > 0.001f) {
                fleeDir = glm::normalize(fleeDir);
            }
            glm::vec3 escapeDir = glm::normalize(fleeDir * 0.7f + fish.scatterDir * 0.3f);
            finalPos += escapeDir * (fish.panicLevel * 25.0f);
        }

        glm::mat4 model = glm::mat4(
            glm::vec4(-interpTan,    0.0f),
            glm::vec4(-interpBinorm, 0.0f),
            glm::vec4(-interpNorm,   0.0f),
            glm::vec4(finalPos,      1.0f)
        );
        model = glm::translate(model, fish.localOffset);

        model = glm::scale(model, glm::vec3(scale));

        glUniformMatrix4fv(glGetUniformLocation(shadowShader, "model"), 1, GL_FALSE, glm::value_ptr(model));

        glBindVertexArray(sp.vao);
        glDrawArrays(GL_TRIANGLES, 0, sp.vertexCount);
    }
}
