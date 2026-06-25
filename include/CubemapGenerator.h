#pragma once

// Generates an underwater cubemap texture by rendering the procedural skybox
// to 6 faces of a GL_TEXTURE_CUBE_MAP.
unsigned int generateUnderwaterCubemap(unsigned int skyboxShader, unsigned int skyboxVAO, int resolution = 512);
