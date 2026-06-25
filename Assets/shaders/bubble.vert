#version 330 core
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec2 aTexCoords;

out vec2 TexCoords;

uniform mat4 view;
uniform mat4 projection;
uniform vec3 bubblePos;
uniform float bubbleSize;
uniform vec3 camRight;
uniform vec3 camUp;

void main() {
    TexCoords = aTexCoords;
    
    // Billboard: expand quad in camera-aligned directions
    vec3 worldPos = bubblePos 
                  + camRight * aPos.x * bubbleSize
                  + camUp * aPos.y * bubbleSize;
    
    gl_Position = projection * view * vec4(worldPos, 1.0);
}
