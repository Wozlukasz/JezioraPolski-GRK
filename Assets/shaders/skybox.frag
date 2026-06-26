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
        // Zawsze rysuj normalne niebo bazowe jako tło pod wodą (będzie widziane przez okno Snella)
        // === Nad wodą ===
        vec3 zenithColor = vec3(0.50, 0.65, 0.80);
        vec3 horizonColor = vec3(0.85, 0.88, 0.90);
        vec3 groundColor = vec3(0.35, 0.42, 0.32);
        
        float t = dir.y;
        vec3 baseColor;
        if (t > 0.0) {
            float skyT = pow(t, 0.6); // Wolniejsze przejście do błękitu
            baseColor = mix(horizonColor, zenithColor, skyT);
        } else {
            float groundT = clamp(-t * 3.0, 0.0, 1.0);
            baseColor = mix(horizonColor, groundColor, groundT);
        }
        
        vec3 sunDir = normalize(vec3(0.6, 0.9, 0.4));
        float sunDot = max(dot(dir, sunDir), 0.0);
        
        float sunDisc = pow(sunDot, 800.0) * 3.0;
        baseColor += vec3(1.0, 0.98, 0.90) * min(sunDisc, 1.5);
        
        float sunGlow = pow(sunDot, 8.0) * 0.15;
        baseColor += vec3(0.95, 0.85, 0.60) * sunGlow;
        
        float haze = exp(-abs(dir.y) * 5.0) * 0.15;
        baseColor += vec3(0.8, 0.85, 0.88) * haze;

        // Teraz symulujemy patrzenie przez wodę na to niebo:
        float underwaterDist;
        if (dir.y > 0.0) {
            // Dystans od kamery do punktu przecięcia z taflą wody (y = 64.0)
            underwaterDist = (64.0 - viewPos.y) / max(dir.y, 0.001);
        } else {
            // Patrząc w dół (pod horyzont) patrzymy w głąb wody
            underwaterDist = 1000.0;
        }
        
        float fogDensity = 0.12 + max(0.0, 64.0 - viewPos.y) * 0.005;
        float fogFactor = 1.0 - exp(-underwaterDist * fogDensity);
        
        // Kolor mętnej wody zależy od głębokości na jakiej jesteśmy
        vec3 fogColor = mix(vec3(0.18, 0.35, 0.22), vec3(0.10, 0.24, 0.16), clamp((64.0 - viewPos.y)/30.0, 0.0, 1.0));
        
        // Smugi świetlne w wodzie
        float ray1 = sin(dir.x * 40.0 + time * 1.5) * cos(dir.z * 35.0 - time * 1.1);
        float ray2 = sin(dir.x * 25.0 - time * 0.9) * cos(dir.z * 20.0 + time * 1.3);
        float rays = smoothstep(0.6, 1.0, (ray1 + ray2) * 0.5 + 0.5);
        fogColor += vec3(0.9, 1.0, 0.8) * rays * pow(sunDot, 2.0) * 0.8;
        
        vec3 finalColor = mix(baseColor, fogColor, clamp(fogFactor, 0.0, 1.0));
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
