#version 330 core

in vec2 vUV;
out vec4 FragColor;

uniform sampler2D u_sceneColor;
uniform sampler2D u_bloomBlur;
uniform float u_bloomStrength;
uniform float u_exposure;

void main()
{
    vec3 sceneColor = texture(u_sceneColor, vUV).rgb;
    vec3 bloomColor = texture(u_bloomBlur, vUV).rgb;

    vec3 hdrColor = sceneColor + bloomColor * u_bloomStrength;
    vec3 mapped = vec3(1.0) - exp(-hdrColor * u_exposure);
    vec3 gammaCorrected = pow(mapped, vec3(1.0 / 2.2));

    FragColor = vec4(gammaCorrected, 1.0);
}
