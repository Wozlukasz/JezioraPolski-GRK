import re

with open('src/PlantManager.cpp', 'r') as f:
    content = f.read()

# 1. Update loadSpecies signature
content = content.replace(
    'const std::string& flatModelPath, const std::string& flatTexPath, bool flipMainTex',
    'const std::vector<std::string>& flatModelPaths, const std::vector<std::string>& flatTexPaths, bool flipMainTex'
)

# 2. Update Flat LOD loading in loadSpecies
old_load = """    // Wczytaj flat (billboard) model jeśli podano
    if (!flatModelPath.empty()) {
        std::vector<Vertex> flatVerts;
        if (loadOBJ(findAssetPath(flatModelPath), flatVerts)) {
            setupBaseVBO(flatVerts, species.flatLOD.baseVBO);
            species.flatLOD.vertexCount = flatVerts.size();
            if (!flatTexPath.empty()) {
                species.flatLOD.textureDiffuse = loadTexture(findAssetPath(flatTexPath).c_str(), true, true); // flip=true, clamp=true
            } else {
                species.flatLOD.textureDiffuse = species.textureDiffuse;
            }
            species.flatLOD.valid = true;
            std::cout << "  -> Flat LOD loaded: " << flatModelPath << " (" << flatVerts.size() / 3 << " tris)" << std::endl;
        }
    }"""
new_load = """    // Wczytaj flat modele jeśli podano (jako fallback dla wszystkich, lub per-wariant)
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
    }"""
content = content.replace(old_load, new_load)

# 3. Update chunked matrices
content = content.replace(
    '    std::map<std::pair<int, int>, std::vector<glm::mat4>> flatChunkedMatrices;',
    '    std::vector<std::map<std::pair<int, int>, std::vector<glm::mat4>>> variantFlatChunkedMatrices(species.variants.size());'
)
content = content.replace(
    '// Flat LOD zbiera WSZYSTKIE instancje (niezależnie od wariantu) do jednego flat mesha\n',
    ''
)

# 4. Density & scaling
content = content.replace(
    'int numSpawnAttempts = hasTuft ? 80000 : 400000;',
    'int numSpawnAttempts = hasTuft ? 80000 : 400000;\n    if (name == "Tatarak") numSpawnAttempts = 6000000;'
)
old_mask = """        if (maskVal > 20) {
            float prob = maskVal / 255.0f;
            // Drzewa (brak maski) - brzegi są wąskie, ale użytkownik prosił o dziesięciokrotne zmniejszenie gęstości sosen
            if (maskPath.empty()) prob = 0.1f; 
            
            std::uniform_real_distribution<float> probDist(0.0f, 1.0f);"""
new_mask = """        if (maskVal > 20) {
            float prob = maskVal / 255.0f;
            if (maskPath.empty()) prob = 0.1f; 
            
            if (name == "Tatarak") prob = 1.0f;
            
            std::uniform_real_distribution<float> probDist(0.0f, 1.0f);"""
content = content.replace(old_mask, new_mask)

