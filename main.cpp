#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <iostream>
#include <vector>
#include <string>
#include <fstream>
#include <sstream>
#include <algorithm>

struct Vertex {
    glm::vec3 position;
    glm::vec3 normal;
    glm::vec2 texCoords;
};

// Global Camera variables
glm::vec3 cameraPos   = glm::vec3(0.0f, 130.0f, 350.0f); // Start high to look down at the lake
glm::vec3 cameraFront = glm::vec3(0.0f, -0.35f, -0.9f);  // Look downwards toward the lake
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

// Callback function declarations
void framebuffer_size_callback(GLFWwindow* window, int width, int height);
void mouse_callback(GLFWwindow* window, double xpos, double ypos);
void key_callback(GLFWwindow* window, int key, int scancode, int action, int mods);
void processInput(GLFWwindow* window);

// Helper to locate files relative to working directories
std::string findAssetPath(const std::string& relativePath) {
    std::ifstream f(relativePath);
    if (f.good()) return relativePath;
    
    std::string upPath = "../" + relativePath;
    std::ifstream fUp(upPath);
    if (fUp.good()) return upPath;
    
    std::string upUpPath = "../../" + relativePath;
    std::ifstream fUpUp(upUpPath);
    if (fUpUp.good()) return upUpPath;

    return relativePath;
}

// Simple and robust OBJ loader
bool loadOBJ(const std::string& path, std::vector<Vertex>& out_vertices) {
    std::vector<glm::vec3> temp_positions;
    std::vector<glm::vec3> temp_normals;
    std::vector<glm::vec2> temp_texcoords;
    
    std::ifstream file(path);
    if (!file.is_open()) {
        std::cerr << "Failed to open OBJ file: " << path << std::endl;
        return false;
    }
    
    std::string line;
    while (std::getline(file, line)) {
        if (line.empty() || line[0] == '#') continue;
        
        std::istringstream ss(line);
        std::string type;
        ss >> type;
        
        if (type == "v") {
            float x, y, z;
            ss >> x >> y >> z;
            temp_positions.push_back(glm::vec3(x, y, z));
        } else if (type == "vn") {
            float x, y, z;
            ss >> x >> y >> z;
            temp_normals.push_back(glm::vec3(x, y, z));
        } else if (type == "vt") {
            float u, v;
            ss >> u >> v;
            temp_texcoords.push_back(glm::vec2(u, v));
        } else if (type == "f") {
            std::vector<std::string> faceTokens;
            std::string token;
            while (ss >> token) {
                faceTokens.push_back(token);
            }
            
            auto parseVertex = [&](const std::string& vertexStr) -> Vertex {
                int v_idx = 0, vt_idx = 0, vn_idx = 0;
                size_t first_slash = vertexStr.find('/');
                if (first_slash == std::string::npos) {
                    v_idx = std::stoi(vertexStr);
                } else {
                    v_idx = std::stoi(vertexStr.substr(0, first_slash));
                    size_t second_slash = vertexStr.find('/', first_slash + 1);
                    if (second_slash == std::string::npos) {
                        std::string vt_str = vertexStr.substr(first_slash + 1);
                        if (!vt_str.empty()) vt_idx = std::stoi(vt_str);
                    } else {
                        std::string vt_str = vertexStr.substr(first_slash + 1, second_slash - first_slash - 1);
                        if (!vt_str.empty()) vt_idx = std::stoi(vt_str);
                        std::string vn_str = vertexStr.substr(second_slash + 1);
                        if (!vn_str.empty()) vn_idx = std::stoi(vn_str);
                    }
                }
                
                Vertex v;
                if (v_idx > 0) {
                    v.position = temp_positions[v_idx - 1];
                } else if (v_idx < 0) {
                    v.position = temp_positions[temp_positions.size() + v_idx];
                } else {
                    v.position = glm::vec3(0.0f);
                }
                
                if (vt_idx > 0 && !temp_texcoords.empty()) {
                    v.texCoords = temp_texcoords[vt_idx - 1];
                } else if (vt_idx < 0 && !temp_texcoords.empty()) {
                    v.texCoords = temp_texcoords[temp_texcoords.size() + vt_idx];
                } else {
                    v.texCoords = glm::vec2(0.0f);
                }
                
                if (vn_idx > 0 && !temp_normals.empty()) {
                    v.normal = temp_normals[vn_idx - 1];
                } else if (vn_idx < 0 && !temp_normals.empty()) {
                    v.normal = temp_normals[temp_normals.size() + vn_idx];
                } else {
                    v.normal = glm::vec3(0.0f, 1.0f, 0.0f);
                }
                return v;
            };

            // Support general polygon triangulation using a simple triangle fan
            for (size_t i = 1; i < faceTokens.size() - 1; ++i) {
                out_vertices.push_back(parseVertex(faceTokens[0]));
                out_vertices.push_back(parseVertex(faceTokens[i]));
                out_vertices.push_back(parseVertex(faceTokens[i + 1]));
            }
        }
    }
    return true;
}

