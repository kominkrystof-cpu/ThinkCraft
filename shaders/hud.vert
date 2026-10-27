#version 330 core

layout (location = 0) in vec2 a_position;
layout (location = 1) in vec4 a_color;

uniform vec2 u_viewport;

out vec4 v_color;

void main() {
    vec2 ndc = vec2(a_position.x / u_viewport.x * 2.0 - 1.0,
                    a_position.y / u_viewport.y * 2.0 - 1.0);
    gl_Position = vec4(ndc, 0.0, 1.0);
    v_color = a_color;
}
