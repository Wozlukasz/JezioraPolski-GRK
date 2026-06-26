#version 330 core
out vec4 FragColor;

in vec2 TexCoord;

uniform sampler2D hudTex;
uniform vec4 color;    // Kolor bazowy (dla tła: rgba, dla tekstu: rgba)
uniform int useTexture; // 0 = solid color, 1 = texture (font atlas)

void main() {
    if (useTexture == 1) {
        // Font atlas: czerwony kanał = pokrycie glypha
        float alpha = texture(hudTex, TexCoord).r;
        FragColor = vec4(color.rgb, color.a * alpha);
    } else {
        FragColor = color;
    }
}
