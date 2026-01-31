#version 330 core

in vec2 TexCoords;
out vec4 color;

uniform sampler2D sprite;       // Original texture
uniform sampler2D deltaTexture; // Damage overlay texture
uniform vec3 spriteColor;
uniform float fireIntensity;
uniform float timeSeconds;

// Damage flags for each quadrant (0 = undamaged, 1 = damaged)
uniform int damageFrontLeft;
uniform int damageFrontRight;
uniform int damageRearLeft;
uniform int damageRearRight;

// Simple pseudo-random noise
float hash(vec2 p) {
    return fract(sin(dot(p, vec2(127.1, 311.7))) * 43758.5453);
}

// Smooth value noise
float noise(vec2 p) {
    vec2 i = floor(p);
    vec2 f = fract(p);
    f = f * f * (3.0 - 2.0 * f);
    
    float a = hash(i);
    float b = hash(i + vec2(1.0, 0.0));
    float c = hash(i + vec2(0.0, 1.0));
    float d = hash(i + vec2(1.0, 1.0));
    
    return mix(mix(a, b, f.x), mix(c, d, f.x), f.y);
}

// Fractal brownian motion for flame turbulence
float fbm(vec2 p) {
    float value = 0.0;
    float amplitude = 0.5;
    for (int i = 0; i < 4; i++) {
        value += amplitude * noise(p);
        p *= 2.0;
        amplitude *= 0.5;
    }
    return value;
}

void main() {
    vec4 originalColor = texture(sprite, TexCoords);
    vec4 deltaColor = texture(deltaTexture, TexCoords);
    
    // Determine which quadrant this pixel is in
    // UV coordinates: (0,0) is bottom-left, (1,1) is top-right
    // Front = top half (v >= 0.5), Rear = bottom half (v < 0.5)
    // Left = left half (u < 0.5), Right = right half (u >= 0.5)
    bool isFront = TexCoords.y >= 0.5;
    bool isLeft = TexCoords.x < 0.5;
    
    // Check if this quadrant is damaged
    bool isDamaged = false;
    if (isFront && isLeft) {
        isDamaged = damageFrontLeft != 0;
    } else if (isFront && !isLeft) {
        isDamaged = damageFrontRight != 0;
    } else if (!isFront && isLeft) {
        isDamaged = damageRearLeft != 0;
    } else {
        isDamaged = damageRearRight != 0;
    }
    
    // Blend delta texture over original if damaged and delta has alpha
    vec4 baseColor = originalColor;
    if (isDamaged && deltaColor.a > 0.0) {
        baseColor = deltaColor;
    }
    
    // Apply sprite color tint
    vec4 base = baseColor * vec4(spriteColor, 1.0);

    // Only apply fire to front half of vehicle (top of texture = front)
    float frontMask = smoothstep(0.4, 0.6, TexCoords.y);
    
    // Skip fire entirely on rear half
    if (frontMask < 0.01) {
        color = base;
        return;
    }

    // Flame coordinates - rise upward over time
    vec2 flameUV = TexCoords;
    flameUV.y -= timeSeconds * 0.8;  // Flames rise
    
    // Turbulent flame shape using fractal noise
    float turbulence = fbm(flameUV * 6.0 + vec2(timeSeconds * 1.5, 0.0));
    float flame = fbm(flameUV * 4.0 + vec2(0.0, timeSeconds * 2.0));
    
    // Combine noises for irregular flame edges
    float flameShape = turbulence * 0.6 + flame * 0.4;
    
    // Fade flames toward edges (center burns hotter)
    float centerFade = 1.0 - abs(TexCoords.x - 0.5) * 1.6;
    centerFade = clamp(centerFade, 0.0, 1.0);
    
    // Vertical gradient - stronger at front
    float vertGrad = smoothstep(0.45, 0.9, TexCoords.y);
    
    // Final flame mask
    float finalFlame = flameShape * centerFade * vertGrad * fireIntensity;
    finalFlame = clamp(finalFlame * 1.8, 0.0, 1.0);
    
    // Fire color gradient: dark red -> orange -> yellow -> white core
    vec3 fireColor;
    if (finalFlame < 0.3) {
        fireColor = mix(vec3(0.1, 0.0, 0.0), vec3(0.8, 0.1, 0.0), finalFlame / 0.3);
    } else if (finalFlame < 0.6) {
        fireColor = mix(vec3(0.8, 0.1, 0.0), vec3(1.0, 0.5, 0.0), (finalFlame - 0.3) / 0.3);
    } else if (finalFlame < 0.85) {
        fireColor = mix(vec3(1.0, 0.5, 0.0), vec3(1.0, 0.9, 0.3), (finalFlame - 0.6) / 0.25);
    } else {
        fireColor = mix(vec3(1.0, 0.9, 0.3), vec3(1.0, 1.0, 0.9), (finalFlame - 0.85) / 0.15);
    }
    
    // Apply fire with additive blending for glow
    float blendAmount = finalFlame * frontMask;
    vec3 result = base.rgb + fireColor * blendAmount * 1.2;
    result = mix(base.rgb, result, blendAmount);
    
    // Add bloom/glow around fire area
    float glow = finalFlame * 0.4 * frontMask;
    result += vec3(1.0, 0.4, 0.1) * glow;
    
    color = vec4(clamp(result, 0.0, 1.0), base.a);
}
