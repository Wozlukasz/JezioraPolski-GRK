#version 330 core
out vec4 FragColor;

in vec2 TexCoord;

uniform sampler2D hudTex;
uniform vec4 color;
uniform int useTexture;

void main() {
    if (useTexture == 1) {
        // Czytamy kanal R (atlas czcionki). Swizzle GL_RED,RED,RED,RED
        // sprawia ze texture().rgba = (r,r,r,r), wiec .r zawsze dziala.
        float coverage = texture(hudTex, TexCoord).r;
        if (coverage < 0.02) discard;
        FragColor = vec4(color.rgb, color.a * coverage);
    } else {
        FragColor = color;
    }
}
