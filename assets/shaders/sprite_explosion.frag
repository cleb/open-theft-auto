#version 330 core

in vec2 TexCoords;
out vec4 color;

uniform sampler2D sprite;
uniform vec3 spriteColor;
uniform float explosionProgress;

void main() {
    vec4 base = texture(sprite, TexCoords) * vec4(spriteColor, 1.0);

    vec2 center = vec2(0.5, 0.5);
    float dist = distance(TexCoords, center);
    float wave = smoothstep(explosionProgress + 0.08, explosionProgress - 0.02, dist);
    float bloom = (1.0 - dist) * (1.0 - explosionProgress);

    vec3 coreColor = vec3(1.0, 0.6, 0.1);
    vec3 brightColor = vec3(1.0, 1.0, 0.7);
    vec3 explosionColor = mix(coreColor, brightColor, wave);

    float intensity = clamp((bloom + wave) * base.a, 0.0, 1.0);
    vec3 finalColor = mix(base.rgb, explosionColor, intensity);

    float fade = 1.0 - smoothstep(0.6, 1.0, explosionProgress);
    float alpha = clamp(base.a * (0.6 + fade) + wave * 0.4 * base.a, 0.0, 1.0);
    color = vec4(finalColor, alpha);
}
