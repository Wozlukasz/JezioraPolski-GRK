#pragma once
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>

// Expose camera variables
extern glm::vec3 cameraPos;
#include <glm/gtc/quaternion.hpp>

extern glm::quat cameraOrientation;
extern glm::vec3 cameraFront;
extern glm::vec3 cameraUp;
extern glm::vec3 cameraRight;

extern bool firstMouse;
extern float lastX;
extern float lastY;
extern float fov;
extern bool captureMouse;

// Time variables
struct TerrainData {
    float height;
    glm::vec2 uv;
};

TerrainData getTerrainData(float x, float z);
float getTerrainHeight(float x, float z);
extern float deltaTime;
extern float lastFrame;

// Interaction flags
extern bool flashlightOn;   // [F] Toggle underwater flashlight
extern bool bubblesActive;   // [B] Toggle bubble emission
extern bool feedingMode;     // [E] Toggle fish feeding/attraction mode

void mouse_callback(GLFWwindow* window, double xpos, double ypos);
void key_callback(GLFWwindow* window, int key, int scancode, int action, int mods);
void processInput(GLFWwindow* window);
