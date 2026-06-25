#version 330 core
out vec4 FragColor;

in vec3 TexCoords;

uniform vec3 viewPos;
uniform samplerCube skyboxCubemap;
uniform bool useCubemap;

void main() {
    vec3 dir = normalize(TexCoords);
    
    bool underwater = (viewPos.y < 64.0);
    
    if (underwater && useCubemap) {
        // === Pod wodą: sample z cubemapy ===
        vec3 cubemapColor = texture(skyboxCubemap, dir).rgb;
        FragColor = vec4(cubemapColor, 1.0);
    } else if (underwater) {
        // === Pod wodą: proceduralny fallback (Generowanie Cubemapy) ===
        vec3 colorTop = vec3(0.06, 0.25, 0.18); // Zmierzchające światło z góry
        vec3 colorMid = vec3(0.05, 0.20, 0.15); // Horyzont - MUST MATCH FOG COLOR!
        vec3 colorBottom = vec3(0.02, 0.10, 0.08); // Głębia - bardzo ciemna

        float t = clamp(dir.y * 0.5 + 0.5, 0.0, 1.0);
        vec3 finalColor;
        if (t > 0.5) {
            finalColor = mix(colorMid, colorTop, (t - 0.5) * 2.0);
        } else {
            finalColor = mix(colorBottom, colorMid, t * 2.0);
        }
        
        // Efekt Snella
        vec3 sunDir = normalize(vec3(0.6, 0.9, 0.4));
        float snellAngle = max(dir.y, 0.0);
        float snellCone = smoothstep(0.35, 0.8, snellAngle);
        
        vec3 snellColor = vec3(0.35, 0.60, 0.30);
        finalColor = mix(finalColor, snellColor, snellCone * 0.5);
        
        float sunDot = max(dot(dir, sunDir), 0.0);
        float sunGlow = pow(sunDot, 16.0) * 0.6 * snellCone;
        finalColor += vec3(0.5, 0.6, 0.3) * sunGlow;
        
        float scatter = pow(max(dot(dir, sunDir), 0.0), 3.0) * 0.08;
        finalColor += vec3(0.15, 0.20, 0.08) * scatter;
        
        FragColor = vec4(finalColor, 1.0);
    } else {
        // === Nad wodą ===
        vec3 zenithColor = vec3(0.25, 0.47, 0.75);
        vec3 horizonColor = vec3(0.70, 0.80, 0.88);
        vec3 groundColor = vec3(0.35, 0.42, 0.32);
        
        float t = dir.y;
        vec3 finalColor;
        if (t > 0.0) {
            float skyT = pow(t, 0.5);
            finalColor = mix(horizonColor, zenithColor, skyT);
        } else {
            float groundT = clamp(-t * 3.0, 0.0, 1.0);
            finalColor = mix(horizonColor, groundColor, groundT);
        }
        
        vec3 sunDir = normalize(vec3(0.6, 0.9, 0.4));
        float sunDot = max(dot(dir, sunDir), 0.0);
        
        float sunDisc = pow(sunDot, 800.0) * 3.0;
        finalColor += vec3(1.0, 0.98, 0.90) * min(sunDisc, 1.5);
        
        float sunGlow = pow(sunDot, 8.0) * 0.15;
        finalColor += vec3(0.95, 0.85, 0.60) * sunGlow;
        
        float haze = exp(-abs(dir.y) * 5.0) * 0.15;
        finalColor += vec3(0.8, 0.85, 0.88) * haze;
        
        FragColor = vec4(finalColor, 1.0);
    }
}