# 5. Spawning logic replacement
old_spawn = """                    int chunkX = std::floor(worldX / 10.0f);
                    int chunkZ = std::floor(worldZ / 10.0f);
                    variantChunkedMatrices[varIdx][{chunkX, chunkZ}].push_back(m);
                    
                    // Flat LOD na DOKŁADNIE tej samej pozycji co detal
                    if (species.flatLOD.valid) {
                        // Uproszczona macierz (bez obrotu elementów kępki — flat model sam ma krzyżyk)
                        glm::mat4 flatM = glm::translate(glm::mat4(1.0f), glm::vec3(worldX, currentY, worldZ));
                        flatM = glm::rotate(flatM, glm::radians(tuftRot), glm::vec3(0,1,0));
                        flatM = glm::scale(flatM, currentScaleVec);
                        flatChunkedMatrices[{chunkX, chunkZ}].push_back(flatM);
                    }
                }
            } else {
                int varIdx = engine() % species.variants.size();
                glm::vec3 currentScaleVec = glm::vec3(randomScaleXZ(engine), randomScaleY(engine), randomScaleXZ(engine)) * scaleMult;

                float currentY = td.height;
                
                // Nie chcemy by podwodne rośliny wystawały ponad lustro wody (63.5f)
                // Omijamy jednak rośliny rosnące nad wodą / na brzegu!
                if (name != "Tatarak" && name != "Drzewo (Sosna)" && name != "Osoka Brzeg") {
                    float topY = currentY + currentScaleVec.y * 1.5f;
                    if (topY > 63.5f) {
                        currentY -= (topY - 63.5f);
                    }
                }
                
                // Drzewa minimalnie latają, więc obniżamy je o metr w dół
                if (name == "Drzewo (Sosna)") {
                    currentY -= 1.0f;
                }

                glm::mat4 m = glm::translate(glm::mat4(1.0f), glm::vec3(x, currentY, z));
                m = glm::rotate(m, glm::radians(rotDist(engine)), glm::vec3(0,1,0));
                m = glm::scale(m, currentScaleVec);
                
                int chunkX = std::floor(x / 10.0f);
                int chunkZ = std::floor(z / 10.0f);
                variantChunkedMatrices[varIdx][{chunkX, chunkZ}].push_back(m);
                
                // Dodaj flat LOD instancję
                if (species.flatLOD.valid) {
                    flatChunkedMatrices[{chunkX, chunkZ}].push_back(m);
                }
            }
        }
    }

    // Zbuduj chunki dla szczegółowych wariantów
    for (size_t i = 0; i < species.variants.size(); ++i) {
        for (const auto& pair : variantChunkedMatrices[i]) {
            PlantChunk chunk;
            chunk.instanceCount = pair.second.size();
            float cx = pair.first.first * 10.0f + 5.0f;
            float cz = pair.first.second * 10.0f + 5.0f;
            float cy = getTerrainHeight(cx, cz);
            if (cy < -900.0f) cy = -10.0f;
            chunk.center = glm::vec3(cx, cy, cz);
            setupChunkVAO(species.variants[i].baseVBO, pair.second, chunk.vao, chunk.instVBO);
            species.variants[i].chunks.push_back(chunk);
        }
    }

    // Zbuduj chunki dla flat LOD
    if (species.flatLOD.valid) {
        for (const auto& pair : flatChunkedMatrices) {
            PlantChunk chunk;
            chunk.instanceCount = pair.second.size();
            float cx = pair.first.first * 10.0f + 5.0f;
            float cz = pair.first.second * 10.0f + 5.0f;
            float cy = getTerrainHeight(cx, cz);
            if (cy < -900.0f) cy = -10.0f;
            chunk.center = glm::vec3(cx, cy, cz);
            setupChunkVAO(species.flatLOD.baseVBO, pair.second, chunk.vao, chunk.instVBO);
            species.flatLOD.chunks.push_back(chunk);
        }
        std::cout << "  -> Flat LOD chunks: " << species.flatLOD.chunks.size() << std::endl;
    }"""
new_spawn = """                    int chunkX = std::floor(worldX / 10.0f); int chunkZ = std::floor(worldZ / 10.0f);
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
    }"""
content = content.replace(old_spawn, new_spawn)

