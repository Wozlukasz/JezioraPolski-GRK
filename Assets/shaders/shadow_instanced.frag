#version 330 core
in vec2 TexCoords;

uniform sampler2D texture_diffuse1;

void main() {
    // Uproszczony cień - zamiast czytać teksturę z kanałem alpha (co jest wolne i ujawnia płaską geometrię),
    // tworzymy proceduralne koło/elipsę na podstawie koordynatów UV.
    // Przyspiesza to znacznie renderowanie shadow mapy i wygląda bardziej naturalnie.
    vec2 centered = TexCoords * 2.0 - 1.0;
    
    // Możemy lekko spłaszczyć elipsę w pionie (y), żeby lepiej symulowała objętość
    centered.y *= 1.2; 
    
    if (length(centered) > 1.0) {
        discard;
    }
}
