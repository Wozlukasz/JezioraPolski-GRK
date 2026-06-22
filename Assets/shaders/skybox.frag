#version 330 core
out vec4 FragColor;

in vec3 TexCoords;

void main() {
    vec3 dir = normalize(TexCoords);
    
    // Zgnita zieleń dla dna, mętny niebiesko-zielony u góry (klimat polskiego jeziora)
    vec3 colorTop = vec3(0.40, 0.65, 0.60); 
    vec3 colorBottom = vec3(0.05, 0.12, 0.08); 
    
    // Gradient interpolowany w zależności od kąta patrzenia (góra-dół)
    float t = clamp(dir.y * 0.5 + 0.5, 0.0, 1.0);
    vec3 finalColor = mix(colorBottom, colorTop, t);
    
    // Słońce majaczące przez mętną wodę
    vec3 sunDir = normalize(vec3(0.6, 0.9, 0.4));
    float sunDot = max(dot(dir, sunDir), 0.0);
    float sunGlow = pow(sunDot, 12.0) * 0.4;
    finalColor += vec3(0.8, 0.9, 0.7) * sunGlow;
    
    FragColor = vec4(finalColor, 1.0);
}
