#version 330 core

in vec2 v_uv;

uniform sampler2D u_font;
uniform vec3 u_color;

out vec4 frag_color;

void main() {
    float alpha = texture(u_font, v_uv).r;
    if (alpha < 0.5) {
        discard;
    }
    frag_color = vec4(u_color, 1.0);
}
