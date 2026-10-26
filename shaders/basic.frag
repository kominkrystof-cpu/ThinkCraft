#version 330 core

in vec2 v_uv;
in vec4 v_color;
in vec3 v_world_position;

uniform sampler2D u_texture;
uniform vec3 u_camera_pos;
uniform vec3 u_fog_color;
uniform float u_fog_start;
uniform float u_fog_end;

out vec4 frag_color;

void main() {
    vec3 color = texture(u_texture, v_uv).rgb * v_color.rgb;
    float view_distance = length(v_world_position - u_camera_pos);
    float fog = clamp((view_distance - u_fog_start)
                      / (u_fog_end - u_fog_start), 0.0, 1.0);

    frag_color = vec4(mix(color, u_fog_color, fog), 1.0);
}