// Shader helper functions
unsigned int compileShader(unsigned int type, const std::string& source) {
    unsigned int id = glCreateShader(type);
    const char* src = source.c_str();
    glShaderSource(id, 1, &src, nullptr);
    glCompileShader(id);
    
    int result;
    glGetShaderiv(id, GL_COMPILE_STATUS, &result);
    if (result == GL_FALSE) {
        int length;
        glGetShaderiv(id, GL_INFO_LOG_LENGTH, &length);
        std::vector<char> message(length);
        glGetShaderInfoLog(id, length, &length, message.data());
        std::cerr << "Failed to compile " << (type == GL_VERTEX_SHADER ? "vertex" : "fragment") << " shader!" << std::endl;
        std::cerr << message.data() << std::endl;
        glDeleteShader(id);
        return 0;
    }
    return id;
}

unsigned int createShaderProgram(const std::string& vertexSource, const std::string& fragmentSource) {
    unsigned int program = glCreateProgram();
    unsigned int vs = compileShader(GL_VERTEX_SHADER, vertexSource);
    unsigned int fs = compileShader(GL_FRAGMENT_SHADER, fragmentSource);
    
    glAttachShader(program, vs);
    glAttachShader(program, fs);
    glLinkProgram(program);
    glValidateProgram(program);
    
    glDeleteShader(vs);
    glDeleteShader(fs);
    
    return program;
}

// Terrain Shader Sources
const std::string terrainVertexShader = R"glsl(
#version 330 core
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aNormal;
layout (location = 2) in vec2 aTexCoords;

out vec3 FragPos;
out vec3 Normal;
out vec2 TexCoords;

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;

void main() {
    FragPos = vec3(model * vec4(aPos, 1.0));
    Normal = mat3(transpose(inverse(model))) * aNormal;
    TexCoords = aTexCoords;
    gl_Position = projection * view * vec4(FragPos, 1.0);
}
)glsl";

const std::string terrainFragmentShader = R"glsl(
#version 330 core
out vec4 FragColor;

in vec3 FragPos;
in vec3 Normal;
in vec2 TexCoords;

uniform vec3 lightDir;
uniform vec3 viewPos;

void main() {
    float y = FragPos.y;
    vec3 baseColor;

    // Height-based coloring:
    // Lake water level is 64.01
    if (y < 64.0) {
        // Deep underwater bed (sand to deep mud)
        float t = clamp((y - 45.0) / 19.0, 0.0, 1.0);
        baseColor = mix(vec3(0.12, 0.16, 0.14), vec3(0.55, 0.52, 0.45), t);
    } else if (y < 65.5) {
        // Shoreline / Sandy Beach
        float t = clamp((y - 64.0) / 1.5, 0.0, 1.0);
        baseColor = mix(vec3(0.55, 0.52, 0.45), vec3(0.78, 0.73, 0.60), t);
    } else if (y < 72.0) {
        // Meadows / Grass
        float t = clamp((y - 65.5) / 6.5, 0.0, 1.0);
        baseColor = mix(vec3(0.40, 0.62, 0.28), vec3(0.25, 0.50, 0.20), t);
    } else {
        // Forest / Hills
        float t = clamp((y - 72.0) / 25.0, 0.0, 1.0);
        baseColor = mix(vec3(0.25, 0.50, 0.20), vec3(0.15, 0.32, 0.15), t);
    }

    // Directional lighting
    vec3 norm = normalize(Normal);
    vec3 lightVector = normalize(lightDir);
    float diff = max(dot(norm, lightVector), 0.0);
    vec3 diffuse = diff * vec3(0.85, 0.85, 0.80);

    // Subtle Ambient
    vec3 ambient = vec3(0.35, 0.38, 0.42);

    vec3 finalColor = (ambient + diffuse) * baseColor;
    FragColor = vec4(finalColor, 1.0);
}
)glsl";

// Water Shader Sources
const std::string waterVertexShader = R"glsl(
#version 330 core
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aNormal;
layout (location = 2) in vec2 aTexCoords;

out vec3 FragPos;
out vec3 Normal;

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;
uniform float time;

