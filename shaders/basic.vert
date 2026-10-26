#version 330 core

layout (location = 0) in vec3 a_position;
layout (location = 1) in vec2 a_uv;
layout (location = 2) in vec3 a_color;
layout (location = 3) in float a_tile;

uniform mat4 u_mvp;

out vec2 v_uv;
out vec3 v_color;
out vec3 v_world_position;
flat out int v_tile;

void main() {
    v_uv = a_uv;
    v_color = a_color;
    v_world_position = a_position;
    v_tile = int(a_tile + 0.5);
    gl_Position = u_mvp * vec4(a_position, 1.0);
}
