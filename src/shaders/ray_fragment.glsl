#version 330 core

out vec4 FragColor;
uniform vec3 u_rayColor;

void main()
{
    FragColor = vec4(u_rayColor, 1.0);
}
