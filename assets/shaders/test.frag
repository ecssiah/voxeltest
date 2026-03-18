#version 410 core

in vec3 v_color;
out vec4 FragColor;

uniform sampler2D texture_sampler;

void main() {
     FragColor = texture(texture_sampler, u_uv);
}