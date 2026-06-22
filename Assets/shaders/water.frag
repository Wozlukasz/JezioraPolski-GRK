#version 330 core
out vec4 FragColor;

in vec3 FragPos;
in vec3 Normal;
in vec2 TexCoords;

uniform vec3 lightDir;
uniform vec3 viewPos;
uniform sampler2D texWater;

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
    
    vec3 waterTexColor = sampleNoTile(texWater, uv);
    vec3 waterColor = waterTexColor * vec3(0.5, 0.8, 1.0); // Semi-transparent turquoise water

    // Specular highlight representing the sun's reflection on waves
    vec3 norm = normalize(Normal);
    vec3 viewDir = normalize(viewPos - FragPos);
    vec3 lightVector = normalize(lightDir);
    
    vec3 halfwayDir = normalize(lightVector + viewDir);
    float spec = pow(max(dot(norm, halfwayDir), 0.0), 128.0);
    vec3 specular = vec3(0.9, 0.95, 1.0) * spec * 0.9;

    FragColor = vec4(waterColor + specular, 0.70); // 70% transparency
}
