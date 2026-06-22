#include "Camera.h"
#include "Model.h"
#include <glm/gtc/matrix_transform.hpp>
#include <vector>

// Global Camera variables
glm::vec3 cameraPos   = glm::vec3(0.0f, 130.0f, 350.0f); 
glm::vec3 cameraFront = glm::vec3(0.0f, -0.35f, -0.9f);  
glm::vec3 cameraUp    = glm::vec3(0.0f, 1.0f, 0.0f);

bool firstMouse = true;
float yaw   = -90.0f;
float pitch = -25.0f;
float lastX = 400.0f;
float lastY = 300.0f;
float fov   = 45.0f;

float deltaTime = 0.0f;
float lastFrame = 0.0f;
bool captureMouse = true;

extern std::vector<Vertex> globalTerrainVertices;

float getTerrainHeight(float x, float z) {
    float bestHeight = -9999.0f;
    bool found = false;

    for (size_t i = 0; i < globalTerrainVertices.size(); i += 3) {
        if (i + 2 >= globalTerrainVertices.size()) break;
        glm::vec3 v0 = globalTerrainVertices[i].position;
        glm::vec3 v1 = globalTerrainVertices[i+1].position;
        glm::vec3 v2 = globalTerrainVertices[i+2].position;
        
        float det = (v1.z - v2.z) * (v0.x - v2.x) + (v2.x - v1.x) * (v0.z - v2.z);
        if (det == 0.0f) continue;
        
        float l1 = ((v1.z - v2.z) * (x - v2.x) + (v2.x - v1.x) * (z - v2.z)) / det;
        float l2 = ((v2.z - v0.z) * (x - v2.x) + (v0.x - v2.x) * (z - v2.z)) / det;
        float l3 = 1.0f - l1 - l2;
        
        if (l1 >= -0.01f && l1 <= 1.01f && l2 >= -0.01f && l2 <= 1.01f && l3 >= -0.01f && l3 <= 1.01f) {
            float h = l1 * v0.y + l2 * v1.y + l3 * v2.y;
            if (!found || h > bestHeight) {
                bestHeight = h;
                found = true;
            }
        }
    }
    return found ? bestHeight : cameraPos.y - 2.0f;
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

    float sensitivity = 0.08f;
    xoffset *= sensitivity;
    yoffset *= sensitivity;

    yaw += xoffset;
    pitch += yoffset;

    if (pitch > 89.0f)
        pitch = 89.0f;
    if (pitch < -89.0f)
        pitch = -89.0f;

    glm::vec3 front;
    front.x = cos(glm::radians(yaw)) * cos(glm::radians(pitch));
    front.y = sin(glm::radians(pitch));
    front.z = sin(glm::radians(yaw)) * cos(glm::radians(pitch));
    cameraFront = glm::normalize(front);
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

    float currentSpeed = 6.0f * deltaTime; // Prędkość spaceru człowieka

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

    cameraPos.x = newPos.x;
    cameraPos.z = newPos.z;

    float terrainHeight = getTerrainHeight(cameraPos.x, cameraPos.z);
    
    float targetY = terrainHeight + 2.0f; // Oko człowieka
    cameraPos.y += (targetY - cameraPos.y) * 15.0f * deltaTime;
}
