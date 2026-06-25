#include "CubemapGenerator.h"
#include <glad/glad.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <iostream>

unsigned int generateUnderwaterCubemap(unsigned int skyboxShader, unsigned int skyboxVAO, int resolution) {
    // Create the cubemap texture
    unsigned int cubemap;
    glGenTextures(1, &cubemap);
    glBindTexture(GL_TEXTURE_CUBE_MAP, cubemap);

    for (int i = 0; i < 6; ++i) {
        glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0, GL_RGB16F,
                     resolution, resolution, 0, GL_RGB, GL_FLOAT, nullptr);
    }
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);

    // Create FBO for rendering
    unsigned int fbo, rbo;
    glGenFramebuffers(1, &fbo);
    glGenRenderbuffers(1, &rbo);
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    glBindRenderbuffer(GL_RENDERBUFFER, rbo);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, resolution, resolution);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, rbo);

    // View matrices for each cubemap face
    glm::mat4 captureProjection = glm::perspective(glm::radians(90.0f), 1.0f, 0.1f, 10.0f);
    glm::mat4 captureViews[] = {
        glm::lookAt(glm::vec3(0), glm::vec3( 1,  0,  0), glm::vec3(0, -1,  0)),
        glm::lookAt(glm::vec3(0), glm::vec3(-1,  0,  0), glm::vec3(0, -1,  0)),
        glm::lookAt(glm::vec3(0), glm::vec3( 0,  1,  0), glm::vec3(0,  0,  1)),
        glm::lookAt(glm::vec3(0), glm::vec3( 0, -1,  0), glm::vec3(0,  0, -1)),
        glm::lookAt(glm::vec3(0), glm::vec3( 0,  0,  1), glm::vec3(0, -1,  0)),
        glm::lookAt(glm::vec3(0), glm::vec3( 0,  0, -1), glm::vec3(0, -1,  0)),
    };

    // Render to each face using the skybox shader
    glUseProgram(skyboxShader);
    glUniformMatrix4fv(glGetUniformLocation(skyboxShader, "projection"), 1, GL_FALSE, glm::value_ptr(captureProjection));
    // Set viewPos underwater to get underwater skybox colors
    glm::vec3 underwaterPos(0.0f, 50.0f, 0.0f);
    glUniform3fv(glGetUniformLocation(skyboxShader, "viewPos"), 1, glm::value_ptr(underwaterPos));

    glViewport(0, 0, resolution, resolution);
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);

    for (int i = 0; i < 6; ++i) {
        glUniformMatrix4fv(glGetUniformLocation(skyboxShader, "view"), 1, GL_FALSE, glm::value_ptr(captureViews[i]));
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                               GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, cubemap, 0);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        glDepthFunc(GL_LEQUAL);
        glBindVertexArray(skyboxVAO);
        glDrawArrays(GL_TRIANGLES, 0, 36);
        glDepthFunc(GL_LESS);
    }

    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    // Cleanup FBO resources
    glDeleteRenderbuffers(1, &rbo);
    glDeleteFramebuffers(1, &fbo);

    std::cout << "Cubemapa podwodna wygenerowana (" << resolution << "x" << resolution << ")" << std::endl;
    return cubemap;
}
