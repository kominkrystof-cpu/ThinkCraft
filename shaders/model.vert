#version 330 core

layout (location = 0) in vec3 a_position;
layout (location = 1) in vec2 a_uv;
layout (location = 2) in float a_shade;
layout (location = 3) in float a_tile;

uniform mat4 u_view_projection;
uniform mat4 u_model;

out vec2 v_uv;
out vec3 v_color;
flat out int v_tile;

void main() {
    v_uv = a_uv * (1.0 / 16.0);
    v_color = vec3(a_shade);
    v_tile = int(a_tile + 0.5);
    gl_Position = u_view_projection * u_model * vec4(a_position, 1.0);
}
