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

// Flashlight
uniform bool flashlightOn;
uniform vec3 flashlightPos;
uniform vec3 flashlightDir;

const float PI = 3.14159265359;

// ============ PBR Functions (Cook-Torrance BRDF) ============

float DistributionGGX(vec3 N, vec3 H, float roughness) {
    float a = roughness * roughness;
    float a2 = a * a;
    float NdotH = max(dot(N, H), 0.0);
    float NdotH2 = NdotH * NdotH;
    
    float denom = (NdotH2 * (a2 - 1.0) + 1.0);
    denom = PI * denom * denom;
    
    return a2 / max(denom, 0.0001);
}

float GeometrySchlickGGX(float NdotV, float roughness) {
    float r = (roughness + 1.0);
    float k = (r * r) / 8.0;
    
    float denom = NdotV * (1.0 - k) + k;
    return NdotV / max(denom, 0.0001);
}

float GeometrySmith(vec3 N, vec3 V, vec3 L, float roughness) {
    float NdotV = max(dot(N, V), 0.0);
    float NdotL = max(dot(N, L), 0.0);
    float ggx2 = GeometrySchlickGGX(NdotV, roughness);
    float ggx1 = GeometrySchlickGGX(NdotL, roughness);
    
    return ggx1 * ggx2;
}

