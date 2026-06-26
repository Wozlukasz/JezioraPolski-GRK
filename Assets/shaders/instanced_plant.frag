#version 330 core
out vec4 FragColor;

in vec2 TexCoords;
in vec3 Normal;
in vec3 FragPos;
in vec4 FragPosLightSpace;

uniform sampler2D texture_diffuse1;
uniform vec3 lightDir;
uniform vec3 viewPos;
uniform sampler2D shadowMap;
uniform float time;

// Flashlight
uniform bool flashlightOn;
uniform vec3 flashlightPos;
uniform vec3 flashlightDir;

uniform float clipY;
uniform int clipMode;
uniform bool isReflectionPass;

const float PI = 3.14159265359;

// ============ PBR Functions ============

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
    return GeometrySchlickGGX(NdotV, roughness) * GeometrySchlickGGX(NdotL, roughness);
}

vec3 fresnelSchlick(float cosTheta, vec3 F0) {
    return F0 + (1.0 - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}

uniform int lodFadeMode;
uniform float lodThreshold;
uniform float lodFadeBand;
uniform bool isTree;

void main() {
    if (clipMode == 1 && FragPos.y < clipY) discard;

    vec4 texColor = texture(texture_diffuse1, TexCoords);
    
    // Mipmapy powodują rozmycie kanału alpha, przez co cienkie modele tataraku znikają!
    // Obniżamy próg dla płaskich modeli i modeli fade-out
    float alphaThreshold;
    if (lodFadeMode == 2 || lodFadeMode == 3) alphaThreshold = 0.25;
    else if (lodFadeMode == 1) alphaThreshold = 0.5;
    else alphaThreshold = 0.8;
    if(texColor.a < alphaThreshold) discard;
    
    // ======== LOD Crossfade (Screen-door dithering) ========
    if (lodFadeMode == 1 || lodFadeMode == 2) {
        float dist = length(viewPos - FragPos);
        float alpha = 1.0;
        
        if (lodFadeMode == 1) {
            // Model detaliczny znika: od 1.0 przy lodThreshold do 0.0 przy lodThreshold+lodFadeBand
            alpha = 1.0 - clamp((dist - lodThreshold) / lodFadeBand, 0.0, 1.0);
        } else if (lodFadeMode == 2) {
            // Model płaski pojawia się: od 0.0 przy (lodThreshold-lodFadeBand) do 1.0 przy lodThreshold
            // Strefa nakładki zaczyna się FADE_BAND przed progiem LOD
            alpha = clamp((dist - (lodThreshold - lodFadeBand)) / lodFadeBand, 0.0, 1.0);
        }
        
        if (alpha < 1.0) {
            // Znacznie szybszy ordered dithering (szachownica), zysk FPS zamiast sin()
            ivec2 pos = ivec2(gl_FragCoord.xy);
            float dither = ((pos.x & 1) ^ (pos.y & 1)) == 1 ? 0.75 : 0.25;
            if (alpha < dither) {
                discard;
            }
        }
    }
    // ========================================================
    
    vec3 albedo = texColor.rgb;
    
    if (isTree) {
        // Ciemna sosna - mocne przyciemnienie i nadanie leśnego, głębokiego zielonego koloru.
        // To sprawia, że białe artefakty krawędzi (z anti-aliasingu) naturalnie stają się ciemnozielone.
        albedo *= vec3(0.2, 0.35, 0.15);
    }
    float roughness = 0.8;  // Plants are rough
    float metallic = 0.0;   // Plants are dielectric
    
    vec3 norm = normalize(Normal);
    vec3 V = normalize(viewPos - FragPos);
    vec3 L = normalize(lightDir);
    vec3 H = normalize(V + L);
    
    vec3 finalColor;
    
    // Optymalizacja wydajnościowa GPU: Modele płaskie (lodFadeMode == 2) używają uproszczonego oświetlenia
    if (lodFadeMode == 2) {
        float NdotL_front = max(dot(norm, L), 0.0);
        float NdotL_back = max(dot(-norm, L), 0.0);
        float NdotL = NdotL_front + NdotL_back * 0.4;
        
        vec3 lightColor = vec3(1.0, 0.95, 0.85) * 2.5;
        vec3 ambient = vec3(0.50, 0.55, 0.45) * albedo;
        vec3 diffuse = albedo / PI * lightColor * NdotL;
        
        finalColor = ambient + diffuse;
    } else {
        // ====== Cook-Torrance BRDF ======
        vec3 F0 = vec3(0.04);
        
        float NDF = DistributionGGX(norm, H, roughness);
        float G   = GeometrySmith(norm, V, L, roughness);
        vec3  F   = fresnelSchlick(max(dot(H, V), 0.0), F0);
        
        vec3 numerator = NDF * G * F;
        float denominator = 4.0 * max(dot(norm, V), 0.0) * max(dot(norm, L), 0.0) + 0.0001;
        vec3 specular = numerator / denominator;
        
        vec3 kS = F;
        vec3 kD = vec3(1.0) - kS;
        kD *= 1.0 - metallic;
        
        // Two-sided lighting for leaves
        float NdotL_front = max(dot(norm, L), 0.0);
        float NdotL_back = max(dot(-norm, L), 0.0);
        float NdotL = NdotL_front + NdotL_back * 0.4;
        
        // Subsurface Scattering (sunlight through leaves)
        float sss = pow(max(dot(V, -L), 0.0), 3.0) * 0.40;
        vec3 sssColor = albedo * vec3(0.9, 1.0, 0.4);
        
        vec3 lightColor = vec3(1.0, 0.95, 0.85) * 2.5;
        vec3 Lo = (kD * albedo / PI + specular) * lightColor * NdotL;
        
        float shadow = 0.0;
        // Wysoki ambient — rośliny są oświetlone rozproszonym światłem ze wszystkich kierunków
        vec3 ambient = vec3(0.50, 0.55, 0.45) * albedo;
        
        finalColor = ambient + (1.0 - shadow) * Lo + sss * sssColor;
    }
    
    // ====== Flashlight ======
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
            float flashNdotL = max(dot(norm, flashToFrag), 0.0) + max(dot(-norm, flashToFrag), 0.0) * 0.4;
            vec3 flashColor = vec3(1.0, 0.9, 0.7) * 3.0;
            finalColor += albedo * flashColor * flashNdotL * attenuation * intensity / PI;
        }
    }
    
    // === Podwodne refleksy światła (god rays) na roślinach ===
    if (FragPos.y < 64.0 && viewPos.y < 64.0) {
        float ray1 = sin(FragPos.x * 0.08 + time * 0.3) * cos(FragPos.z * 0.06 + time * 0.2);
        float ray2 = sin(FragPos.x * 0.12 - time * 0.15) * cos(FragPos.z * 0.1 + time * 0.25);
        float ray3 = sin((FragPos.x + FragPos.z) * 0.05 + time * 0.18);
        float rayPattern = (ray1 + ray2 * 0.7 + ray3 * 0.4) * 0.5 + 0.5;
        rayPattern = smoothstep(0.35, 0.85, rayPattern);
        
        float surfaceDist = 64.0 - FragPos.y;
        float rayFade = exp(-surfaceDist * 0.10);
        
        float viewLightDot = max(dot(V, normalize(lightDir)), 0.0);
        float directional = mix(0.3, 1.0, pow(viewLightDot, 2.0));
        
        float rayStrength = rayPattern * rayFade * directional * 0.18;
        finalColor += vec3(0.20, 0.25, 0.22) * rayStrength;
    }
    
    // ======== MGŁA PODWODNA ========
    float dist = length(viewPos - FragPos);
    vec3 fogColor = vec3(0.18, 0.35, 0.22);
    float fogFactor = 0.0;
    
    if (isReflectionPass) {
        fogColor = vec3(0.53, 0.81, 0.92);
        fogFactor = 1.0 - exp(-dist * 0.003);
    } else if (viewPos.y < 64.0) {
        // ---- ZUPA ZIELONA (fotorealizm wg zdjęcia) ----
        vec3 dir = normalize(FragPos - viewPos);
        
        vec3 deepWater = vec3(0.23, 0.35, 0.12);
        vec3 shallowWater = vec3(0.55, 0.67, 0.25);
        fogColor = mix(deepWater, shallowWater, clamp(dir.y * 0.5 + 0.5, 0.0, 1.0));
        
        float fogDensity = 0.15;
        
        if (FragPos.y > 64.0) {
            float underwaterDist = (64.0 - viewPos.y) / max(dir.y, 0.001);
            fogFactor = 1.0 - exp(-underwaterDist * fogDensity);
        } else {
            float dist = length(FragPos - viewPos);
            fogFactor = 1.0 - exp(-dist * fogDensity);
        }
    } else {
        // Kamerzysta jest nad wodą
        if (FragPos.y > 64.0) {
            fogColor = vec3(0.53, 0.81, 0.92);
            fogFactor = 1.0 - exp(-dist * 0.003);
        } else {
            fogFactor = 1.0 - exp(-(64.0 - FragPos.y) * 0.12);
            float waterDepth = clamp((64.0 - FragPos.y) / 30.0, 0.0, 1.0);
            fogColor = mix(vec3(0.18, 0.35, 0.22), vec3(0.10, 0.24, 0.16), waterDepth);
        }
    }
    
    finalColor = mix(finalColor, fogColor, fogFactor);
    
    // Kluczowa poprawka: Wymuszamy alpha = 1.0. Przezroczystość roślin i tak jest
    // obcinana (discard) testem alpha. Wypuszczenie mniejszego alpha powodowało
    // mieszanie się (blending) z wyczyszczonym ciemnym tłem jeziora i tworzyło ciemne krawędzie!
    FragColor = vec4(finalColor, 1.0);
}
