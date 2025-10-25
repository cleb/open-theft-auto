#version 330 core

out vec4 FragColor;

in vec3 FragPos;
in vec3 SurfaceNormal;
in vec2 WaterUV;
in float WaveFactor;

uniform vec3 lightPos;
uniform vec3 lightColor;
uniform vec3 cameraPos;
uniform vec3 deepColor;
uniform vec3 shallowColor;
uniform float foamIntensity;
uniform float time;

float rippleNoise(vec2 uv) {
    vec2 wave = vec2(
        sin(uv.x * 6.2831 + time * 0.8),
        cos(uv.y * 6.2831 - time * 0.6)
    );
    return dot(wave, vec2(0.5)) * 0.5 + 0.5;
}

void main() {
    vec3 norm = normalize(SurfaceNormal);
    vec3 lightDir = normalize(lightPos - FragPos);
    vec3 viewDir = normalize(cameraPos - FragPos);

    float diff = max(dot(norm, lightDir), 0.0);
    vec3 reflectDir = reflect(-lightDir, norm);
    float spec = pow(max(dot(viewDir, reflectDir), 0.0), 32.0);

    float fresnel = pow(1.0 - max(dot(norm, viewDir), 0.0), 3.0);

    vec2 flowUV = WaterUV * 0.35 + vec2(time * 0.05, time * -0.04);
    float detail = rippleNoise(flowUV);
    float foamMask = clamp(WaveFactor * 0.5 + 0.5 + (detail - 0.5) * 0.35, 0.0, 1.0);
    float foam = smoothstep(0.65, 0.95, foamMask) * foamIntensity;

    vec3 baseColor = mix(deepColor, shallowColor, clamp(WaveFactor * 0.5 + 0.5, 0.0, 1.0));
    vec3 ambient = deepColor * 0.25;
    vec3 lighting = ambient + lightColor * (diff + 0.4 * spec);

    vec3 color = baseColor * lighting;
    color += fresnel * (vec3(0.35, 0.55, 0.75) + detail * 0.2);
    color = mix(color, vec3(0.95, 0.98, 1.0), foam);
    color = clamp(color, 0.0, 1.0);

    FragColor = vec4(color, 0.82 + foam * 0.1);
}
