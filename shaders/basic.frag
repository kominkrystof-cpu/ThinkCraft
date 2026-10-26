#version 330 core

in vec2 v_uv;
in vec3 v_color;
in vec3 v_world_position;
flat in int v_tile;

uniform sampler2D u_texture;
uniform vec3 u_camera_pos;
uniform vec3 u_fog_color;
uniform float u_fog_start;
uniform float u_fog_end;

out vec4 frag_color;

const float ATLAS_GRID = 4.0;
const float TILE_SIZE = 16.0;

void main() {
    vec2 cell = vec2(mod(float(v_tile), ATLAS_GRID),
                     floor(float(v_tile) / ATLAS_GRID));
    vec2 local_uv = (fract(v_uv) * (TILE_SIZE - 1.0) + 0.5) / TILE_SIZE;
    vec2 atlas_uv = (cell + local_uv) / ATLAS_GRID;
    vec3 color = texture(u_texture, atlas_uv).rgb * v_color;
    float view_distance = length(v_world_position - u_camera_pos);
    float fog = clamp((view_distance - u_fog_start)
                      / (u_fog_end - u_fog_start), 0.0, 1.0);

    frag_color = vec4(mix(color, u_fog_color, fog), 1.0);
}
