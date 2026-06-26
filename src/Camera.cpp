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

float fov   = 45.0f;

float deltaTime = 0.0f;
float lastFrame = 0.0f;
bool captureMouse = true;

void updateCameraVectors() {
    // Oblicz wektory kierunkowe kamery bezpośrednio z aktualnego kwaternionu (brak Gimbal Lock)
    cameraFront = glm::normalize(cameraOrientation * glm::vec3(0.0f, 0.0f, -1.0f));
    cameraUp    = glm::normalize(cameraOrientation * glm::vec3(0.0f, 1.0f, 0.0f));
    cameraRight = glm::normalize(cameraOrientation * glm::vec3(1.0f, 0.0f, 0.0f));
}


// Interaction flags
bool flashlightOn = false;
bool bubblesActive = false;
bool feedingMode = false;

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

    float sensitivity = 0.002f; // Czułość dla radianów
    xoffset *= sensitivity;
    yoffset *= sensitivity;

    // Obrót Pitch (Góra/Dół) względem LOKALNEJ osi X
    // Mnożenie z prawej strony oznacza obrót w układzie lokalnym!
    glm::quat qPitch = glm::angleAxis(yoffset, glm::vec3(1.0f, 0.0f, 0.0f));
    
    // Sprawdzamy limit wychylenia w pionie (ok. 89 stopni), aby nurek nie zrobił salta
    glm::quat tempOrientation = cameraOrientation * qPitch;
    glm::vec3 testFront = glm::normalize(tempOrientation * glm::vec3(0.0f, 0.0f, -1.0f));
    if (testFront.y < 0.99f && testFront.y > -0.99f) {
        cameraOrientation = tempOrientation;
    }

    // Obrót Yaw (Lewo/Prawo) względem GLOBALNEJ osi Y (0,1,0)
    // Mnożenie z lewej strony oznacza obrót w układzie globalnym! Zapobiega to przechyłom bocznym (roll).
    glm::quat qYaw = glm::angleAxis(-xoffset, glm::vec3(0.0f, 1.0f, 0.0f));
    cameraOrientation = qYaw * cameraOrientation;
    
    // Normalizacja zapobiega gromadzeniu błędów zmiennoprzecinkowych
    cameraOrientation = glm::normalize(cameraOrientation);

    updateCameraVectors();
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
    // Flashlight toggle
    if (key == GLFW_KEY_L && action == GLFW_PRESS) {
        flashlightOn = !flashlightOn;
        std::cout << "Latarka: " << (flashlightOn ? "ON" : "OFF") << std::endl;
    }
    // Bubbles toggle
    if (key == GLFW_KEY_B && action == GLFW_PRESS) {
        bubblesActive = !bubblesActive;
        std::cout << "Babelki: " << (bubblesActive ? "ON" : "OFF") << std::endl;
    }
    // Feeding mode toggle
    if (key == GLFW_KEY_E && action == GLFW_PRESS) {
        feedingMode = !feedingMode;
        std::cout << "Karmienie ryb: " << (feedingMode ? "ON" : "OFF") << std::endl;
    }
    // Fullscreen toggle
    if (key == GLFW_KEY_F && action == GLFW_PRESS) {
        static int winX = 0, winY = 0, winW = 1200, winH = 800;
        if (glfwGetWindowMonitor(window)) {
            // Restore to windowed mode
            glfwSetWindowMonitor(window, nullptr, winX, winY, winW, winH, 0);
        } else {
            // Switch to full screen
            glfwGetWindowPos(window, &winX, &winY);
            glfwGetWindowSize(window, &winW, &winH);
            GLFWmonitor* monitor = glfwGetPrimaryMonitor();
            const GLFWvidmode* mode = glfwGetVideoMode(monitor);
            glfwSetWindowMonitor(window, monitor, 0, 0, mode->width, mode->height, mode->refreshRate);
        }
    }
}

void processInput(GLFWwindow* window) {
    if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
        glfwSetWindowShouldClose(window, true);

    // Ograniczenie deltaTime — zapobiega "wystrzeliwaniu" kamery przy niskim FPS
    float clampedDt = std::min(deltaTime, 1.0f / 15.0f);
    
    // Boost prędkości przy użyciu klawisza CTRL
    float speedMultiplier = (glfwGetKey(window, GLFW_KEY_LEFT_CONTROL) == GLFW_PRESS) ? 3.0f : 1.0f;
    float currentSpeed = 6.0f * clampedDt * speedMultiplier; // Prędkość spaceru / pływania

    glm::vec3 front = glm::normalize(cameraFront);
    glm::vec3 right = glm::normalize(glm::cross(front, cameraUp));
    glm::vec3 up    = glm::vec3(0.0f, 1.0f, 0.0f);

    glm::vec3 newPos = cameraPos;
    if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS)
        newPos += currentSpeed * front;
    if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS)
        newPos -= currentSpeed * front;
    if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS)
        newPos -= currentSpeed * right;
    if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS)
        newPos += currentSpeed * right;
    if (glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS)
        newPos += currentSpeed * up;
    if (glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS)
        newPos -= currentSpeed * up;
    if (glfwGetKey(window, GLFW_KEY_T) == GLFW_PRESS) {
        newPos = glm::vec3(28.0f, 57.0f, 35.0f);
        std::cout << "Teleportacja do lawicy ryb: (28, 57, 35)" << std::endl;
    }

    // Sprawdź czy nowa pozycja jest na terenie
    float newTerrainHeight = getTerrainHeight(newPos.x, newPos.z);
    if (newTerrainHeight > -900.0f) {
        // Blokada przed wejściem pod ziemię (1.0m nad terenem)
        if (newPos.y < newTerrainHeight + 1.0f) {
            newPos.y = newTerrainHeight + 1.0f;
        }
        cameraPos = newPos;
    }
}
