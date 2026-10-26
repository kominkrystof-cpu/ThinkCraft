#version 330 core

layout (location = 0) in vec2 a_position;
layout (location = 1) in vec2 a_uv;

uniform vec2 u_viewport;

out vec2 v_uv;

void main() {
    vec2 ndc = vec2(a_position.x / u_viewport.x * 2.0 - 1.0,
                    1.0 - a_position.y / u_viewport.y * 2.0);
    gl_Position = vec4(ndc, 0.0, 1.0);
    v_uv = a_uv;
}