# 6. Init updates
content = content.replace('"Assets/flat-models/objects/moczarka-delikatna-flat.obj",\n                "Assets/flat-models/renders/moczarka-delikatna/moczarka-delikatna-flat0.png"', '{"Assets/flat-models/objects/moczarka-delikatna-flat.obj"},\n                {"Assets/flat-models/renders/moczarka-delikatna/moczarka-delikatna-flat0.png"}')
content = content.replace('"Assets/flat-models/objects/mech-zdrojek-flat.obj",\n                "Assets/flat-models/renders/mech-zdrojek/mech-zdrojek-flat0.png"', '{"Assets/flat-models/objects/mech-zdrojek-flat.obj"},\n                {"Assets/flat-models/renders/mech-zdrojek/mech-zdrojek-flat0.png"}')
content = content.replace('"Assets/flat-models/objects/moczarka-kanadyjska-flat.obj",\n                "Assets/flat-models/renders/moczarka-kanadyjska/moczarka-kanadyjska-flat0.png"', '{"Assets/flat-models/objects/moczarka-kanadyjska-flat.obj"},\n                {"Assets/flat-models/renders/moczarka-kanadyjska/moczarka-kanadyjska-flat0.png"}')
content = content.replace('"Assets/flat-models/objects/rogatek-sztywny-flat.obj",\n                "Assets/flat-models/renders/rogatek-sztywny/rogatek-sztywny-flat0.png"', '{"Assets/flat-models/objects/rogatek-sztywny-flat.obj"},\n                {"Assets/flat-models/renders/rogatek-sztywny/rogatek-sztywny-flat0.png"}')
content = content.replace('"Assets/flat-models/objects/rogatek-krótkoszyjkowy-flat.obj",\n                "Assets/flat-models/renders/rogatek-krótkoszyjkowy/rogatek-krótkoszyjkowy-flat0.png"', '{"Assets/flat-models/objects/rogatek-krótkoszyjkowy-flat.obj"},\n                {"Assets/flat-models/renders/rogatek-krótkoszyjkowy/rogatek-krótkoszyjkowy-flat0.png"}')

content = content.replace('"Assets/flat-models/objects/tatarak1-flat.obj",\n                "Assets/flat-models/renders/tatarak/tatarak1-0.png"', '{"Assets/flat-models/objects/tatarak1-flat.obj", "Assets/flat-models/objects/tatarak6-flat.obj", "Assets/flat-models/objects/tatarak7-flat.obj"},\n                {"Assets/flat-models/renders/tatarak/tatarak1-0.png", "Assets/flat-models/renders/tatarak/tatarak6-0.png", "Assets/flat-models/renders/tatarak/tatarak7-0.png"}')
content = content.replace('{"Assets/models/drzewo/drzewo.obj"}, 22.0f, "", "", true);', '{"Assets/models/drzewo/drzewo.obj"}, 22.0f, {}, {}, true);')

# 7. Render updates
old_render = """        bool smoothLOD = (species.name == "Tatarak");
        
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
                
                bool useDetail = species.flatLOD.valid ? (dist < currentLodThreshold) : (dist < currentRenderDist + currentChunkRadius);
                
                if (useDetail && inFrustum) {
                    // Wodorosty (brak smoothLOD) mają twarde cięcie (0). Tataraki mają płynne zanikanie (1).
                    glUniform1i(loc_lodFadeMode, smoothLOD ? 1 : 0); 
                    glBindVertexArray(chunk.vao);
                    glDrawArraysInstanced(GL_TRIANGLES, 0, var.vertexCount, chunk.instanceCount);
                }
            }
        }
        
        // PASS 2: Flat billboardy (chunki dalsze, ale w zasięgu mgły)
        if (species.flatLOD.valid) {
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, species.flatLOD.textureDiffuse);
            
            for (const auto& chunk : species.flatLOD.chunks) {
                glm::vec2 chunkPos2D(chunk.center.x, chunk.center.z);
                float dist = glm::distance(chunkPos2D, camPos2D);
                
                bool inFrustum = sphereInFrustum(frustumPlanes, chunk.center, CHUNK_RADIUS);
                // Flat LOD: render od progu LOD do końca widoczności
                bool useFlat = (dist >= currentLodThreshold) && (dist < RENDER_DIST + CHUNK_RADIUS);
                
                if (useFlat && inFrustum) {
                    // Tataraki powoli się wyłaniają (2), wodorosty pojawiają się twardo na 100% (0)
                    glUniform1i(loc_lodFadeMode, smoothLOD ? 2 : 0); 
                    glBindVertexArray(chunk.vao);
                    glDrawArraysInstanced(GL_TRIANGLES, 0, species.flatLOD.vertexCount, chunk.instanceCount);
                }
            }
        }"""
