#version 330 core
out vec4 FragColor;

in vec3 FragPos;
in vec3 Normal;
in vec2 TexCoords;
in mat3 TBN;
in vec4 FragPosLightSpace;

uniform vec3 lightDir;
uniform vec3 viewPos;

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

void main() {
    float y = FragPos.y;
    
    vec2 uv = TexCoords * 150.0;
    
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

    vec3 ambient = vec3(0.15, 0.2, 0.25) * baseColor;
    
    vec3 finalColor = ambient + (1.0 - shadow) * (diffuse + specular);

    // Mgła podwodna — ograniczona widoczność jak w polskim jeziorze
    float dist = length(viewPos - FragPos);
    vec3 fogColor = vec3(0.05, 0.12, 0.08);
    float fogFactor = 0.0;
    
    if (viewPos.y > 64.0) {
        // Kamera nad wodą
        if (FragPos.y < 64.0) {
            // Patrzymy pod wodę - dno zanika w mroku (mętna woda)
            float depth = 64.0 - FragPos.y;
            fogFactor = clamp(depth / 10.0, 0.0, 1.0);
        }
        // Na lądzie (FragPos.y >= 64.0) fogFactor zostaje 0.0
    } else {
        // Kamera pod wodą - mgła dystansowa
        fogFactor = clamp((dist - 5.0) / 30.0, 0.0, 1.0);
    }
    
    finalColor = mix(finalColor, fogColor, fogFactor);

    FragColor = vec4(finalColor, 1.0);
}
