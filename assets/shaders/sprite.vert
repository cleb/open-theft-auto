#version 330 core

layout (location = 0) in vec4 vertex; // <vec2 position, vec2 texCoords>

out vec2 TexCoords;

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;
uniform vec4 uvOffsetScale; // xy = offset, zw = scale

void main() {
    // Transform base UV coordinates (0-1) to sprite sheet region
    vec2 baseUV = vertex.zw;
    TexCoords = uvOffsetScale.xy + baseUV * uvOffsetScale.zw;
    
    // Place sprite in XY plane (z=0) for top-down view
    gl_Position = projection * view * model * vec4(vertex.xy, 0.0, 1.0);
}