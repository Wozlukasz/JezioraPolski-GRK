#version 330 core
in vec2 TexCoords;

uniform sampler2D texture_diffuse1;

void main() {
    if(texture(texture_diffuse1, TexCoords).a < 0.1) discard;
}
