#version 330 core
out vec4 FragColor;

in vec3 FragPos;
in vec3 Normal;
in vec2 TexCoords;
in mat3 TBN;
in vec4 FragPosLightSpace;

uniform vec3 lightDir;
uniform vec3 viewPos;
uniform float time;

uniform sampler2D splatMap;
uniform sampler2D texMud;
uniform sampler2D texSoil;
uniform sampler2D texGrass;

uniform sampler2D normMud;
uniform sampler2D normSoil;
uniform sampler2D normGrass;

uniform sampler2D roughMud;
uniform sampler2D roughGrass;

uniform sampler2D shadowMap;

float ShadowCalculation(vec4 fragPosLightSpace, vec3 normal, vec3 lightDir) {
    vec3 projCoords = fragPosLightSpace.xyz / fragPosLightSpace.w;
    projCoords = projCoords * 0.5 + 0.5;
    if(projCoords.z > 1.0) return 0.0;

    float currentDepth = projCoords.z;
    float bias = max(0.005 * (1.0 - dot(normal, lightDir)), 0.001);
    
    float shadow = 0.0;
    vec2 texelSize = 1.0 / textureSize(shadowMap, 0);
    for(int x = -1; x <= 1; ++x) {
        for(int y = -1; y <= 1; ++y) {
            float pcfDepth = texture(shadowMap, projCoords.xy + vec2(x, y) * texelSize).r; 
            shadow += currentDepth - bias > pcfDepth  ? 1.0 : 0.0;        
        }    
    }
    shadow /= 9.0;
    return shadow;
}

// Kaustyki podwodne — symulacja falujących plam światła na dnie
float caustics(vec3 pos, float time) {
    vec2 p = pos.xz * 0.15;
    float c1 = sin(p.x * 3.7 + time * 1.3) * cos(p.y * 2.9 + time * 0.9);
    float c2 = sin(p.x * 5.1 - time * 1.7) * cos(p.y * 4.3 + time * 1.1);
    float c3 = sin((p.x + p.y) * 2.3 + time * 0.8);
    float c = (c1 + c2 * 0.5 + c3 * 0.3) * 0.5 + 0.5;
    return smoothstep(0.3, 0.8, c);
}

