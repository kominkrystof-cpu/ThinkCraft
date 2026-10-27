#version 330 core

in vec2 v_uv;
in vec3 v_color;
flat in int v_tile;

uniform sampler2DArray u_texture;

out vec4 frag_color;

void main() {
    vec4 texel = texture(u_texture, vec3(v_uv, float(v_tile)));

    if (texel.a < 0.05) {
        discard;
    }

    frag_color = vec4(texel.rgb * v_color, texel.a);
}
