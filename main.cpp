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
#include "stb_image.h" // For stbi_set_flip_vertically_on_load

std::vector<Vertex> globalTerrainVertices;

void framebuffer_size_callback(GLFWwindow* window, int width, int height) {
    glViewport(0, 0, width, height);
}

int main() {
    if (!glfwInit()) {
        std::cerr << "Failed to initialize GLFW" << std::endl;
        return -1;
    }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE); // Required on macOS

    GLFWwindow* window = glfwCreateWindow(1024, 768, "Lake Strzeszynek 3D Viewer", NULL, NULL);
    if (!window) {
        std::cerr << "Failed to create GLFW window" << std::endl;
        glfwTerminate();
        return -1;
    }
    glfwMakeContextCurrent(window);
    glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);
    glfwSetCursorPosCallback(window, mouse_callback);
    glfwSetKeyCallback(window, key_callback);

    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);

    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
        std::cerr << "Failed to initialize GLAD" << std::endl;
        return -1;
    }

    glEnable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    unsigned int terrainShader = createShaderProgramFromFiles("Assets/shaders/terrain.vert", "Assets/shaders/terrain.frag");
    unsigned int waterShader = createShaderProgramFromFiles("Assets/shaders/water.vert", "Assets/shaders/water.frag");

    std::vector<Vertex> waterVertices;

    std::string terrainPath = findAssetPath("Assets/models/strzeszynek/teren-jezioro.obj");
    std::string waterPath = findAssetPath("Assets/models/strzeszynek/woda-jezioro.obj");

    std::cout << "Loading Terrain model: " << terrainPath << "..." << std::endl;
    if (!loadOBJ(terrainPath, globalTerrainVertices)) {
        std::cerr << "Failed to load terrain!" << std::endl;
        return -1;
    }
    std::cout << "Loaded " << globalTerrainVertices.size() << " terrain vertices." << std::endl;

    std::cout << "Loading Water model: " << waterPath << "..." << std::endl;
    if (!loadOBJ(waterPath, waterVertices)) {
        std::cerr << "Failed to load water!" << std::endl;
        return -1;
    }
    std::cout << "Loaded " << waterVertices.size() << " water vertices." << std::endl;

    unsigned int terrainVAO, terrainVBO;
    glGenVertexArrays(1, &terrainVAO);
    glGenBuffers(1, &terrainVBO);
    glBindVertexArray(terrainVAO);
    glBindBuffer(GL_ARRAY_BUFFER, terrainVBO);
    glBufferData(GL_ARRAY_BUFFER, globalTerrainVertices.size() * sizeof(Vertex), globalTerrainVertices.data(), GL_STATIC_DRAW);
    
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, position));
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, normal));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, texCoords));
    glEnableVertexAttribArray(2);

    unsigned int waterVAO, waterVBO;
    glGenVertexArrays(1, &waterVAO);
    glGenBuffers(1, &waterVBO);
    glBindVertexArray(waterVAO);
    glBindBuffer(GL_ARRAY_BUFFER, waterVBO);
    glBufferData(GL_ARRAY_BUFFER, waterVertices.size() * sizeof(Vertex), waterVertices.data(), GL_STATIC_DRAW);

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, position));
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, normal));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, texCoords));
    glEnableVertexAttribArray(2);

    glBindVertexArray(0);

    stbi_set_flip_vertically_on_load(true);
    unsigned int splatMapTex = loadTexture(findAssetPath("Assets/materials/strzeszynek/strzeszynek-splat-map.png").c_str());
    unsigned int mudTex = loadTexture(findAssetPath("Assets/materials/strzeszynek/teren-jezioro/mud/mud diffuse.bmp").c_str());
    unsigned int soilTex = loadTexture(findAssetPath("Assets/materials/strzeszynek/teren-jezioro/soil/soil-22-diffuse.jpg").c_str());
    unsigned int grassTex = loadTexture(findAssetPath("Assets/materials/strzeszynek/teren-jezioro/grass/PX_Ground_Grass_02_albedo.jpg").c_str());
    unsigned int waterTex = loadTexture(findAssetPath("Assets/materials/strzeszynek/woda-jezioro/woda-jezioro-diffuse.png").c_str());

    glUseProgram(terrainShader);
    glUniform1i(glGetUniformLocation(terrainShader, "splatMap"), 0);
    glUniform1i(glGetUniformLocation(terrainShader, "texMud"), 1);
    glUniform1i(glGetUniformLocation(terrainShader, "texSoil"), 2);
    glUniform1i(glGetUniformLocation(terrainShader, "texGrass"), 3);

    glUseProgram(waterShader);
    glUniform1i(glGetUniformLocation(waterShader, "texWater"), 0);

    glm::vec3 lightDir(0.6f, 0.9f, 0.4f);

    std::cout << "\n=============================================" << std::endl;
    std::cout << "Controls:" << std::endl;
    std::cout << " - W/S/A/D to walk around" << std::endl;
    std::cout << " - Move mouse to look around" << std::endl;
    std::cout << " - Press 'C' to toggle mouse cursor capture" << std::endl;
    std::cout << " - Press 'ESC' to close the app" << std::endl;
    std::cout << "=============================================\n" << std::endl;

    while (!glfwWindowShouldClose(window)) {
        float currentFrame = static_cast<float>(glfwGetTime());
        deltaTime = currentFrame - lastFrame;
        lastFrame = currentFrame;

        processInput(window);

        glClearColor(0.53f, 0.81f, 0.92f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        glm::mat4 model = glm::mat4(1.0f);
        glm::mat4 view = glm::lookAt(cameraPos, cameraPos + cameraFront, cameraUp);
        
        int width, height;
        glfwGetFramebufferSize(window, &width, &height);
        float aspect = (height == 0) ? 1.0f : (float)width / (float)height;
        glm::mat4 projection = glm::perspective(glm::radians(fov), aspect, 0.1f, 2000.0f);

        glUseProgram(terrainShader);
        
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, splatMapTex);
        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, mudTex);
        glActiveTexture(GL_TEXTURE2);
        glBindTexture(GL_TEXTURE_2D, soilTex);
        glActiveTexture(GL_TEXTURE3);
        glBindTexture(GL_TEXTURE_2D, grassTex);
        
        glUniformMatrix4fv(glGetUniformLocation(terrainShader, "model"), 1, GL_FALSE, glm::value_ptr(model));
        glUniformMatrix4fv(glGetUniformLocation(terrainShader, "view"), 1, GL_FALSE, glm::value_ptr(view));
        glUniformMatrix4fv(glGetUniformLocation(terrainShader, "projection"), 1, GL_FALSE, glm::value_ptr(projection));
        glUniform3fv(glGetUniformLocation(terrainShader, "lightDir"), 1, glm::value_ptr(lightDir));
        glUniform3fv(glGetUniformLocation(terrainShader, "viewPos"), 1, glm::value_ptr(cameraPos));

        glBindVertexArray(terrainVAO);
        glDrawArrays(GL_TRIANGLES, 0, globalTerrainVertices.size());

        glUseProgram(waterShader);
        
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, waterTex);
        
        glUniformMatrix4fv(glGetUniformLocation(waterShader, "model"), 1, GL_FALSE, glm::value_ptr(model));
        glUniformMatrix4fv(glGetUniformLocation(waterShader, "view"), 1, GL_FALSE, glm::value_ptr(view));
        glUniformMatrix4fv(glGetUniformLocation(waterShader, "projection"), 1, GL_FALSE, glm::value_ptr(projection));
        glUniform3fv(glGetUniformLocation(waterShader, "lightDir"), 1, glm::value_ptr(lightDir));
        glUniform3fv(glGetUniformLocation(waterShader, "viewPos"), 1, glm::value_ptr(cameraPos));
        glUniform1f(glGetUniformLocation(waterShader, "time"), currentFrame);

        glBindVertexArray(waterVAO);
        glDrawArrays(GL_TRIANGLES, 0, waterVertices.size());

        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    glDeleteVertexArrays(1, &terrainVAO);
    glDeleteBuffers(1, &terrainVBO);
    glDeleteVertexArrays(1, &waterVAO);
    glDeleteBuffers(1, &waterVBO);
    glDeleteProgram(terrainShader);
    glDeleteProgram(waterShader);

    glfwTerminate();
    return 0;
}