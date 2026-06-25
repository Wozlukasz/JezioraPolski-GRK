#pragma once
#include <vector>
#include <glm/glm.hpp>

struct Bubble {
    glm::vec3 position;
    float size;       // Radius
    float age;        // Seconds alive
    float lifetime;   // Max lifetime
    float phase;      // Random phase for sinusoidal drift
    glm::vec3 velocity;
    bool active;
};

class BubbleSystem {
public:
    void init();
    void emit(const glm::vec3& position, int count);
    void update(float deltaTime);
    void render(const glm::mat4& view, const glm::mat4& projection, 
                const glm::vec3& viewPos, unsigned int shader);

private:
    std::vector<Bubble> bubbles;
    unsigned int vao, vbo;
    static const int MAX_BUBBLES = 300;
    int nextBubble = 0;
};