new_render = """        bool smoothLOD = false; 
        
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
                // Drzewa i Tataraki nie znikają bez względu na dystans (widoczne zawsze)
                if (species.name == "Drzewo (Sosna)" || species.name == "Tatarak") currentRenderDist = 999999.0f;
                
                bool useDetail = var.hasFlatLOD ? (dist < currentLodThreshold) : (dist < currentRenderDist + currentChunkRadius);
                
                if (useDetail && inFrustum) {
                    // Wodorosty (brak smoothLOD) mają twarde cięcie (0). Tataraki mają płynne zanikanie (1).
                    glUniform1i(loc_lodFadeMode, smoothLOD ? 1 : 0); 
                    glBindVertexArray(chunk.vao);
                    glDrawArraysInstanced(GL_TRIANGLES, 0, var.vertexCount, chunk.instanceCount);
                }
            }
        }
        
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
                float currentFlatRenderDist = RENDER_DIST;
                if (species.name == "Tatarak" || species.name == "Drzewo (Sosna)") currentFlatRenderDist = 999999.0f;
                
                bool useFlat = (dist >= currentLodThreshold) && (dist < currentFlatRenderDist + CHUNK_RADIUS);
                
                if (useFlat && inFrustum) {
                    // lodFadeMode = 3 oznacza twarde cięcie dla FLAT modeli
                    glUniform1i(loc_lodFadeMode, smoothLOD ? 2 : 3); 
                    glBindVertexArray(chunk.flatVAO);
                    glDrawArraysInstanced(GL_TRIANGLES, 0, var.flatVertexCount, chunk.instanceCount);
                }
            }
        }"""
content = content.replace(old_render, new_render)

# 8. Render shadow updates
old_shadow = """    for (auto& species : speciesList) {
        if (species.flatLOD.valid) {
            // Używamy zoptymalizowanego płaskiego modelu do rzucania cieni (uproszczony cień)
            for (const auto& chunk : species.flatLOD.chunks) {
                glm::vec2 chunkPos2D(chunk.center.x, chunk.center.z);
                float dist = glm::distance(chunkPos2D, camPos2D);
                if (dist < SHADOW_DIST + CHUNK_RADIUS &&
                    sphereInFrustum(frustumPlanes, chunk.center, CHUNK_RADIUS * 2.0f)) {
                    
                    int drawInstanceCount = chunk.instanceCount / 3;
                    if (drawInstanceCount < 1) drawInstanceCount = 1;

                    glBindVertexArray(chunk.vao);
                    glDrawArraysInstanced(GL_TRIANGLES, 0, species.flatLOD.vertexCount, drawInstanceCount);
                }
            }
        } else {
            // Fallback dla roślin bez płaskiego modelu (np. Tatarak, Osoka)
            for (auto& var : species.variants) {
                for (const auto& chunk : var.chunks) {
                    glm::vec2 chunkPos2D(chunk.center.x, chunk.center.z);
                    float dist = glm::distance(chunkPos2D, camPos2D);
                    if (dist < SHADOW_DIST + CHUNK_RADIUS &&
                        sphereInFrustum(frustumPlanes, chunk.center, CHUNK_RADIUS * 2.0f)) {
                        
                        int drawInstanceCount = chunk.instanceCount / 3;
                        if (drawInstanceCount < 1) drawInstanceCount = 1;

                        glBindVertexArray(chunk.vao);
                        glDrawArraysInstanced(GL_TRIANGLES, 0, var.vertexCount, drawInstanceCount);
                    }
                }
            }
        }
    }"""
new_shadow = """    for (auto& species : speciesList) {
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
    }"""
content = content.replace(old_shadow, new_shadow)

with open('src/PlantManager.cpp', 'w') as f:
    f.write(content)

