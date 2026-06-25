import re

with open('main.cpp', 'r') as f:
    content = f.read()

# 1. Create Reflection FBO before the main loop
reflection_fbo_code = """
    // Reflection Map FBO
    const unsigned int REFLECTION_WIDTH = 1024, REFLECTION_HEIGHT = 1024;
    unsigned int reflectionFBO;
    glGenFramebuffers(1, &reflectionFBO);
    unsigned int reflectionTex;
    glGenTextures(1, &reflectionTex);
    glBindTexture(GL_TEXTURE_2D, reflectionTex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, REFLECTION_WIDTH, REFLECTION_HEIGHT, 0, GL_RGB, GL_UNSIGNED_BYTE, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glBindFramebuffer(GL_FRAMEBUFFER, reflectionFBO);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, reflectionTex, 0);

    unsigned int reflectionRBO;
    glGenRenderbuffers(1, &reflectionRBO);
    glBindRenderbuffer(GL_RENDERBUFFER, reflectionRBO);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, REFLECTION_WIDTH, REFLECTION_HEIGHT);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, reflectionRBO);
    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
        std::cout << "ERROR::FRAMEBUFFER:: Reflection FBO is not complete!" << std::endl;
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    // Uniformy clipPlane dla shaderów (aby ucinać modele pod wodą w trakcie renderowania odbicia)
    GLint loc_terrain_clipY = glGetUniformLocation(terrainShader, "clipY");
    GLint loc_terrain_clipMode = glGetUniformLocation(terrainShader, "clipMode");
    GLint loc_plant_clipY = glGetUniformLocation(plantShader, "clipY");
    GLint loc_plant_clipMode = glGetUniformLocation(plantShader, "clipMode");

"""

target_insert_fbo = "    // Bubble emission timer"
if reflection_fbo_code not in content:
    content = content.replace(target_insert_fbo, reflection_fbo_code + target_insert_fbo)

# 2. Add Reflection Pass rendering
render_pass_target = """        // 1. Shadow mapping pass"""
reflection_pass_code = """
        // ==================== REFLECTION PASS ====================
        if (cameraPos.y > 63.0f) { // Renderuj odbicie tylko gdy jesteśmy nad wodą
            glBindFramebuffer(GL_FRAMEBUFFER, reflectionFBO);
            glViewport(0, 0, REFLECTION_WIDTH, REFLECTION_HEIGHT);
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

            // Odwrócona kamera (y względem tafli 64.0)
            float distToWater = cameraPos.y - 64.0f;
            glm::vec3 refCamPos = cameraPos;
            refCamPos.y -= 2.0f * distToWater;
            
            // Front wektor - pitch jest odwrócony
            glm::vec3 refCamFront = cameraFront;
            refCamFront.y = -refCamFront.y;
            
            glm::mat4 refView = glm::lookAt(refCamPos, refCamPos + refCamFront, glm::vec3(0.0f, 1.0f, 0.0f));
            glm::mat4 refVpMatrix = projection * refView;

            // Renderowanie Skyboxa w odbiciu
            glDepthFunc(GL_LEQUAL);
            glUseProgram(skyboxShader);
            glUniformMatrix4fv(loc_skybox_view, 1, GL_FALSE, glm::value_ptr(glm::mat4(glm::mat3(refView))));
            glUniformMatrix4fv(loc_skybox_projection, 1, GL_FALSE, glm::value_ptr(projection));
            glUniform3fv(loc_skybox_viewPos, 1, glm::value_ptr(refCamPos));
            glUniform1i(loc_skybox_useCubemap, 0);
            glBindVertexArray(skyboxVAO);
            glDrawArrays(GL_TRIANGLES, 0, 36);
            glDepthFunc(GL_LESS);

            // Renderowanie Terenu w odbiciu (z clippingiem)
            glUseProgram(terrainShader);
            glUniformMatrix4fv(loc_terrain_view, 1, GL_FALSE, glm::value_ptr(refView));
            glUniform3fv(loc_terrain_viewPos, 1, glm::value_ptr(refCamPos));
            glUniform1f(loc_terrain_clipY, 64.0f);
            glUniform1i(loc_terrain_clipMode, 1); // Włącz clip dla odbicia
            
            glActiveTexture(GL_TEXTURE0); glBindTexture(GL_TEXTURE_2D, splatMapTex);
            glActiveTexture(GL_TEXTURE1); glBindTexture(GL_TEXTURE_2D, mudTex);
            glActiveTexture(GL_TEXTURE2); glBindTexture(GL_TEXTURE_2D, soilTex);
            glActiveTexture(GL_TEXTURE3); glBindTexture(GL_TEXTURE_2D, grassTex);
            glActiveTexture(GL_TEXTURE4); glBindTexture(GL_TEXTURE_2D, mudNorm);
            glActiveTexture(GL_TEXTURE5); glBindTexture(GL_TEXTURE_2D, soilNorm);
            glActiveTexture(GL_TEXTURE6); glBindTexture(GL_TEXTURE_2D, grassNorm);
            glActiveTexture(GL_TEXTURE7); glBindTexture(GL_TEXTURE_2D, mudRough);
            glActiveTexture(GL_TEXTURE8); glBindTexture(GL_TEXTURE_2D, grassRough);
            glActiveTexture(GL_TEXTURE9); glBindTexture(GL_TEXTURE_2D, depthMap);
            
            glBindVertexArray(terrainVAO);
            glDrawArrays(GL_TRIANGLES, 0, globalTerrainVertices.size());

            // Renderowanie Roślin (Drzewa itp.) w odbiciu
            glUseProgram(plantShader);
            glUniformMatrix4fv(loc_plant_view, 1, GL_FALSE, glm::value_ptr(refView));
            glUniform3fv(loc_plant_viewPos, 1, glm::value_ptr(refCamPos));
            glUniform1f(loc_plant_clipY, 64.0f);
            glUniform1i(loc_plant_clipMode, 1); // Włącz clip
            
            glActiveTexture(GL_TEXTURE1); glBindTexture(GL_TEXTURE_2D, depthMap);
            plantManager.render(plantShader, refCamPos, refVpMatrix);
        }

        // Przywracamy framebuffer, wyłączamy clipping
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glUseProgram(terrainShader); glUniform1i(loc_terrain_clipMode, 0);
        glUseProgram(plantShader); glUniform1i(loc_plant_clipMode, 0);
        // =========================================================

        // 1. Shadow mapping pass"""

if "REFLECTION PASS" not in content:
    content = content.replace(render_pass_target, reflection_pass_code)

# 3. Add reflectionTex to Water shader binding
water_bind_target = """        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, waterTex);"""
water_bind_new = """        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, waterTex);
        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, reflectionTex);
        glUniform1i(glGetUniformLocation(waterShader, "reflectionTex"), 1);"""
        
if "reflectionTex" not in content.split("glUseProgram(waterShader);")[-1]:
    content = content.replace(water_bind_target, water_bind_new)

with open('main.cpp', 'w') as f:
    f.write(content)
