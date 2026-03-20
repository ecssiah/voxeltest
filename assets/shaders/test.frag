#version 410 core

in vec3 v_normal;
in vec2 v_uv;

out vec4 FragColor;

uniform sampler2D u_texture_sampler;

void main()
{
    vec3 texture_color = texture(u_texture_sampler, v_uv).rgb;

    vec3 normal_color = v_normal * 0.5 + 0.5;
    vec3 color = mix(texture_color, normal_color, 0.5);

    FragColor = vec4(color, 1.0);
}
