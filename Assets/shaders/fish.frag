#version 330 core
out vec4 FragColor;

in vec3 FragPos;
in vec3 Normal;
in vec2 TexCoords;
in mat3 TBN;
in vec4 FragPosLightSpace;

uniform sampler2D albedoMap;     // unit 0
uniform sampler2D normalMap;     // unit 1
uniform sampler2D roughnessMap;  // unit 2
uniform sampler2D shadowMap;     // unit 3

uniform vec3 lightDir;
uniform vec3 viewPos;
uniform float time;

// Flashlight
uniform bool flashlightOn;
uniform vec3 flashlightPos;
uniform vec3 flashlightDir;

const float PI = 3.14159265359;

// ============ PBR Functions (Cook-Torrance BRDF) ============

// GGX/Trowbridge-Reitz Normal Distribution Function
float DistributionGGX(vec3 N, vec3 H, float roughness) {
    float a = roughness * roughness;
    float a2 = a * a;
    float NdotH = max(dot(N, H), 0.0);
    float NdotH2 = NdotH * NdotH;
    
    float denom = (NdotH2 * (a2 - 1.0) + 1.0);
    denom = PI * denom * denom;
    
    return a2 / max(denom, 0.0001);
}

// Smith's Geometry Function (Schlick-GGX)
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

