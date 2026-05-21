#version 330 core

in vec2 vUV;
out vec4 FragColor;

uniform sampler2D u_input;
uniform vec2 u_texelSize;
uniform int u_horizontal;

void main()
{
    vec2 axis = (u_horizontal == 1) ? vec2(u_texelSize.x, 0.0) : vec2(0.0, u_texelSize.y);

    float weights[5] = float[](0.227027, 0.1945946, 0.1216216, 0.054054, 0.016216);
    vec3 result = texture(u_input, vUV).rgb * weights[0];

    for (int i = 1; i < 5; ++i) {
        vec2 offset = axis * float(i);
        result += texture(u_input, vUV + offset).rgb * weights[i];
        result += texture(u_input, vUV - offset).rgb * weights[i];
    }

    FragColor = vec4(result, 1.0);
}
