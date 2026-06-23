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
    
    // Oświetlenie dyfuzyjne dwustronne — liście przepuszczają światło
    float diff = max(dot(norm, lightVector), 0.0);
    float backDiff = max(dot(-norm, lightVector), 0.0);
    
    // Subsurface Scattering (SSS) — liście podświetlone od tyłu świecą zielonkawo
    float sss = pow(max(dot(viewDir, -lightVector), 0.0), 3.0) * 0.30;
    vec3 sssColor = texColor.rgb * vec3(0.7, 1.0, 0.5);
    
    // Łączenie oświetlenia — jaśniejsze, cieplejsze
    vec3 diffuse = (diff + backDiff * 0.4) * vec3(1.0);
    
    // Cienie wyłączone na roślinach (wydajność)
    float shadow = 0.0;
    
    // Wyższy ambient — rośliny pod wodą dobrze oświetlone jak na zdjęciu referencyjnym
    vec3 ambient = vec3(0.35) * texColor.rgb;
    
    vec3 finalColor = ambient + (1.0 - shadow) * diffuse * texColor.rgb + sss * sssColor;
    
    // === Korekta barw pod wodą ===
    // Cieplejsza, zielonkawa kolorystyka jak na zdjęciu referencyjnym
    if (FragPos.y < 64.0) {
        float waterDepth = clamp((64.0 - FragPos.y) / 15.0, 0.0, 1.0);
        // Lekko podkręcamy zieleń, tłumimy czerwień
        finalColor.r *= mix(1.0, 0.6, waterDepth);
        finalColor.g *= mix(1.0, 1.1, waterDepth); // Zieleń się nawet lekko wzmacnia
        finalColor.b *= mix(1.0, 0.5, waterDepth);  // Niebieski też zanika (ciepły ton)
    }
    
    // Mgła podwodna — jasna, ciepła zieleń jak na zdjęciu referencyjnym
    float dist = length(viewPos - FragPos);
    vec3 fogColor = vec3(0.15, 0.35, 0.12); // Ciepła, jasna zieleń — kluczowa zmiana!
    float fogFactor = 0.0;
    
    if (viewPos.y > 64.0) {
        if (FragPos.y < 64.0) {
            float depth = 64.0 - FragPos.y;
            fogFactor = clamp(depth / 5.0, 0.0, 1.0);
        }
    } else {
        // Widoczność ~10m, ale mgła jest jasna, nie ciemna
        fogFactor = clamp((dist - 2.0) / 8.0, 0.0, 1.0);
    }
    
    finalColor = mix(finalColor, fogColor, fogFactor);
    
    FragColor = vec4(finalColor, texColor.a);
}
