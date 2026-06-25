#include "BubbleSystem.h"
#include <glad/glad.h>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <random>
#include <cmath>

void BubbleSystem::init() {
    bubbles.resize(MAX_BUBBLES);
    for (auto& b : bubbles) {
        b.active = false;
    }

    // Create a simple quad for billboard rendering (two triangles)
    float quadVertices[] = {
        // positions      // uvs
        -0.5f, -0.5f, 0.0f,  0.0f, 0.0f,
         0.5f, -0.5f, 0.0f,  1.0f, 0.0f,
         0.5f,  0.5f, 0.0f,  1.0f, 1.0f,

        -0.5f, -0.5f, 0.0f,  0.0f, 0.0f,
         0.5f,  0.5f, 0.0f,  1.0f, 1.0f,
        -0.5f,  0.5f, 0.0f,  0.0f, 1.0f,
    };

    glGenVertexArrays(1, &vao);
    glGenBuffers(1, &vbo);
    glBindVertexArray(vao);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(quadVertices), quadVertices, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);
    glBindVertexArray(0);
}

void BubbleSystem::emit(const glm::vec3& position, int count) {
    static std::mt19937 rng(42);
    std::uniform_real_distribution<float> sizeDist(0.03f, 0.12f);
    std::uniform_real_distribution<float> lifeDist(3.0f, 8.0f);
    std::uniform_real_distribution<float> phaseDist(0.0f, 6.2832f);
    std::uniform_real_distribution<float> spreadDist(-0.3f, 0.3f);
    std::uniform_real_distribution<float> speedDist(0.8f, 2.5f);

    for (int i = 0; i < count; ++i) {
        Bubble& b = bubbles[nextBubble % MAX_BUBBLES];
        nextBubble++;

        b.position = position + glm::vec3(spreadDist(rng), spreadDist(rng) * 0.5f, spreadDist(rng));
        b.size = sizeDist(rng);
        b.age = 0.0f;
        b.lifetime = lifeDist(rng);
        b.phase = phaseDist(rng);
        b.velocity = glm::vec3(spreadDist(rng) * 0.1f, speedDist(rng), spreadDist(rng) * 0.1f);
        b.active = true;
    }
}

void BubbleSystem::update(float deltaTime) {
    for (auto& b : bubbles) {
        if (!b.active) continue;

        b.age += deltaTime;
        if (b.age >= b.lifetime || b.position.y > 64.0f) {
            b.active = false;
            continue;
        }

        // Buoyancy: main upward movement
        b.position += b.velocity * deltaTime;

        // Sinusoidal lateral drift (like real bubbles wobble)
        float wobbleX = sin(b.age * 2.5f + b.phase) * 0.15f * deltaTime;
        float wobbleZ = cos(b.age * 1.8f + b.phase * 1.3f) * 0.12f * deltaTime;
        b.position.x += wobbleX;
        b.position.z += wobbleZ;

        // Bubbles grow slightly as they rise (pressure decrease)
        b.size += deltaTime * 0.005f;
    }
}

void BubbleSystem::render(const glm::mat4& view, const glm::mat4& projection,
                           const glm::vec3& viewPos, unsigned int shader) {
    glUseProgram(shader);
    glUniformMatrix4fv(glGetUniformLocation(shader, "view"), 1, GL_FALSE, glm::value_ptr(view));
    glUniformMatrix4fv(glGetUniformLocation(shader, "projection"), 1, GL_FALSE, glm::value_ptr(projection));
    glUniform3fv(glGetUniformLocation(shader, "viewPos"), 1, glm::value_ptr(viewPos));

    // Extract camera right and up vectors from the view matrix for billboarding
    glm::vec3 camRight = glm::vec3(view[0][0], view[1][0], view[2][0]);
    glm::vec3 camUp = glm::vec3(view[0][1], view[1][1], view[2][1]);
    glUniform3fv(glGetUniformLocation(shader, "camRight"), 1, glm::value_ptr(camRight));
    glUniform3fv(glGetUniformLocation(shader, "camUp"), 1, glm::value_ptr(camUp));

    glBindVertexArray(vao);

    GLint loc_bubblePos = glGetUniformLocation(shader, "bubblePos");
    GLint loc_bubbleSize = glGetUniformLocation(shader, "bubbleSize");
    GLint loc_bubbleAlpha = glGetUniformLocation(shader, "bubbleAlpha");
    GLint loc_bubbleAge = glGetUniformLocation(shader, "bubbleAge");

    for (const auto& b : bubbles) {
        if (!b.active) continue;

        float alpha = 1.0f - (b.age / b.lifetime);
        alpha = glm::clamp(alpha, 0.0f, 1.0f);

        glUniform3fv(loc_bubblePos, 1, glm::value_ptr(b.position));
        glUniform1f(loc_bubbleSize, b.size);
        glUniform1f(loc_bubbleAlpha, alpha);
        glUniform1f(loc_bubbleAge, b.age);

        glDrawArrays(GL_TRIANGLES, 0, 6);
    }

    glBindVertexArray(0);
}
