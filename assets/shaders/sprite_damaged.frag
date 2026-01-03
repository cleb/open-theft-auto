#version 330 core

in vec2 TexCoords;
out vec4 color;

uniform sampler2D sprite;       // Original texture
uniform sampler2D deltaTexture; // Damage overlay texture
uniform vec3 spriteColor;

// Damage flags for each quadrant (0 = undamaged, 1 = damaged)
uniform int damageFrontLeft;
uniform int damageFrontRight;
uniform int damageRearLeft;
uniform int damageRearRight;

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
    vec4 finalColor = originalColor;
    if (isDamaged && deltaColor.a > 0.0) {
        finalColor = deltaColor;
    }
    
    color = vec4(spriteColor, 1.0) * finalColor;
}
