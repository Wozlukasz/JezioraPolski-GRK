#include "Camera.h"
#include "Model.h"
#include <glm/gtc/matrix_transform.hpp>
#include <vector>

// Global Camera variables
glm::vec3 cameraPos = glm::vec3(32.1f, 66.0f, 38.3f); // Spawnpoint na brzegu blisko środka mapy
glm::quat cameraOrientation = glm::quat(glm::vec3(glm::radians(-15.0f), glm::radians(-45.0f), 0.0f));

glm::vec3 cameraFront = cameraOrientation * glm::vec3(0.0f, 0.0f, -1.0f);
glm::vec3 cameraUp    = cameraOrientation * glm::vec3(0.0f, 1.0f, 0.0f);
glm::vec3 cameraRight = cameraOrientation * glm::vec3(1.0f, 0.0f, 0.0f);

bool firstMouse = true;
float lastX = 400.0f;
float lastY = 300.0f;

// Total accumulated pitch to prevent looking past straight up/down
float accumulatedPitch = glm::radians(-25.0f);
float fov   = 45.0f;

float deltaTime = 0.0f;
float lastFrame = 0.0f;
bool captureMouse = true;

extern std::vector<Vertex> globalTerrainVertices;

#include <unordered_map>
#include <algorithm>
#include <iostream>

struct GridCell {
    std::vector<size_t> triangles;
};

static std::unordered_map<int, GridCell> spatialGrid;
static float cellSize = 10.0f;
static bool gridInitialized = false;

void initTerrainGrid() {
    if (gridInitialized || globalTerrainVertices.empty()) return;
    
    for (size_t i = 0; i < globalTerrainVertices.size(); i += 3) {
        if (i + 2 >= globalTerrainVertices.size()) break;
        glm::vec3 v0 = globalTerrainVertices[i].position;
        glm::vec3 v1 = globalTerrainVertices[i+1].position;
        glm::vec3 v2 = globalTerrainVertices[i+2].position;
        
        float minX = std::min({v0.x, v1.x, v2.x});
        float maxX = std::max({v0.x, v1.x, v2.x});
        float minZ = std::min({v0.z, v1.z, v2.z});
        float maxZ = std::max({v0.z, v1.z, v2.z});
        
        int gxMin = std::floor(minX / cellSize);
        int gxMax = std::floor(maxX / cellSize);
        int gzMin = std::floor(minZ / cellSize);
        int gzMax = std::floor(maxZ / cellSize);
        
        for (int gx = gxMin; gx <= gxMax; ++gx) {
            for (int gz = gzMin; gz <= gzMax; ++gz) {
                int key = gx * 10000 + gz;
                spatialGrid[key].triangles.push_back(i);
            }
        }
    }
    gridInitialized = true;
    std::cout << "Zoptymalizowana siatka terenu utworzona. Ilosc komorek: " << spatialGrid.size() << std::endl;
}

TerrainData getTerrainData(float x, float z) {
    if (!gridInitialized) initTerrainGrid();

    TerrainData result = { -1000.0f, glm::vec2(0.0f) };
    if (globalTerrainVertices.empty()) return result;

    int gx = std::floor(x / cellSize);
    int gz = std::floor(z / cellSize);
    int key = gx * 10000 + gz;
    
    auto it = spatialGrid.find(key);
    if (it == spatialGrid.end()) return result;

    for (size_t i : it->second.triangles) {
        glm::vec3 v0 = globalTerrainVertices[i].position;
        glm::vec3 v1 = globalTerrainVertices[i+1].position;
        glm::vec3 v2 = globalTerrainVertices[i+2].position;

        float det = (v1.z - v2.z) * (v0.x - v2.x) + (v2.x - v1.x) * (v0.z - v2.z);
        if (std::abs(det) < 1e-6f) continue;

        float w0 = ((v1.z - v2.z) * (x - v2.x) + (v2.x - v1.x) * (z - v2.z)) / det;
        float w1 = ((v2.z - v0.z) * (x - v2.x) + (v0.x - v2.x) * (z - v2.z)) / det;
        float w2 = 1.0f - w0 - w1;

        if (w0 >= -0.01f && w1 >= -0.01f && w2 >= -0.01f) {
            result.height = w0 * v0.y + w1 * v1.y + w2 * v2.y;
            result.uv = w0 * globalTerrainVertices[i].texCoords + 
                        w1 * globalTerrainVertices[i+1].texCoords + 
                        w2 * globalTerrainVertices[i+2].texCoords;
            break;
        }
    }

    return result;
}