void main() {
    float y = FragPos.y;
    
    // Zwiększamy krotność UV z 150 do 300, aby tekstura gruntu wydawała się drobniejsza
    vec2 uv = TexCoords * 300.0;
    
    vec3 mudColor = texture(texMud, uv).rgb;
    vec3 soilColor = texture(texSoil, uv).rgb;
    vec3 grassColor = texture(texGrass, uv).rgb;

    vec3 mudN = texture(normMud, uv).rgb * 2.0 - 1.0;
    vec3 soilN = texture(normSoil, uv).rgb * 2.0 - 1.0;
    vec3 grassN = texture(normGrass, uv).rgb * 2.0 - 1.0;

    float mudR = texture(roughMud, uv).r;
    float grassR = texture(roughGrass, uv).r;
    float soilR = 0.9; // Brak tekstury roughness dla soil
    
    vec4 splat = texture(splatMap, TexCoords);
    float splatSum = splat.r + splat.g + splat.b;
    
    vec3 baseColor;
    vec3 normalMap;
    float roughness;

    if (splatSum > 0.01) {
        baseColor = (mudColor * splat.r + soilColor * splat.g + grassColor * splat.b) / splatSum;
        normalMap = (mudN * splat.r + soilN * splat.g + grassN * splat.b) / splatSum;
        roughness = (mudR * splat.r + soilR * splat.g + grassR * splat.b) / splatSum;
    } else {
        if (y < 64.0) { baseColor = mudColor; normalMap = mudN; roughness = mudR; }
        else if (y < 65.5) { baseColor = soilColor; normalMap = soilN; roughness = soilR; }
        else { baseColor = grassColor; normalMap = grassN; roughness = grassR; }
    }

    vec3 norm = normalize(TBN * normalize(normalMap));
    vec3 lightVector = normalize(lightDir);
    vec3 viewDir = normalize(viewPos - FragPos);
    vec3 halfwayDir = normalize(lightVector + viewDir);

    // Cook-Torrance BRDF approximations for metallic = 0.0 (dielectric like mud/grass)
    float diff = max(dot(norm, lightVector), 0.0);
    vec3 diffuse = diff * baseColor;

    float spec = pow(max(dot(norm, halfwayDir), 0.0), mix(2.0, 128.0, 1.0 - roughness));
    vec3 specular = vec3(0.1) * spec * (1.0 - roughness); // slight specular

    // Shadow
    float shadow = ShadowCalculation(FragPosLightSpace, norm, lightVector);       

    vec3 ambient = vec3(0.20, 0.25, 0.18) * baseColor;
    
    vec3 finalColor = ambient + (1.0 - shadow) * (diffuse + specular);

    // === Kaustyki podwodne ===
    if (FragPos.y < 63.5 && FragPos.y > 50.0) {
        float causticsIntensity = caustics(FragPos, time);
        float depthFade = clamp((63.5 - FragPos.y) / 12.0, 0.0, 1.0);
        float causticsStrength = causticsIntensity * (1.0 - depthFade) * 0.35;
        finalColor += vec3(0.12, 0.22, 0.06) * causticsStrength * (1.0 - shadow);
    }

    // === Podwodne refleksy światła (god rays / light shafts) ===
    // Animowane słupy światła padające z góry przez wodę
    if (FragPos.y < 64.0 && viewPos.y < 64.0) {
        vec3 lightVec = normalize(lightDir);
        // Promienie zależne od pozycji XZ, powoli płynące
        float ray1 = sin(FragPos.x * 0.08 + time * 0.3) * cos(FragPos.z * 0.06 + time * 0.2);
        float ray2 = sin(FragPos.x * 0.12 - time * 0.15) * cos(FragPos.z * 0.1 + time * 0.25);
        float ray3 = sin((FragPos.x + FragPos.z) * 0.05 + time * 0.18);
        float rayPattern = (ray1 + ray2 * 0.7 + ray3 * 0.4) * 0.5 + 0.5;
        rayPattern = smoothstep(0.35, 0.85, rayPattern);
        
        // Siła promieni maleje z głębokością
        float surfaceDist = 64.0 - FragPos.y;
        float rayFade = exp(-surfaceDist * 0.12);
        
        // Kierunkowa składowa — promienie są silniejsze gdy patrzymy w stronę światła
        float viewLightDot = max(dot(normalize(viewPos - FragPos), lightVec), 0.0);
        float directional = mix(0.3, 1.0, pow(viewLightDot, 2.0));
        
        float rayStrength = rayPattern * rayFade * directional * 0.20;
        finalColor += vec3(0.15, 0.25, 0.08) * rayStrength;
    }

    // Mgła podwodna — miękka, eksponencjalna (realistyczna)
    float dist = length(viewPos - FragPos);
    vec3 fogColor = vec3(0.25, 0.45, 0.15); // Jaśniejsza, bardziej żółta zieleń jak ze zdjęcia
    float fogFactor = 0.0;
    
    if (viewPos.y > 64.0) {
        if (FragPos.y < 64.0) {
            float depth = 64.0 - FragPos.y;
            fogFactor = 1.0 - exp(-depth * 0.25);
        }
    } else {
        // Eksponencjalna mgła dystansowa — gładkie zanikanie zamiast ostrego cięcia
        float fogDensity = 0.12; // Im wyższa wartość, tym gęstsza mgła
        fogFactor = 1.0 - exp(-dist * fogDensity);
        
        // Absorpcja barw pod wodą: ciepły zielony ton
        if (FragPos.y < 64.0) {
            float waterDepth = clamp((64.0 - FragPos.y) / 15.0, 0.0, 1.0);
            finalColor.r *= mix(1.0, 0.6, waterDepth);
            finalColor.g *= mix(1.0, 1.1, waterDepth);
            finalColor.b *= mix(1.0, 0.5, waterDepth);
        }
    }
    
    finalColor = mix(finalColor, fogColor, fogFactor);

    FragColor = vec4(finalColor, 1.0);
}
