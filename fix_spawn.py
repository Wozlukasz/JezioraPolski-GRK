import re

with open('src/PlantManager.cpp', 'r') as f:
    content = f.read()

# Fix spawning loop errors
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

if old_spawn in content:
    content = content.replace(old_spawn, new_spawn)
    with open('src/PlantManager.cpp', 'w') as f:
        f.write(content)
    print("Fixed spawn block successfully.")
else:
    print("Could not find the spawn block!")
    
# Fix one remaining species.flatLOD in render:
old_render_part = """                bool useDetail = species.flatLOD.valid ? (dist < LOD_THRESHOLD) : (dist < currentRenderDist + currentChunkRadius);"""
new_render_part = """                bool useDetail = var.hasFlatLOD ? (dist < currentLodThreshold) : (dist < currentRenderDist + currentChunkRadius);"""
if old_render_part in content:
    content = content.replace(old_render_part, new_render_part)
    with open('src/PlantManager.cpp', 'w') as f:
        f.write(content)
    print("Fixed render block successfully.")

old_render_part2 = """        if (species.flatLOD.valid) {
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, species.flatLOD.textureDiffuse);
            
            for (const auto& chunk : species.flatLOD.chunks) {"""
new_render_part2 = """        // PASS 2: Flat billboardy (chunki dalsze, ale w zasięgu mgły)
        glActiveTexture(GL_TEXTURE0);
        for (auto& var : species.variants) {
            if (!var.hasFlatLOD) continue;
            
            glBindTexture(GL_TEXTURE_2D, var.flatTexture);
            for (const auto& chunk : var.chunks) {"""
if old_render_part2 in content:
    content = content.replace(old_render_part2, new_render_part2)
    with open('src/PlantManager.cpp', 'w') as f:
        f.write(content)
    print("Fixed render block 2 successfully.")