vec3 fresnelSchlick(float cosTheta, vec3 F0) {
    return F0 + (1.0 - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}

// ============ Shadow ============

float ShadowCalculation(vec4 fragPosLightSpace, vec3 normal, vec3 lightVec) {
    vec3 projCoords = fragPosLightSpace.xyz / fragPosLightSpace.w;
    projCoords = projCoords * 0.5 + 0.5;
    if(projCoords.z > 1.0) return 0.0;

    float currentDepth = projCoords.z;
    float bias = max(0.005 * (1.0 - dot(normal, lightVec)), 0.001);
    
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
    
    vec3 albedo;
    vec3 normalMap;
    float roughness;

    if (splatSum > 0.01) {
        albedo = (mudColor * splat.r + soilColor * splat.g + grassColor * splat.b) / splatSum;
        normalMap = (mudN * splat.r + soilN * splat.g + grassN * splat.b) / splatSum;
        roughness = (mudR * splat.r + soilR * splat.g + grassR * splat.b) / splatSum;
    } else {
        if (y < 64.0) { albedo = mudColor; normalMap = mudN; roughness = mudR; }
        else if (y < 65.5) { albedo = soilColor; normalMap = soilN; roughness = soilR; }
        else { albedo = grassColor; normalMap = grassN; roughness = grassR; }
    }

    // Normal mapping via TBN
    vec3 N = normalize(TBN * normalize(normalMap));
    vec3 V = normalize(viewPos - FragPos);
    vec3 L = normalize(lightDir);
    vec3 H = normalize(V + L);

    // ====== Cook-Torrance BRDF (metallic = 0.0 for terrain — dielectric) ======
    float metallic = 0.0;
    vec3 F0 = vec3(0.04); // Dielectric base reflectivity
    
    float NDF = DistributionGGX(N, H, roughness);
    float G   = GeometrySmith(N, V, L, roughness);
    vec3  F   = fresnelSchlick(max(dot(H, V), 0.0), F0);
    
    vec3 numerator = NDF * G * F;
    float denominator = 4.0 * max(dot(N, V), 0.0) * max(dot(N, L), 0.0) + 0.0001;
    vec3 specular = numerator / denominator;
    
    vec3 kS = F;
    vec3 kD = vec3(1.0) - kS;
    kD *= 1.0 - metallic;
    
    float NdotL = max(dot(N, L), 0.0);

    // Shadow
    float shadow = ShadowCalculation(FragPosLightSpace, N, L);       

    // Light color
    vec3 lightColor = vec3(1.0, 0.95, 0.85) * 2.5;
    
    vec3 Lo = (kD * albedo / PI + specular) * lightColor * NdotL;
    
    vec3 ambient = vec3(0.20, 0.25, 0.18) * albedo;
    
    vec3 finalColor = ambient + (1.0 - shadow) * Lo;

    // ====== Flashlight (spot light) ======
    if (flashlightOn) {
        vec3 flashToFrag = normalize(flashlightPos - FragPos);
        float flashDist = length(flashlightPos - FragPos);
        float attenuation = 1.0 / (1.0 + 0.09 * flashDist + 0.032 * flashDist * flashDist);
        
        float theta = dot(flashToFrag, normalize(-flashlightDir));
        float innerCutoff = cos(radians(12.5));
        float outerCutoff = cos(radians(25.0));
        float epsilon = innerCutoff - outerCutoff;
        float intensity = clamp((theta - outerCutoff) / epsilon, 0.0, 1.0);
        
        if (theta > outerCutoff) {
            vec3 flashH = normalize(flashToFrag + V);
            float flashNDF = DistributionGGX(N, flashH, roughness);
            float flashG = GeometrySmith(N, V, flashToFrag, roughness);
            vec3 flashF = fresnelSchlick(max(dot(flashH, V), 0.0), F0);
            vec3 flashSpec = (flashNDF * flashG * flashF) / max(4.0 * max(dot(N, V), 0.0) * max(dot(N, flashToFrag), 0.0) + 0.0001, 0.0001);
            
            vec3 flashKD = (vec3(1.0) - flashF) * (1.0 - metallic);
            float flashNdotL = max(dot(N, flashToFrag), 0.0);
            
            vec3 flashColor = vec3(1.0, 0.9, 0.7) * 3.0;
            finalColor += (flashKD * albedo / PI + flashSpec) * flashColor * flashNdotL * attenuation * intensity;
        }
    }

    // === Kaustyki podwodne ===
    if (FragPos.y < 63.5 && FragPos.y > 50.0) {
        float causticsIntensity = caustics(FragPos, time);
        float depthFade = clamp((63.5 - FragPos.y) / 12.0, 0.0, 1.0);
        float causticsStrength = causticsIntensity * (1.0 - depthFade) * 0.35;
        finalColor += vec3(0.12, 0.22, 0.06) * causticsStrength * (1.0 - shadow);
    }

    // === Podwodne refleksy światła (god rays / light shafts) ===
    if (FragPos.y < 64.0 && viewPos.y < 64.0) {
        vec3 lightVec = normalize(lightDir);
        float ray1 = sin(FragPos.x * 0.08 + time * 0.3) * cos(FragPos.z * 0.06 + time * 0.2);
        float ray2 = sin(FragPos.x * 0.12 - time * 0.15) * cos(FragPos.z * 0.1 + time * 0.25);
        float ray3 = sin((FragPos.x + FragPos.z) * 0.05 + time * 0.18);
        float rayPattern = (ray1 + ray2 * 0.7 + ray3 * 0.4) * 0.5 + 0.5;
        rayPattern = smoothstep(0.35, 0.85, rayPattern);
        
        float surfaceDist = 64.0 - FragPos.y;
        float rayFade = exp(-surfaceDist * 0.12);
        
        float viewLightDot = max(dot(normalize(viewPos - FragPos), lightVec), 0.0);
        float directional = mix(0.3, 1.0, pow(viewLightDot, 2.0));
        
        float rayStrength = rayPattern * rayFade * directional * 0.20;
        finalColor += vec3(0.15, 0.25, 0.08) * rayStrength;
    }

    // Mgła podwodna — miękka, eksponencjalna (realistyczna)
    float dist = length(viewPos - FragPos);
    vec3 fogColor = vec3(0.05, 0.20, 0.15); // Mroczniejsza, realistyczna zieleń
    float fogFactor = 0.0;
    
    if (viewPos.y > 64.0) {
        if (FragPos.y < 64.0) {
            float depth = 64.0 - FragPos.y;
            fogFactor = 1.0 - exp(-depth * 0.25);
        } else {
            // Mgła atmosferyczna (nad wodą) - horyzont wtapia się w niebo
            fogColor = vec3(0.53, 0.81, 0.92); // Kolor jasnego nieba (z main.cpp)
            float fogDensity = 0.003; // Zgodnie z gęstością z instanced_plant.frag
            fogFactor = 1.0 - exp(-dist * fogDensity);
        }
    } else {
        // Eksponencjalna mgła dystansowa — gładkie zanikanie zamiast ostrego cięcia
        float fogDensity = 0.04; // Im wyższa wartość, tym gęstsza mgła
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
