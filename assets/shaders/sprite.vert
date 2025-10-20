#version 330 core

layout (location = 0) in vec4 vertex; // <vec2 position, vec2 texCoords>

out vec2 TexCoords;

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;

void main() {
    TexCoords = vertex.zw;
    // Place sprite in XY plane (z=0) for top-down view
    gl_Position = projection * view * model * vec4(vertex.xy, 0.0, 1.0);
}