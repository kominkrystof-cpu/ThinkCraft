#version 330 core

layout (location = 0) in vec3 a_position;
layout (location = 1) in vec2 a_uv;
layout (location = 2) in float a_shade;
layout (location = 3) in float a_tile;

uniform mat4 u_mvp;
uniform vec3 u_camera_pos;
uniform vec3 u_chunk_origin;
uniform float u_fog_start;
uniform float u_fog_end;

out vec2 v_uv;
out vec3 v_color;
flat out int v_tile;
out float v_fog;

void main() {
    v_uv = a_uv;
    v_color = vec3(a_shade);
    v_tile = int(a_tile + 0.5);
    vec3 world_position = a_position + u_chunk_origin;
    float view_distance = length(world_position - u_camera_pos);
    v_fog = clamp((view_distance - u_fog_start)
                  / (u_fog_end - u_fog_start), 0.0, 1.0);
    gl_Position = u_mvp * vec4(world_position, 1.0);
}
