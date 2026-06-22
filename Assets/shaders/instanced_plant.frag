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

float ShadowCalculation(vec4 fragPosLightSpace, vec3 normal, vec3 lightDir) {
    vec3 projCoords = fragPosLightSpace.xyz / fragPosLightSpace.w;
    projCoords = projCoords * 0.5 + 0.5;
    if(projCoords.z > 1.0) return 0.0;
    float currentDepth = projCoords.z;
    float bias = max(0.005 * (1.0 - dot(normal, lightDir)), 0.001);
    float shadow = currentDepth - bias > texture(shadowMap, projCoords.xy).r ? 1.0 : 0.0;
    return shadow;
}

void main() {
    vec4 texColor = texture(texture_diffuse1, TexCoords);
    if(texColor.a < 0.1) discard; // Alpha cutoff dla liści
    
    vec3 norm = normalize(Normal);
    vec3 lightVector = normalize(lightDir);
    float diff = max(dot(norm, lightVector), 0.0);
    vec3 diffuse = diff * vec3(1.0);
    
    // Zgodnie z prośbą, wyłączamy cienie rzucane na rośliny. 
    // Znacznie poprawia to wydajność i sprawia, że modele wyglądają bardziej "zielono".
    // (Rośliny nadal renderują się w przepustowości cieni, więc teren będzie miał cienie roślin)
    float shadow = 0.0;       
    vec3 ambient = vec3(0.2) * texColor.rgb;
    
    vec3 finalColor = ambient + (1.0 - shadow) * diffuse * texColor.rgb;
    
    // Mgła podwodna — ograniczona widoczność jak w polskim jeziorze eutroficznym
    float dist = length(viewPos - FragPos);
    vec3 fogColor = vec3(0.05, 0.12, 0.08); // Mętna zieleń dna jeziora
    float fogFactor = 0.0;
    
    if (viewPos.y > 64.0) {
        // Kamera nad wodą
        if (FragPos.y < 64.0) {
            // Rośliny pod wodą zanikają wraz z głębokością - szybko (10m)
            float depth = 64.0 - FragPos.y;
            fogFactor = clamp(depth / 5.0, 0.0, 1.0);
        }
    } else {
        // Kamera pod wodą - widoczność ograniczona do 10m
        fogFactor = clamp((dist - 2.0) / 8.0, 0.0, 1.0);
    }
    
    finalColor = mix(finalColor, fogColor, fogFactor);
    
    FragColor = vec4(finalColor, texColor.a);
}
