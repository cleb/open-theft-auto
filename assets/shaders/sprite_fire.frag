#version 330 core

in vec2 TexCoords;
out vec4 color;

uniform sampler2D sprite;
uniform vec3 spriteColor;
uniform float fireIntensity;
uniform float timeSeconds;

void main() {
    vec4 base = texture(sprite, TexCoords) * vec4(spriteColor, 1.0);

    float frontMask = step(0.5, TexCoords.y);

    float flicker = sin(TexCoords.y * 12.0 + timeSeconds * 7.0) * 0.5 + 0.5;
    float noise = sin((TexCoords.x * 1.4 + TexCoords.y) * 16.0 + timeSeconds * 6.5) * 0.5 + 0.5;
    float heat = clamp(flicker * 0.7 + noise * 0.3, 0.0, 1.0);

    vec3 fireHot = vec3(1.0, 0.98, 0.8);
    vec3 fireMid = vec3(1.0, 0.55, 0.12);
    vec3 fireCool = vec3(0.85, 0.08, 0.02);
    vec3 fireColor = mix(fireCool, fireMid, heat);
    fireColor = mix(fireColor, fireHot, smoothstep(0.6, 1.0, heat));

    float flameMask = smoothstep(0.05, 1.0, (TexCoords.y - 0.45) + heat * 0.35);
    float baseIntensity = clamp(fireIntensity * (0.8 + heat * 0.8), 0.0, 1.0);
    float edgeGlow = smoothstep(0.15, 0.95, flameMask) * (0.35 + heat * 0.45);
    float intensity = clamp((baseIntensity * flameMask + edgeGlow) * frontMask, 0.0, 1.0);

    vec3 blended = mix(base.rgb, fireColor, intensity * 0.95);
    blended += fireColor * intensity * 0.3;
    color = vec4(clamp(blended, 0.0, 1.0), base.a);
}
