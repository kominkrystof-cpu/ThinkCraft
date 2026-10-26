#version 330 core

in vec2 v_uv;
in vec3 v_color;
flat in int v_tile;
in float v_fog;

uniform sampler2DArray u_texture;
uniform vec3 u_fog_color;

out vec4 frag_color;

void main() {
    vec3 color = texture(u_texture, vec3(v_uv, float(v_tile))).rgb * v_color;

    frag_color = vec4(mix(color, u_fog_color, v_fog), 1.0);
}
