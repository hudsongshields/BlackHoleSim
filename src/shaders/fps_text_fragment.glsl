#version 330 core
in vec2 v_uv;
out vec4 fragColor;

uniform sampler2D u_text;
uniform vec3 u_textColor;

void main() {
    float alpha = texture(u_text, v_uv).r;
    fragColor = vec4(u_textColor, alpha);
}
