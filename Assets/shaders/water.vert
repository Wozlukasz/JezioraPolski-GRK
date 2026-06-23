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
uniform float time;

void main() {
    vec3 pos = aPos;
    
    // Wielowarstwowe fale — daje realistyczniejszy, naturalny ruch powierzchni
    float wave1 = sin(pos.x * 0.4 + time * 1.6) * 0.04;
    float wave2 = cos(pos.z * 0.4 + time * 1.3) * 0.04;
    float wave3 = sin(pos.x * 0.8 + pos.z * 0.6 + time * 2.2) * 0.015;
    float wave4 = cos(pos.x * 1.2 - pos.z * 0.3 + time * 1.0) * 0.01;
    pos.y += wave1 + wave2 + wave3 + wave4;
    
    FragPos = vec3(model * vec4(pos, 1.0));
    Normal = vec3(0.0, 1.0, 0.0); 
    TexCoords = aTexCoords;
    gl_Position = projection * view * vec4(FragPos, 1.0);
}
