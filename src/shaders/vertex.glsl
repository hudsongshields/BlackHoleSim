#version 330 core

layout(location = 0) in vec2 aPos;  // we give it this from C++
out vec2 vUV;                       // goes to fragment shader

void main()
{
    gl_Position = vec4(aPos, 0.0, 1.0);  // put triangle on screen
    vUV = aPos * 0.5 + 0.5;              // convert [-1,1] -> [0,1] UV
}
