#version 330 core

layout (location = 0) in vec3 a_position;
layout (location = 1) in vec2 a_uv;
layout (location = 2) in vec4 a_color;

uniform mat4 u_mvp;

out vec2 v_uv;
out vec4 v_color;
out vec3 v_world_position;

void main() {
    v_uv = a_uv;
    v_color = a_color;
    v_world_position = a_position;
    gl_Position = u_mvp * vec4(a_position, 1.0);
}