void main() {
    // Generate gentle wave deformation based on sinusoids
    vec3 pos = aPos;
    pos.y += sin(pos.x * 0.08 + time * 1.6) * 0.12 + cos(pos.z * 0.08 + time * 1.3) * 0.12;
    
    FragPos = vec3(model * vec4(pos, 1.0));
    Normal = vec3(0.0, 1.0, 0.0); 
    gl_Position = projection * view * vec4(FragPos, 1.0);
}
)glsl";

const std::string waterFragmentShader = R"glsl(
#version 330 core
out vec4 FragColor;

in vec3 FragPos;
in vec3 Normal;

uniform vec3 lightDir;
uniform vec3 viewPos;

void main() {
    vec3 waterColor = vec3(0.05, 0.42, 0.58); // Semi-transparent turquoise water

    // Specular highlight representing the sun's reflection on waves
    vec3 norm = normalize(Normal);
    vec3 viewDir = normalize(viewPos - FragPos);
    vec3 lightVector = normalize(lightDir);
    
    vec3 halfwayDir = normalize(lightVector + viewDir);
    float spec = pow(max(dot(norm, halfwayDir), 0.0), 128.0);
    vec3 specular = vec3(0.9, 0.95, 1.0) * spec * 0.9;

    FragColor = vec4(waterColor + specular, 0.70); // 70% transparency
}
)glsl";

