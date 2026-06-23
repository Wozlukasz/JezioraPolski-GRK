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

void main() {
    vec4 texColor = texture(texture_diffuse1, TexCoords);
    if(texColor.a < 0.1) discard;
    
    vec3 norm = normalize(Normal);
    vec3 lightVector = normalize(lightDir);
    vec3 viewDir = normalize(viewPos - FragPos);
    
    // Oświetlenie dyfuzyjne dwustronne
    float diff = max(dot(norm, lightVector), 0.0);
    float backDiff = max(dot(-norm, lightVector), 0.0);
    
    // Subsurface Scattering (żółtawo-zielone światło słoneczne przez liście)
    float sss = pow(max(dot(viewDir, -lightVector), 0.0), 3.0) * 0.40;
    vec3 sssColor = texColor.rgb * vec3(0.9, 1.0, 0.4);
    
    vec3 diffuse = (diff + backDiff * 0.5) * vec3(1.0);
    
    float shadow = 0.0;
    // Znacznie jaśniejszy ambient, by wodorosty były soczyście widoczne jak na zdjęciu
    vec3 ambient = vec3(0.50, 0.55, 0.40) * texColor.rgb;
    
    vec3 finalColor = ambient + (1.0 - shadow) * diffuse * texColor.rgb + sss * sssColor;
    
    // === Podwodne refleksy światła (god rays) na roślinach ===
    if (FragPos.y < 64.0 && viewPos.y < 64.0) {
        float ray1 = sin(FragPos.x * 0.08 + time * 0.3) * cos(FragPos.z * 0.06 + time * 0.2);
        float ray2 = sin(FragPos.x * 0.12 - time * 0.15) * cos(FragPos.z * 0.1 + time * 0.25);
        float ray3 = sin((FragPos.x + FragPos.z) * 0.05 + time * 0.18);
        float rayPattern = (ray1 + ray2 * 0.7 + ray3 * 0.4) * 0.5 + 0.5;
        rayPattern = smoothstep(0.35, 0.85, rayPattern);
        
        float surfaceDist = 64.0 - FragPos.y;
        float rayFade = exp(-surfaceDist * 0.10);
        
        float viewLightDot = max(dot(viewDir, normalize(lightDir)), 0.0);
        float directional = mix(0.3, 1.0, pow(viewLightDot, 2.0));
        
        float rayStrength = rayPattern * rayFade * directional * 0.18;
        finalColor += vec3(0.12, 0.22, 0.06) * rayStrength;
    }
    
    // Korekta barw pod wodą
    if (FragPos.y < 64.0) {
        float waterDepth = clamp((64.0 - FragPos.y) / 15.0, 0.0, 1.0);
        finalColor.r *= mix(1.0, 0.6, waterDepth);
        finalColor.g *= mix(1.0, 1.1, waterDepth);
        finalColor.b *= mix(1.0, 0.5, waterDepth);
    }
    
    // Mgła podwodna — jasny, ciepły, żółtawy zielony (jak na zdjęciu)
    float dist = length(viewPos - FragPos);
    vec3 fogColor = vec3(0.25, 0.45, 0.15);
    float fogFactor = 0.0;
    
    if (viewPos.y > 64.0) {
        if (FragPos.y < 64.0) {
            float depth = 64.0 - FragPos.y;
            fogFactor = 1.0 - exp(-depth * 0.25);
        }
    } else {
        // Eksponencjalna mgła — gładkie, naturalne zanikanie
        float fogDensity = 0.12;
        fogFactor = 1.0 - exp(-dist * fogDensity);
    }
    
    finalColor = mix(finalColor, fogColor, fogFactor);
    
    FragColor = vec4(finalColor, texColor.a);
}