float getTerrainHeight(float x, float z) {
    return getTerrainData(x, z).height;
}

void mouse_callback(GLFWwindow* window, double xposIn, double yposIn) {
    if (!captureMouse) return;

    float xpos = static_cast<float>(xposIn);
    float ypos = static_cast<float>(yposIn);

    if (firstMouse) {
        lastX = xpos;
        lastY = ypos;
        firstMouse = false;
    }

    float xoffset = xpos - lastX;
    float yoffset = lastY - ypos; // reversed since y-coordinates go from bottom to top
    lastX = xpos;
    lastY = ypos;

    float sensitivity = 0.002f; // Wartość dla radianów
    xoffset *= sensitivity;
    yoffset *= sensitivity;

    // Ograniczenie pitch (żeby nie wywrócić kamery na plecy)
    float newPitch = accumulatedPitch + yoffset;
    if (newPitch > glm::radians(89.0f)) {
        yoffset = glm::radians(89.0f) - accumulatedPitch;
        accumulatedPitch = glm::radians(89.0f);
    } else if (newPitch < glm::radians(-89.0f)) {
        yoffset = glm::radians(-89.0f) - accumulatedPitch;
        accumulatedPitch = glm::radians(-89.0f);
    } else {
        accumulatedPitch = newPitch;
    }

    // Yaw: obrót wokół globalnej osi Y
    glm::quat qYaw = glm::angleAxis(-xoffset, glm::vec3(0.0f, 1.0f, 0.0f));
    
    // Pitch: obrót wokół lokalnej osi X (prawej)
    cameraRight = cameraOrientation * glm::vec3(1.0f, 0.0f, 0.0f);
    glm::quat qPitch = glm::angleAxis(yoffset, cameraRight);
    
    // Aktualizacja kwaternionu orientacji
    cameraOrientation = qYaw * qPitch * cameraOrientation;
    cameraOrientation = glm::normalize(cameraOrientation);
    
    // Aktualizacja wektorów
    cameraFront = cameraOrientation * glm::vec3(0.0f, 0.0f, -1.0f);
    cameraUp = cameraOrientation * glm::vec3(0.0f, 1.0f, 0.0f);
    cameraRight = cameraOrientation * glm::vec3(1.0f, 0.0f, 0.0f);
}

void key_callback(GLFWwindow* window, int key, int scancode, int action, int mods) {
    if (key == GLFW_KEY_C && action == GLFW_PRESS) {
        captureMouse = !captureMouse;
        if (captureMouse) {
            glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
            firstMouse = true;
        } else {
            glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
        }
    }
}

void processInput(GLFWwindow* window) {
    if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
        glfwSetWindowShouldClose(window, true);

    // Ograniczenie deltaTime — zapobiega "wystrzeliwaniu" kamery przy niskim FPS
    float clampedDt = std::min(deltaTime, 1.0f / 15.0f);
    float currentSpeed = 6.0f * clampedDt; // Prędkość spaceru człowieka

    glm::vec3 frontXZ = glm::normalize(glm::vec3(cameraFront.x, 0.0f, cameraFront.z));
    glm::vec3 rightXZ = glm::normalize(glm::cross(frontXZ, cameraUp));

    glm::vec3 newPos = cameraPos;
    if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS)
        newPos += currentSpeed * frontXZ;
    if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS)
        newPos -= currentSpeed * frontXZ;
    if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS)
        newPos -= currentSpeed * rightXZ;
    if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS)
        newPos += currentSpeed * rightXZ;

    // Sprawdź czy nowa pozycja jest na terenie — jeśli nie, zostań w miejscu
    float newTerrainHeight = getTerrainHeight(newPos.x, newPos.z);
    if (newTerrainHeight > -900.0f) {
        cameraPos.x = newPos.x;
        cameraPos.z = newPos.z;
        float targetY = newTerrainHeight + 2.0f; // Oko człowieka
        cameraPos.y += (targetY - cameraPos.y) * 15.0f * clampedDt;
    }
    // Jeśli teren nie istnieje pod nową pozycją, kamera zostaje w miejscu
}