int main() {
    // Initialize GLFW
    if (!glfwInit()) {
        std::cerr << "Failed to initialize GLFW" << std::endl;
        return -1;
    }

    // Configure GLFW for macOS Compatibility
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE); // Required on macOS

    // Create Window
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

    // Capture the cursor by default for mouse look controls
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);

    // Initialize GLAD before calling any OpenGL functions
    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
        std::cerr << "Failed to initialize GLAD" << std::endl;
        return -1;
    }

    // OpenGL Configuration
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    // Create Shaders
    unsigned int terrainShader = createShaderProgram(terrainVertexShader, terrainFragmentShader);
    unsigned int waterShader = createShaderProgram(waterVertexShader, waterFragmentShader);

    // Load Models
    std::vector<Vertex> terrainVertices;
    std::vector<Vertex> waterVertices;

    std::string terrainPath = findAssetPath("Assets/models/strzeszynek/teren-jezioro.obj");
    std::string waterPath = findAssetPath("Assets/models/strzeszynek/woda-jezioro.obj");

    std::cout << "Loading Terrain model: " << terrainPath << "..." << std::endl;
    if (!loadOBJ(terrainPath, terrainVertices)) {
        std::cerr << "Failed to load terrain!" << std::endl;
        return -1;
    }
    std::cout << "Loaded " << terrainVertices.size() << " terrain vertices." << std::endl;

    std::cout << "Loading Water model: " << waterPath << "..." << std::endl;
    if (!loadOBJ(waterPath, waterVertices)) {
        std::cerr << "Failed to load water!" << std::endl;
        return -1;
    }
    std::cout << "Loaded " << waterVertices.size() << " water vertices." << std::endl;

    // Buffer setups
    unsigned int terrainVAO, terrainVBO;
    glGenVertexArrays(1, &terrainVAO);
    glGenBuffers(1, &terrainVBO);
    glBindVertexArray(terrainVAO);
    glBindBuffer(GL_ARRAY_BUFFER, terrainVBO);
    glBufferData(GL_ARRAY_BUFFER, terrainVertices.size() * sizeof(Vertex), terrainVertices.data(), GL_STATIC_DRAW);
    
    // Position Attribute
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, position));
    glEnableVertexAttribArray(0);
    // Normal Attribute
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, normal));
    glEnableVertexAttribArray(1);
    // TexCoords Attribute
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, texCoords));
    glEnableVertexAttribArray(2);

    unsigned int waterVAO, waterVBO;
    glGenVertexArrays(1, &waterVAO);
    glGenBuffers(1, &waterVBO);
    glBindVertexArray(waterVAO);
    glBindBuffer(GL_ARRAY_BUFFER, waterVBO);
    glBufferData(GL_ARRAY_BUFFER, waterVertices.size() * sizeof(Vertex), waterVertices.data(), GL_STATIC_DRAW);

    // Position Attribute
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, position));
    glEnableVertexAttribArray(0);
    // Normal Attribute
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, normal));
    glEnableVertexAttribArray(1);
    // TexCoords Attribute
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, texCoords));
    glEnableVertexAttribArray(2);

    glBindVertexArray(0);

    // Lighting parameters
    glm::vec3 lightDir(0.6f, 0.9f, 0.4f);

    std::cout << "\n=============================================" << std::endl;
    std::cout << "Controls:" << std::endl;
    std::cout << " - W/S/A/D to move/fly around" << std::endl;
    std::cout << " - SPACE to fly UP" << std::endl;
    std::cout << " - LEFT SHIFT to fly DOWN" << std::endl;
    std::cout << " - Move mouse to look around" << std::endl;
    std::cout << " - Press 'C' to toggle mouse cursor capture" << std::endl;
    std::cout << " - Press 'ESC' to close the app" << std::endl;
    std::cout << "=============================================\n" << std::endl;

    // Render Loop
    while (!glfwWindowShouldClose(window)) {
        // Calculate frame time
        float currentFrame = static_cast<float>(glfwGetTime());
        deltaTime = currentFrame - lastFrame;
        lastFrame = currentFrame;

        // Input
        processInput(window);

        // Rendering Background color (Sky Blue)
        glClearColor(0.53f, 0.81f, 0.92f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        // Coordinate Matrices
        glm::mat4 model = glm::mat4(1.0f);
        glm::mat4 view = glm::lookAt(cameraPos, cameraPos + cameraFront, cameraUp);
        
        int width, height;
        glfwGetFramebufferSize(window, &width, &height);
        float aspect = (height == 0) ? 1.0f : (float)width / (float)height;
        glm::mat4 projection = glm::perspective(glm::radians(fov), aspect, 0.1f, 2000.0f);

        // 1. Draw Opaque Terrain
        glUseProgram(terrainShader);
        
        glUniformMatrix4fv(glGetUniformLocation(terrainShader, "model"), 1, GL_FALSE, glm::value_ptr(model));
        glUniformMatrix4fv(glGetUniformLocation(terrainShader, "view"), 1, GL_FALSE, glm::value_ptr(view));
        glUniformMatrix4fv(glGetUniformLocation(terrainShader, "projection"), 1, GL_FALSE, glm::value_ptr(projection));
        glUniform3fv(glGetUniformLocation(terrainShader, "lightDir"), 1, glm::value_ptr(lightDir));
        glUniform3fv(glGetUniformLocation(terrainShader, "viewPos"), 1, glm::value_ptr(cameraPos));

        glBindVertexArray(terrainVAO);
        glDrawArrays(GL_TRIANGLES, 0, terrainVertices.size());

        // 2. Draw Semi-Transparent Water
        glUseProgram(waterShader);
        
        glUniformMatrix4fv(glGetUniformLocation(waterShader, "model"), 1, GL_FALSE, glm::value_ptr(model));
        glUniformMatrix4fv(glGetUniformLocation(waterShader, "view"), 1, GL_FALSE, glm::value_ptr(view));
        glUniformMatrix4fv(glGetUniformLocation(waterShader, "projection"), 1, GL_FALSE, glm::value_ptr(projection));
        glUniform3fv(glGetUniformLocation(waterShader, "lightDir"), 1, glm::value_ptr(lightDir));
        glUniform3fv(glGetUniformLocation(waterShader, "viewPos"), 1, glm::value_ptr(cameraPos));
        glUniform1f(glGetUniformLocation(waterShader, "time"), currentFrame);

        glBindVertexArray(waterVAO);
        glDrawArrays(GL_TRIANGLES, 0, waterVertices.size());

        // Buffers swap and poll events
        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    // De-allocate resources
    glDeleteVertexArrays(1, &terrainVAO);
    glDeleteBuffers(1, &terrainVBO);
    glDeleteVertexArrays(1, &waterVAO);
    glDeleteBuffers(1, &waterVBO);
    glDeleteProgram(terrainShader);
    glDeleteProgram(waterShader);

    glfwTerminate();
    return 0;
}

// Callback implementation
void framebuffer_size_callback(GLFWwindow* window, int width, int height) {
    glViewport(0, 0, width, height);
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

    float currentSpeed = 120.0f * deltaTime; // Movement speed (units/sec)
    if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS)
        cameraPos += currentSpeed * cameraFront;
    if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS)
        cameraPos -= currentSpeed * cameraFront;
    if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS)
        cameraPos -= glm::normalize(glm::cross(cameraFront, cameraUp)) * currentSpeed;
    if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS)
        cameraPos += glm::normalize(glm::cross(cameraFront, cameraUp)) * currentSpeed;
    if (glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS)
        cameraPos += currentSpeed * cameraUp;
    if (glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS)
        cameraPos -= currentSpeed * cameraUp;
}