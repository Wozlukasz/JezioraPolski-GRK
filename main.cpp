#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <iostream>
#include <vector>

#include "Utils.h"
#include "Texture.h"
#include "Model.h"
#include "Shader.h"
#include "Camera.h"
#include "PlantManager.h"
#include "stb_image.h"

std::vector<Vertex> globalTerrainVertices;

void framebuffer_size_callback(GLFWwindow* window, int width, int height) {
    glViewport(0, 0, width, height);
}

int main() {
    if (!glfwInit()) return -1;
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);

    GLFWwindow* window = glfwCreateWindow(1024, 768, "Lake Strzeszynek 3D Viewer", NULL, NULL);
    if (!window) { glfwTerminate(); return -1; }
    glfwMakeContextCurrent(window);
    glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);
    glfwSetCursorPosCallback(window, mouse_callback);
    glfwSetKeyCallback(window, key_callback);
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);

    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) return -1;

    glEnable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    // Shadery
    unsigned int terrainShader = createShaderProgramFromFiles("Assets/shaders/terrain.vert", "Assets/shaders/terrain.frag");
    unsigned int waterShader = createShaderProgramFromFiles("Assets/shaders/water.vert", "Assets/shaders/water.frag");
    unsigned int skyboxShader = createShaderProgramFromFiles("Assets/shaders/skybox.vert", "Assets/shaders/skybox.frag");
    unsigned int shadowShader = createShaderProgramFromFiles("Assets/shaders/shadow.vert", "Assets/shaders/shadow.frag");
    unsigned int plantShader = createShaderProgramFromFiles("Assets/shaders/instanced_plant.vert", "Assets/shaders/instanced_plant.frag");
    unsigned int shadowPlantShader = createShaderProgramFromFiles("Assets/shaders/shadow_instanced.vert", "Assets/shaders/shadow_instanced.frag");

    // Skybox vertices
    float skyboxVertices[] = {
        -1.0f,  1.0f, -1.0f, -1.0f, -1.0f, -1.0f,  1.0f, -1.0f, -1.0f,  1.0f, -1.0f, -1.0f,  1.0f,  1.0f, -1.0f, -1.0f,  1.0f, -1.0f,
        -1.0f, -1.0f,  1.0f, -1.0f, -1.0f, -1.0f, -1.0f,  1.0f, -1.0f, -1.0f,  1.0f, -1.0f, -1.0f,  1.0f,  1.0f, -1.0f, -1.0f,  1.0f,
         1.0f, -1.0f, -1.0f,  1.0f, -1.0f,  1.0f,  1.0f,  1.0f,  1.0f,  1.0f,  1.0f,  1.0f,  1.0f,  1.0f, -1.0f,  1.0f, -1.0f, -1.0f,
        -1.0f, -1.0f,  1.0f, -1.0f,  1.0f,  1.0f,  1.0f,  1.0f,  1.0f,  1.0f,  1.0f,  1.0f,  1.0f, -1.0f,  1.0f, -1.0f, -1.0f,  1.0f,
        -1.0f,  1.0f, -1.0f,  1.0f,  1.0f, -1.0f,  1.0f,  1.0f,  1.0f,  1.0f,  1.0f,  1.0f, -1.0f,  1.0f,  1.0f, -1.0f,  1.0f, -1.0f,
        -1.0f, -1.0f, -1.0f, -1.0f, -1.0f,  1.0f,  1.0f, -1.0f, -1.0f,  1.0f, -1.0f, -1.0f, -1.0f, -1.0f,  1.0f,  1.0f, -1.0f,  1.0f
    };
    unsigned int skyboxVAO, skyboxVBO;
    glGenVertexArrays(1, &skyboxVAO);
    glGenBuffers(1, &skyboxVBO);
    glBindVertexArray(skyboxVAO);
    glBindBuffer(GL_ARRAY_BUFFER, skyboxVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(skyboxVertices), &skyboxVertices, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);

    // Terrain & Water
    std::vector<Vertex> waterVertices;
    loadOBJ(findAssetPath("Assets/models/strzeszynek/teren-jezioro.obj"), globalTerrainVertices);
    loadOBJ(findAssetPath("Assets/models/strzeszynek/woda-jezioro.obj"), waterVertices);

    // Ustaw kamerę na poziomie terenu (oko człowieka = +2m)
    float spawnHeight = getTerrainHeight(cameraPos.x, cameraPos.z);
    if (spawnHeight > -900.0f) {
        cameraPos.y = spawnHeight + 2.0f;
    }

    unsigned int terrainVAO, terrainVBO;
    glGenVertexArrays(1, &terrainVAO);
    glGenBuffers(1, &terrainVBO);
    glBindVertexArray(terrainVAO);
    glBindBuffer(GL_ARRAY_BUFFER, terrainVBO);
    glBufferData(GL_ARRAY_BUFFER, globalTerrainVertices.size() * sizeof(Vertex), globalTerrainVertices.data(), GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, position)); glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, normal)); glEnableVertexAttribArray(1);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, texCoords)); glEnableVertexAttribArray(2);
    glVertexAttribPointer(3, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, tangent)); glEnableVertexAttribArray(3);
    glVertexAttribPointer(4, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, bitangent)); glEnableVertexAttribArray(4);

    unsigned int waterVAO, waterVBO;
    glGenVertexArrays(1, &waterVAO);
    glGenBuffers(1, &waterVBO);
    glBindVertexArray(waterVAO);
    glBindBuffer(GL_ARRAY_BUFFER, waterVBO);
    glBufferData(GL_ARRAY_BUFFER, waterVertices.size() * sizeof(Vertex), waterVertices.data(), GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, position)); glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, normal)); glEnableVertexAttribArray(1);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, texCoords)); glEnableVertexAttribArray(2);

    // Plant Manager - Instancing, Tufts & Splatmaps
    PlantManager plantManager;
    plantManager.init();

    // Shadow Map FBO
    const unsigned int SHADOW_WIDTH = 2048, SHADOW_HEIGHT = 2048;
    unsigned int depthMapFBO;
    glGenFramebuffers(1, &depthMapFBO);
    unsigned int depthMap;
    glGenTextures(1, &depthMap);
    glBindTexture(GL_TEXTURE_2D, depthMap);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT, SHADOW_WIDTH, SHADOW_HEIGHT, 0, GL_DEPTH_COMPONENT, GL_FLOAT, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
    float borderColor[] = { 1.0, 1.0, 1.0, 1.0 };
    glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, borderColor);
    glBindFramebuffer(GL_FRAMEBUFFER, depthMapFBO);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, depthMap, 0);
    glDrawBuffer(GL_NONE); glReadBuffer(GL_NONE);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    // Tekstury PBR Terenu
    stbi_set_flip_vertically_on_load(true);
    unsigned int splatMapTex = loadTexture(findAssetPath("Assets/materials/strzeszynek/strzeszynek-splat-map.png").c_str());
    unsigned int mudTex = loadTexture(findAssetPath("Assets/materials/strzeszynek/teren-jezioro/mud/mud diffuse.bmp").c_str());
    unsigned int soilTex = loadTexture(findAssetPath("Assets/materials/strzeszynek/teren-jezioro/soil/soil-22-diffuse.jpg").c_str());
    unsigned int grassTex = loadTexture(findAssetPath("Assets/materials/strzeszynek/teren-jezioro/grass/PX_Ground_Grass_02_albedo.jpg").c_str());
    unsigned int waterTex = loadTexture(findAssetPath("Assets/materials/strzeszynek/woda-jezioro/woda-jezioro-diffuse.png").c_str());
    unsigned int mudNorm = loadTexture(findAssetPath("Assets/materials/strzeszynek/teren-jezioro/mud/mud normal.bmp").c_str());
    unsigned int soilNorm = loadTexture(findAssetPath("Assets/materials/strzeszynek/teren-jezioro/soil/soil-22-diffuse_normal.jpg").c_str());
    unsigned int grassNorm = loadTexture(findAssetPath("Assets/materials/strzeszynek/teren-jezioro/grass/PX_Ground_Grass_02_normal.jpg").c_str());
    unsigned int mudRough = loadTexture(findAssetPath("Assets/materials/strzeszynek/teren-jezioro/mud/mud roughness.bmp").c_str());
    unsigned int grassRough = loadTexture(findAssetPath("Assets/materials/strzeszynek/teren-jezioro/grass/PX_Ground_Grass_02_roughness.jpg").c_str());

    glUseProgram(terrainShader);
    glUniform1i(glGetUniformLocation(terrainShader, "splatMap"), 0);
    glUniform1i(glGetUniformLocation(terrainShader, "texMud"), 1);
    glUniform1i(glGetUniformLocation(terrainShader, "texSoil"), 2);
    glUniform1i(glGetUniformLocation(terrainShader, "texGrass"), 3);
    glUniform1i(glGetUniformLocation(terrainShader, "normMud"), 4);
    glUniform1i(glGetUniformLocation(terrainShader, "normSoil"), 5);
    glUniform1i(glGetUniformLocation(terrainShader, "normGrass"), 6);
    glUniform1i(glGetUniformLocation(terrainShader, "roughMud"), 7);
    glUniform1i(glGetUniformLocation(terrainShader, "roughGrass"), 8);
    glUniform1i(glGetUniformLocation(terrainShader, "shadowMap"), 9);

    glUseProgram(waterShader);
    glUniform1i(glGetUniformLocation(waterShader, "texWater"), 0);

    glUseProgram(plantShader);
    glUniform1i(glGetUniformLocation(plantShader, "texture_diffuse1"), 0);
    glUniform1i(glGetUniformLocation(plantShader, "shadowMap"), 1);

    glUseProgram(shadowPlantShader);
    glUniform1i(glGetUniformLocation(shadowPlantShader, "texture_diffuse1"), 0);

    glm::vec3 lightDir(0.6f, 0.9f, 0.4f);

    // ---- Cache uniform locations (avoid per-frame string lookups) ----
    GLint loc_shadow_lightSpaceMatrix = glGetUniformLocation(shadowShader, "lightSpaceMatrix");
    GLint loc_shadow_model = glGetUniformLocation(shadowShader, "model");
    GLint loc_shadowPlant_lightSpaceMatrix = glGetUniformLocation(shadowPlantShader, "lightSpaceMatrix");
    
    GLint loc_terrain_model = glGetUniformLocation(terrainShader, "model");
    GLint loc_terrain_view = glGetUniformLocation(terrainShader, "view");
    GLint loc_terrain_projection = glGetUniformLocation(terrainShader, "projection");
    GLint loc_terrain_lightSpaceMatrix = glGetUniformLocation(terrainShader, "lightSpaceMatrix");
    GLint loc_terrain_lightDir = glGetUniformLocation(terrainShader, "lightDir");
    GLint loc_terrain_viewPos = glGetUniformLocation(terrainShader, "viewPos");

    GLint loc_plant_view = glGetUniformLocation(plantShader, "view");
    GLint loc_plant_projection = glGetUniformLocation(plantShader, "projection");
    GLint loc_plant_lightSpaceMatrix = glGetUniformLocation(plantShader, "lightSpaceMatrix");
    GLint loc_plant_lightDir = glGetUniformLocation(plantShader, "lightDir");
    GLint loc_plant_viewPos = glGetUniformLocation(plantShader, "viewPos");

    GLint loc_water_model = glGetUniformLocation(waterShader, "model");
    GLint loc_water_view = glGetUniformLocation(waterShader, "view");
    GLint loc_water_projection = glGetUniformLocation(waterShader, "projection");
    GLint loc_water_lightDir = glGetUniformLocation(waterShader, "lightDir");
    GLint loc_water_viewPos = glGetUniformLocation(waterShader, "viewPos");
    GLint loc_water_time = glGetUniformLocation(waterShader, "time");

    GLint loc_skybox_view = glGetUniformLocation(skyboxShader, "view");
    GLint loc_skybox_projection = glGetUniformLocation(skyboxShader, "projection");

    while (!glfwWindowShouldClose(window)) {
        float currentFrame = static_cast<float>(glfwGetTime());
        deltaTime = currentFrame - lastFrame;
        lastFrame = currentFrame;

        processInput(window);

        glm::mat4 model = glm::mat4(1.0f);

        // Oblicz macierze kamery wcześniej — potrzebne do frustum culling w obu passach
        int width, height; glfwGetFramebufferSize(window, &width, &height);
        glm::mat4 view = glm::translate(glm::mat4_cast(glm::conjugate(cameraOrientation)), -cameraPos);
        float aspect = (height == 0) ? 1.0f : (float)width / (float)height;
        glm::mat4 projection = glm::perspective(glm::radians(fov), aspect, 0.1f, 2000.0f);
        glm::mat4 vpMatrix = projection * view;

        glm::mat4 lightProjection = glm::ortho(-400.0f, 400.0f, -400.0f, 400.0f, 1.0f, 1000.0f);
        glm::vec3 lightPos = cameraPos + normalize(lightDir) * 300.0f; 
        glm::mat4 lightView = glm::lookAt(lightPos, cameraPos, glm::vec3(0.0, 1.0, 0.0));
        glm::mat4 lightSpaceMatrix = lightProjection * lightView;

        // 1. Shadow mapping pass
        glViewport(0, 0, SHADOW_WIDTH, SHADOW_HEIGHT);
        glBindFramebuffer(GL_FRAMEBUFFER, depthMapFBO);
        glClear(GL_DEPTH_BUFFER_BIT);
        
        glUseProgram(shadowShader);
        glUniformMatrix4fv(loc_shadow_lightSpaceMatrix, 1, GL_FALSE, glm::value_ptr(lightSpaceMatrix));
        glUniformMatrix4fv(loc_shadow_model, 1, GL_FALSE, glm::value_ptr(model));
        glCullFace(GL_FRONT);
        glBindVertexArray(terrainVAO);
        glDrawArrays(GL_TRIANGLES, 0, globalTerrainVertices.size());
        
        // Cienie roślin
        glUseProgram(shadowPlantShader);
        glUniformMatrix4fv(loc_shadowPlant_lightSpaceMatrix, 1, GL_FALSE, glm::value_ptr(lightSpaceMatrix));
        // Tylko rysuj rośliny/cienie jeśli jesteśmy pod wodą
        if (cameraPos.y <= 64.0f) {
            plantManager.renderShadow(shadowPlantShader, cameraPos, vpMatrix);
        }
        
        glEnable(GL_CULL_FACE);
        glCullFace(GL_BACK);
        glBindFramebuffer(GL_FRAMEBUFFER, 0);

        // 2. Normal render pass
        glViewport(0, 0, width, height);
        glClearColor(0.53f, 0.81f, 0.92f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        // Teren
        glUseProgram(terrainShader);
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
        glUniformMatrix4fv(loc_terrain_model, 1, GL_FALSE, glm::value_ptr(model));
        glUniformMatrix4fv(loc_terrain_view, 1, GL_FALSE, glm::value_ptr(view));
        glUniformMatrix4fv(loc_terrain_projection, 1, GL_FALSE, glm::value_ptr(projection));
        glUniformMatrix4fv(loc_terrain_lightSpaceMatrix, 1, GL_FALSE, glm::value_ptr(lightSpaceMatrix));
        glUniform3fv(loc_terrain_lightDir, 1, glm::value_ptr(lightDir));
        glUniform3fv(loc_terrain_viewPos, 1, glm::value_ptr(cameraPos));
        glBindVertexArray(terrainVAO);
        glDrawArrays(GL_TRIANGLES, 0, globalTerrainVertices.size());

        // Rośliny
        glUseProgram(plantShader);
        glActiveTexture(GL_TEXTURE1); glBindTexture(GL_TEXTURE_2D, depthMap);
        glUniformMatrix4fv(loc_plant_view, 1, GL_FALSE, glm::value_ptr(view));
        glUniformMatrix4fv(loc_plant_projection, 1, GL_FALSE, glm::value_ptr(projection));
        glUniformMatrix4fv(loc_plant_lightSpaceMatrix, 1, GL_FALSE, glm::value_ptr(lightSpaceMatrix));
        glUniform3fv(loc_plant_lightDir, 1, glm::value_ptr(lightDir));
        glUniform3fv(loc_plant_viewPos, 1, glm::value_ptr(cameraPos));
        
        glDisable(GL_CULL_FACE); // Wyłącz culling dla dwustronnych liści
        // Rośliny renderowane tylko pod wodą
        if (cameraPos.y <= 64.0f) {
            plantManager.render(plantShader, cameraPos, vpMatrix);
        }
        glEnable(GL_CULL_FACE);

        // Woda
        glUseProgram(waterShader);
        glActiveTexture(GL_TEXTURE0); glBindTexture(GL_TEXTURE_2D, waterTex);
        glUniformMatrix4fv(loc_water_model, 1, GL_FALSE, glm::value_ptr(model));
        glUniformMatrix4fv(loc_water_view, 1, GL_FALSE, glm::value_ptr(view));
        glUniformMatrix4fv(loc_water_projection, 1, GL_FALSE, glm::value_ptr(projection));
        glUniform3fv(loc_water_lightDir, 1, glm::value_ptr(lightDir));
        glUniform3fv(loc_water_viewPos, 1, glm::value_ptr(cameraPos));
        glUniform1f(loc_water_time, currentFrame);
        glBindVertexArray(waterVAO);
        glDrawArrays(GL_TRIANGLES, 0, waterVertices.size());

        // Skybox
        glDepthFunc(GL_LEQUAL); 
        glUseProgram(skyboxShader);
        glm::mat4 skyView = glm::mat4(glm::mat3(view)); 
        glUniformMatrix4fv(loc_skybox_view, 1, GL_FALSE, glm::value_ptr(skyView));
        glUniformMatrix4fv(loc_skybox_projection, 1, GL_FALSE, glm::value_ptr(projection));
        glBindVertexArray(skyboxVAO);
        glDrawArrays(GL_TRIANGLES, 0, 36);
        glDepthFunc(GL_LESS); 

        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    glfwTerminate();
    return 0;
}