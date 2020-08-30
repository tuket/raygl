layout(location = 0) out vec4 o_color;

layout(location = 0) uniform sampler2D u_tex;

in vec2 v_tc;

void main()
{
    vec3 color = texture(u_tex, v_tc).rgb;
    // reinhard tonemapping
    color = color / (color + 1);
    // gamma correction
    color = pow(color, vec3(1.0/2.2));

    o_color = vec4(color, 1);
}