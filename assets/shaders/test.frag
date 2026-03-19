#version 410 core

in vec3 v_normal;
in vec2 v_uv;

out vec4 FragColor;

uniform sampler2D u_texture_sampler;

void main()
{
     FragColor = texture(u_texture_sampler, v_uv);
}