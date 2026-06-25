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

    // Pod wodą — woda jest nieprzezroczysta (patrzymy od spodu na taflę)
    if (viewPos.y < 64.0) {
        waterColor = vec3(0.04, 0.10, 0.07);
        specular = vec3(0.0);
        alpha = 0.85;
        
        // Promień Snella: słońce widoczne jako jasna plama z dołu
        vec3 refractedSun = refract(-lightVector, vec3(0.0, -1.0, 0.0), 1.0/1.33);
        float underSunGlow = pow(max(dot(viewDir, -refractedSun), 0.0), 40.0);
        waterColor += vec3(0.15, 0.20, 0.12) * underSunGlow;
    }

    FragColor = vec4(waterColor + specular, alpha);
    
    // Mgła podwodna aplikowana na taflę wody, aby w oddali lub na głębokości ukrywała obiekty na zewnątrz (niebo/drzewa)
    if (viewPos.y < 64.0) {
        vec3 fogColor = vec3(0.05, 0.20, 0.15); // Zgodnie z main.cpp i innymi shaderami
        float dist = length(viewPos - FragPos);
        float baseDensity = 0.06;
        float depthDensity = max(0.0, 64.0 - viewPos.y) * 0.01;
        float fogDensity = baseDensity + depthDensity;
        float fogFactor = 1.0 - exp(-dist * fogDensity);
        
        // Zmieszanie ostatecznego koloru z mgłą i wymuszenie pełnego krycia w oddali
        FragColor.rgb = mix(FragColor.rgb, fogColor, fogFactor);
        FragColor.a = mix(FragColor.a, 1.0, fogFactor);
    }
}
