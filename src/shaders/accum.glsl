layout(location = 0) out vec3 o_color;

in vec2 v_tc;

uniform sampler2DArray u_atten;
uniform sampler2DArray u_emitColor;

void main()
{
    o_color = vec3(0);
    for(int i = 0; i < k_numBounces; i++) {
        vec3 tc = vec3(v_tc, k_numBounces - i - 1);
        o_color *= texture(u_atten, tc).rgb;
        o_color += texture(u_emitColor, tc).rgb;
    }
}