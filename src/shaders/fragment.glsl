#version 330 core

in vec2 vUV;
out vec4 FragColor;
uniform sampler2D u_scene;



void main()
{
    vec3 color = texture(u_scene, vUV).rgb;
    FragColor = vec4(color, 1.0);
}

