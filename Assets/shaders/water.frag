#version 330 core
out vec4 FragColor;

in vec3 FragPos;
in vec3 Normal;
in vec2 TexCoords;

uniform vec3 lightDir;
uniform vec3 viewPos;
uniform sampler2D texWater;
uniform sampler2D reflectionTex;
uniform float time;

in vec4 ClipSpace;

vec4 hash4(vec2 p) {
    return fract(sin(vec4(1.0+dot(p,vec2(37.0,17.0)), 
                          2.0+dot(p,vec2(11.0,47.0)),
                          3.0+dot(p,vec2(41.0,29.0)),
                          4.0+dot(p,vec2(23.0,31.0))))*103.0);
}

vec3 sampleNoTile(sampler2D samp, vec2 uv) {
    vec2 gridUV = uv * 0.15; 
    vec2 p = floor(gridUV);
    vec2 f = fract(gridUV);
    vec2 u = f*f*(3.0-2.0*f);
    
    float w00 = (1.0-u.x)*(1.0-u.y);
    float w10 = u.x*(1.0-u.y);
    float w01 = (1.0-u.x)*u.y;
    float w11 = u.x*u.y;
    
    vec4 h00 = hash4(p + vec2(0.0,0.0));
    vec4 h10 = hash4(p + vec2(1.0,0.0));
    vec4 h01 = hash4(p + vec2(0.0,1.0));
    vec4 h11 = hash4(p + vec2(1.0,1.0));
    
    vec2 uv00 = uv * sign(h00.zw - 0.5) + h00.xy;
    vec2 uv10 = uv * sign(h10.zw - 0.5) + h10.xy;
    vec2 uv01 = uv * sign(h01.zw - 0.5) + h01.xy;
    vec2 uv11 = uv * sign(h11.zw - 0.5) + h11.xy;
    
    vec3 c00 = texture(samp, uv00).rgb;
    vec3 c10 = texture(samp, uv10).rgb;
    vec3 c01 = texture(samp, uv01).rgb;
    vec3 c11 = texture(samp, uv11).rgb;

    return w00*c00 + w10*c10 + w01*c01 + w11*c11;
}

// Kaustyki animowane — siatka zniekształceń świetlnych na spodzie tafli
float causticPattern(vec2 uv, float t) {
    vec2 p = uv * 8.0;
    float c = 0.0;
    // Kilka warstw fal interferujących ze sobą
    c += sin(p.x * 1.3 + t * 1.1 + sin(p.y * 0.9 + t * 0.7)) * 0.5 + 0.5;
    c += sin(p.y * 1.1 - t * 0.8 + sin(p.x * 1.4 - t * 0.6)) * 0.5 + 0.5;
    c += sin((p.x + p.y) * 0.8 + t * 1.3) * 0.5 + 0.5;
    c /= 3.0;
    // Zaostrzenie — kaustyki mają ostre jasne pasy
    c = pow(c, 4.0);
    return c;
}

