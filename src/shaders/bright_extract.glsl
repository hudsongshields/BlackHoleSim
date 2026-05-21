#version 330 core

in vec2 vUV;
out vec4 FragColor;

uniform sampler2D u_input;
uniform float u_threshold;

void main()
{
    vec3 color = texture(u_input, vUV).rgb;
    float luminance = dot(color, vec3(0.2126, 0.7152, 0.0722));
    FragColor = luminance > u_threshold ? vec4(color, 1.0) : vec4(0.0);
}
