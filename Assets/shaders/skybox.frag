#version 330 core
out vec4 FragColor;

in vec3 TexCoords;

uniform vec3 viewPos;
uniform samplerCube skyboxCubemap;
uniform bool useCubemap;
uniform bool isReflectionPass;
uniform float time;

void main() {
    vec3 dir = normalize(TexCoords);
    
    // Gdy renderujemy odbicie tafli z podziemi, wymuś widok nadwodny
    bool underwater = (viewPos.y < 64.0) && !isReflectionPass;
    
    if (underwater) {
        // ---- ZUPA ZIELONA (fotorealizm wg zdjęcia) ----
        vec3 deepWater = vec3(0.23, 0.35, 0.12);    // Ciemniejsza zieleń na dole
        vec3 shallowWater = vec3(0.55, 0.67, 0.25); // Jasna, żółtawa zieleń u góry
        vec3 fogColor = mix(deepWater, shallowWater, clamp(dir.y * 0.5 + 0.5, 0.0, 1.0));

        // Światło z powierzchni przebijające przez mroczną zupę (rozmyta żółtawa poświata)
        vec3 filteredSky = vec3(0.65, 0.75, 0.35);
        
        vec3 sunDir = normalize(vec3(0.6, 0.9, 0.4));
        float sunDot = max(dot(dir, sunDir), 0.0);
        float sunDisc = pow(sunDot, 16.0) * 2.0; 
        filteredSky += vec3(1.0, 1.0, 0.8) * sunDisc;

        float underwaterDist = (dir.y > 0.0) ? (64.0 - viewPos.y) / max(dir.y, 0.001) : 1000.0;
        
        // BARDZO gęsta mgła!
        float fogDensity = 0.15;
        float fogFactor = 1.0 - exp(-underwaterDist * fogDensity);
        
        vec3 finalColor = mix(filteredSky, fogColor, clamp(fogFactor, 0.0, 1.0));
        FragColor = vec4(finalColor, 1.0);
    } else {
        // === Nad wodą ===
        // Kolory zaktualizowane dla pochmurnego/mglistego polskiego nieba ze zdjęcia
        vec3 zenithColor = vec3(0.50, 0.65, 0.80);
        vec3 horizonColor = vec3(0.85, 0.88, 0.90);
        vec3 groundColor = vec3(0.35, 0.42, 0.32);
        
        float t = dir.y;
        vec3 finalColor;
        if (t > 0.0) {
            float skyT = pow(t, 0.6); // Wolniejsze przejście do błękitu
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
