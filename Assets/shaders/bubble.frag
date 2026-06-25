#version 330 core
out vec4 FragColor;

in vec2 TexCoords;

uniform float bubbleAlpha;
uniform float bubbleAge;
uniform vec3 viewPos;
uniform vec3 bubblePos;

void main() {
    // Compute distance from center of the quad (circular shape)
    vec2 centered = TexCoords * 2.0 - 1.0;
    float dist = length(centered);
    
    // Discard pixels outside the circle
    if (dist > 1.0) discard;
    
    // Bubble appearance: transparent center, bright rim (Fresnel-like)
    float rim = smoothstep(0.5, 1.0, dist);
    float innerTransparency = smoothstep(0.0, 0.6, dist);
    
    // Base bubble color: slightly blue-green tinted
    vec3 bubbleColor = vec3(0.6, 0.85, 0.75);
    
    // Rim highlight (brighter at the edge)
    vec3 rimColor = vec3(0.8, 0.95, 0.9);
    vec3 color = mix(bubbleColor, rimColor, rim);
    
    // Specular highlight (small bright spot)
    vec2 highlightPos = vec2(-0.3, 0.3);
    float highlight = 1.0 - smoothstep(0.0, 0.25, length(centered - highlightPos));
    color += vec3(1.0, 1.0, 0.95) * highlight * 0.6;
    
    // Alpha: transparent center, opaque rim, overall fading with age
    float alpha = mix(0.08, 0.45, rim) * bubbleAlpha;
    alpha += highlight * 0.3;
    
    // Subtle iridescence based on viewing angle
    float iridescence = sin(bubbleAge * 3.0 + dist * 6.0) * 0.1;
    color += vec3(iridescence, -iridescence * 0.5, iridescence * 0.3);
    
    // Underwater fog
    float fogDist = length(viewPos - bubblePos);
    vec3 fogColor = vec3(0.05, 0.20, 0.15);
    float fogFactor = 1.0 - exp(-fogDist * 0.12);
    color = mix(color, fogColor, fogFactor * 0.5);
    
    FragColor = vec4(color, alpha);
}
