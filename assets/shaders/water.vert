#version 330 core

layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aNormal;
layout (location = 2) in vec2 aTexCoord;

out vec3 FragPos;
out vec3 SurfaceNormal;
out vec2 WaterUV;
out float WaveFactor;

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;
uniform float time;
uniform float waveStrength;
uniform float waveFrequency;
uniform float waveSpeed;

void main() {
    float phaseX = aPos.x * waveFrequency + time * waveSpeed;
    float phaseY = aPos.y * waveFrequency - time * waveSpeed * 0.6;
    float diagPhase = (aPos.x - aPos.y) * waveFrequency * 0.5 + time * waveSpeed * 0.8;

    float primaryWave = sin(phaseX) * cos(phaseY);
    float secondaryWave = 0.5 * sin(diagPhase);
    float displacement = (primaryWave + secondaryWave) * waveStrength;

    vec3 displacedPosition = vec3(aPos.xy, aPos.z + displacement);

    float dPrimaryDx = waveFrequency * cos(phaseX) * cos(phaseY);
    float dPrimaryDy = -waveFrequency * sin(phaseX) * sin(phaseY);
    float dSecondaryDx = 0.5 * cos(diagPhase) * waveFrequency * 0.5;
    float dSecondaryDy = -0.5 * cos(diagPhase) * waveFrequency * 0.5;

    float slopeX = waveStrength * (dPrimaryDx + dSecondaryDx);
    float slopeY = waveStrength * (dPrimaryDy + dSecondaryDy);

    vec3 normal = normalize(vec3(-slopeX, -slopeY, 1.0));
    mat3 normalMatrix = mat3(transpose(inverse(model)));

    vec4 worldPosition = model * vec4(displacedPosition, 1.0);
    FragPos = worldPosition.xyz;
    SurfaceNormal = normalize(normalMatrix * normal);
    WaterUV = aTexCoord;
    WaveFactor = displacement / max(waveStrength, 0.0001);

    gl_Position = projection * view * worldPosition;
}
