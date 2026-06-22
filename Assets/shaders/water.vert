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
    // Generate gentle wave deformation based on sinusoids
    vec3 pos = aPos;
    // Zwiększona częstotliwość fali i zmniejszona amplituda
    pos.y += sin(pos.x * 0.4 + time * 1.6) * 0.05 + cos(pos.z * 0.4 + time * 1.3) * 0.05;
    
    FragPos = vec3(model * vec4(pos, 1.0));
    Normal = vec3(0.0, 1.0, 0.0); 
    TexCoords = aTexCoords;
    gl_Position = projection * view * vec4(FragPos, 1.0);
}
