#version 330 core

in vec2 vUV;
out vec4 FragColor;

uniform sampler2D u_scene;
uniform samplerCube u_skybox;

void main()
{
    vec4 sceneSample = texture(u_scene, vUV);
    if (sceneSample.a < 0.0) {
        vec3 color = texture(u_skybox, normalize(sceneSample.rgb)).rgb;
        FragColor = vec4(color, 1.0);
    }
    else {
        FragColor = sceneSample;
    }
}