// Fresnel-Schlick Approximation
vec3 fresnelSchlick(float cosTheta, vec3 F0) {
    return F0 + (1.0 - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}

// ============ Shadow Calculation ============

float ShadowCalculation(vec4 fragPosLightSpace, vec3 normal, vec3 lightVec) {
    vec3 projCoords = fragPosLightSpace.xyz / fragPosLightSpace.w;
    projCoords = projCoords * 0.5 + 0.5;
    if(projCoords.z > 1.0) return 0.0;

    float currentDepth = projCoords.z;
    float bias = max(0.004 * (1.0 - dot(normal, lightVec)), 0.0005);
    
    float shadow = 0.0;
    vec2 texelSize = 1.0 / textureSize(shadowMap, 0);
    // 3x3 PCF — 9 próbek (optymalizacja wydajnościowa)
    for(int x = -1; x <= 1; ++x) {
        for(int y = -1; y <= 1; ++y) {
            float pcfDepth = texture(shadowMap, projCoords.xy + vec2(x, y) * texelSize).r; 
            shadow += currentDepth - bias > pcfDepth ? 1.0 : 0.0;
        }    
    }
    shadow /= 9.0;
    return shadow;
}

// ============ Kaustyki ============

float caustics(vec3 pos, float t) {
    vec2 p = pos.xz * 0.15;
    float c1 = sin(p.x * 3.7 + t * 1.3) * cos(p.y * 2.9 + t * 0.9);
    float c2 = sin(p.x * 5.1 - t * 1.7) * cos(p.y * 4.3 + t * 1.1);
    float c3 = sin((p.x + p.y) * 2.3 + t * 0.8);
    float c = (c1 + c2 * 0.5 + c3 * 0.3) * 0.5 + 0.5;
    return smoothstep(0.3, 0.8, c);
}

void main() {
    // Sample PBR textures
    vec3 albedo = texture(albedoMap, TexCoords).rgb;
    float roughness = texture(roughnessMap, TexCoords).r;
    float metallic = 0.3; // Fish scales are somewhat metallic

    // Normal mapping
    vec3 normalTex = texture(normalMap, TexCoords).rgb;
    normalTex = normalTex * 2.0 - 1.0;
    vec3 N = normalize(TBN * normalTex);
    
    vec3 V = normalize(viewPos - FragPos);
    vec3 L = normalize(lightDir);
    vec3 H = normalize(V + L);
    
    // ====== Cook-Torrance BRDF ======
    vec3 F0 = vec3(0.04);
    F0 = mix(F0, albedo, metallic);
    
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
    
    // Light color (warm sunlight)
    vec3 lightColor = vec3(1.0, 0.95, 0.85) * 2.5;
    
    vec3 Lo = (kD * albedo / PI + specular) * lightColor * NdotL;
    
    // Ambient (underwater scattered light) - podniesiony, żeby tekstura była widoczna w cieniu
    vec3 ambient = vec3(0.50, 0.55, 0.60) * albedo;
    
    vec3 finalColor = ambient + (1.0 - shadow) * Lo;
    
    // ====== Flashlight (spot light) ======
    if (flashlightOn) {
        vec3 flashDir = normalize(flashlightPos - FragPos);
        float flashDist = length(flashlightPos - FragPos);
        float attenuation = 1.0 / (1.0 + 0.09 * flashDist + 0.032 * flashDist * flashDist);
        
        float theta = dot(flashDir, normalize(-flashlightDir));
        float innerCutoff = cos(radians(12.5));
        float outerCutoff = cos(radians(25.0));
        float epsilon = innerCutoff - outerCutoff;
        float intensity = clamp((theta - outerCutoff) / epsilon, 0.0, 1.0);
        
        if (theta > outerCutoff) {
            vec3 flashH = normalize(flashDir + V);
            float flashNDF = DistributionGGX(N, flashH, roughness);
            float flashG = GeometrySmith(N, V, flashDir, roughness);
            vec3 flashF = fresnelSchlick(max(dot(flashH, V), 0.0), F0);
            vec3 flashSpec = (flashNDF * flashG * flashF) / max(4.0 * max(dot(N, V), 0.0) * max(dot(N, flashDir), 0.0) + 0.0001, 0.0001);
            
            vec3 flashKD = (vec3(1.0) - flashF) * (1.0 - metallic);
            float flashNdotL = max(dot(N, flashDir), 0.0);
            
            vec3 flashColor = vec3(1.0, 0.9, 0.7) * 3.0;
            finalColor += (flashKD * albedo / PI + flashSpec) * flashColor * flashNdotL * attenuation * intensity;
        }
    }
    
    // ====== Kaustyki podwodne ======
    if (FragPos.y < 63.5) {
        float ci = caustics(FragPos, time);
        float depthFade = clamp((63.5 - FragPos.y) / 30.0, 0.0, 1.0);
        float cs = ci * (1.0 - depthFade) * 0.45;
        finalColor += vec3(0.15, 0.28, 0.10) * cs * (1.0 - shadow);
    }
    
    // ====== God rays ======
    if (FragPos.y < 64.0 && viewPos.y < 64.0) {
        float ray1 = sin(FragPos.x * 0.08 + time * 0.3) * cos(FragPos.z * 0.06 + time * 0.2);
        float ray2 = sin(FragPos.x * 0.12 - time * 0.15) * cos(FragPos.z * 0.1 + time * 0.25);
        float ray3 = sin((FragPos.x + FragPos.z) * 0.05 + time * 0.18);
        float rayPattern = (ray1 + ray2 * 0.7 + ray3 * 0.4) * 0.5 + 0.5;
        rayPattern = smoothstep(0.35, 0.85, rayPattern);
        
        float surfaceDist = 64.0 - FragPos.y;
        float rayFade = exp(-surfaceDist * 0.12);
        
        float viewLightDot = max(dot(V, L), 0.0);
        float directional = mix(0.3, 1.0, pow(viewLightDot, 2.0));
        
        float rayStrength = rayPattern * rayFade * directional * 0.18;
        finalColor += vec3(0.20, 0.25, 0.22) * rayStrength;
    }
    
    // ====== Underwater fog ======
    float dist = length(viewPos - FragPos);
    vec3 fogColor = vec3(0.12, 0.28, 0.18); // Bardziej realistyczny, mętno-zielonkawy
    float fogFactor = 0.0;
    
    if (viewPos.y > 64.0) {
        if (FragPos.y < 64.0) {
            float depth = 64.0 - FragPos.y;
            fogFactor = 1.0 - exp(-depth * 0.12);
        }
    } else {
        float baseDensity = 0.05; // Poprawiony realizm głębi, ok. 15m do zamglenia
        float depthDensity = max(0.0, 64.0 - viewPos.y) * 0.005;
        float fogDensity = baseDensity + depthDensity;
        fogFactor = 1.0 - exp(-dist * fogDensity);
    }
    
    // Absorpcja barw pod wodą — zredukowane by dno było jaśniejsze
    if (FragPos.y < 64.0) {
        float waterDepth = clamp((64.0 - FragPos.y) / 30.0, 0.0, 1.0);
        fogColor = mix(vec3(0.18, 0.35, 0.22), vec3(0.10, 0.24, 0.16), waterDepth);
    }
    
    finalColor = mix(finalColor, fogColor, fogFactor);
    
    FragColor = vec4(finalColor, 1.0);
}