void main() {
    vec2 uv = TexCoords * 100.0;
    
    // Dynamiczne zniekształcenie UV zależne od czasu — symuluje falowanie
    vec2 distortion1 = vec2(
        sin(uv.x * 0.3 + time * 0.4) * 0.02,
        cos(uv.y * 0.3 + time * 0.35) * 0.02
    );
    vec2 distortion2 = vec2(
        cos(uv.x * 0.15 - time * 0.25) * 0.015,
        sin(uv.y * 0.15 + time * 0.3) * 0.015
    );
    vec2 animatedUV = uv + distortion1 + distortion2;
    
    vec3 waterTexColor = sampleNoTile(texWater, animatedUV);

    // Wylicz dynamiczny wektor normalny powierzchni wody z fal
    vec3 norm = normalize(Normal);
    float wave1 = sin(FragPos.x * 0.4 + time * 1.6) * cos(FragPos.z * 0.3 + time * 1.1);
    float wave2 = sin(FragPos.x * 0.7 - time * 0.9) * cos(FragPos.z * 0.5 + time * 0.7);
    float wave3 = sin(FragPos.x * 1.3 + FragPos.z * 0.8 + time * 2.1) * 0.3;
    vec3 waveNormal = normalize(vec3(
        -0.03 * (wave1 + wave2 * 0.5 + wave3),
        1.0,
        -0.03 * (wave1 * 0.7 - wave2 * 0.4 + wave3)
    ));
    norm = normalize(mix(norm, waveNormal, 0.7));

    vec3 viewDir = normalize(viewPos - FragPos);
    vec3 lightVector = normalize(lightDir);

    // Efekt Fresnela — pod ostrym kątem woda staje się bardziej lustrzana
    float fresnel = pow(1.0 - max(dot(norm, viewDir), 0.0), 4.0);
    fresnel = clamp(fresnel, 0.05, 0.85);
    
    // Kolor wody: głęboki zielono-niebieski jak w polskim jeziorze
    vec3 deepWaterColor = vec3(0.02, 0.08, 0.06);    // głęboka ciemna zieleń
    vec3 shallowWaterColor = vec3(0.08, 0.20, 0.15); // płytsza, jaśniejsza
    
    // Mieszanie tekstury wody z kolorem bazowym
    vec3 surfaceColor = mix(deepWaterColor, shallowWaterColor, 0.4) + waterTexColor * 0.15;
    
    // Obliczanie współrzędnych przestrzeni ekranu (NDC) dla pobrania piksela z tekstury odbicia
    vec2 ndc = (ClipSpace.xy / ClipSpace.w) / 2.0 + 0.5;
    
    // Dodajemy małe przesunięcia dla realistycznych zniekształceń fali
    vec2 distort = vec2(norm.x, norm.z) * 0.05; 
    
    // Obraz z kamery odbicia jest teraz idealnie odwrócony przez macierz skalowania -1
    vec2 reflectionTexCoords = clamp(ndc + distort, 0.001, 0.999);
    
    // Pobranie koloru z wyrenderowanej wcześniej mapy odbicia (Reflection FBO)
    vec3 skyReflectionColor = texture(reflectionTex, reflectionTexCoords).rgb;
    
    // Łączymy Fresnel: pod kątem prostym widać dno (prawie brak odbicia), pod ostrym — pełne lustro z drzewami!
    vec3 waterColor = mix(surfaceColor, skyReflectionColor, fresnel);

    // Odblask słońca (specular) — powiększony blask
    vec3 halfwayDir = normalize(lightVector + viewDir);
    float spec = pow(max(dot(norm, halfwayDir), 0.0), 512.0);
    vec3 specular = vec3(1.0, 0.95, 0.85) * spec * 1.5;

    // Dodatkowy miękki blask (sun glint) — rozproszone iskierki na falach
    float glint = pow(max(dot(norm, halfwayDir), 0.0), 32.0);
    specular += vec3(0.9, 0.85, 0.7) * glint * 0.08;

    // Oświetlenie dyfuzyjne (delikatne, bo woda jest ciemna)
    float diff = max(dot(norm, lightVector), 0.0);
    waterColor += diff * vec3(0.03, 0.06, 0.04);

    // Przezroczystość zależna od Fresnela i kąta patrzenia
    float alpha = mix(0.75, 0.95, fresnel);

    // ===================================================================
    // POD WODĄ: widok od spodu tafli — okno Snella + kaustyki
    // ===================================================================
    if (viewPos.y < 64.0) {
        // --- Okno Snella zależne od pofałdowanej fali (norm) ---
        float cosTheta = max(dot(viewDir, norm), 0.0);
        float snellCosThreshold = 0.66;
        float snellWindow = smoothstep(snellCosThreshold - 0.05, snellCosThreshold + 0.05, cosTheta);

        // Zupa zielona ze zdjęcia
        vec3 deepWater = vec3(0.23, 0.35, 0.12);
        vec3 shallowWater = vec3(0.55, 0.67, 0.25);
        vec3 darkReflection = mix(deepWater, shallowWater, 0.2); // Odbicie toni poza oknem Snell'a
        
        vec2 causticUV = FragPos.xz * 0.05;
        float caus = causticPattern(causticUV, time);
        vec3 causticColor = vec3(0.8, 1.0, 0.6) * caus * 1.5;

        // Błyski słoneczne na falkach
        vec3 halfwayDir = normalize(lightVector + viewDir);
        float spec = pow(max(dot(norm, halfwayDir), 0.0), 64.0);
        vec3 sunSparkles = vec3(1.0, 1.0, 0.8) * spec * 3.0;

        // Woda nie jest przezroczysta, dodajemy tylko światło kaustyk i słońca zmieszane z zieloną wodą
        waterColor = mix(darkReflection, causticColor + sunSparkles, snellWindow);

        specular = vec3(0.0);

        // Alpha: w bardzo brudnej wodzie tafla rzadko cokolwiek przepuszcza
        alpha = mix(0.98, 0.30, snellWindow);

        // ---- ZUPA ZIELONA NA SAMEJ TAFLI ----
        vec3 dir = normalize(FragPos - viewPos);
        vec3 fogColor = mix(deepWater, shallowWater, clamp(dir.y * 0.5 + 0.5, 0.0, 1.0));
        
        float dist = length(viewPos - FragPos);
        float fogDensity = 0.15; // Bardzo gęsta zupa
        float fogFactor = 1.0 - exp(-dist * fogDensity);

        // Im dalej, tym woda staje się jedną gęstą mgłą
        waterColor = mix(waterColor, fogColor, fogFactor);
        alpha = mix(alpha, 1.0, fogFactor);

        FragColor = vec4(waterColor, alpha);
        return;
    }

    FragColor = vec4(waterColor + specular, alpha);
}
