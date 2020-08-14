layout(location = 0) out vec4 o_color;

layout(location = 0) uniform sampler2D u_tex;

in vec2 v_tc;

void main()
{
    o_color = texture(u_tex, v_tc);
}