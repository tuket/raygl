layout(location = 0) out vec4 o_color;

in vec3 v_rayDir;

void main()
{
    //o_color = vec4(0, 1, 0, 1);
    //o_color = vec4(0.5*v_rayDir+vec3(0.5), 1.0);
    o_color = vec4(abs(v_rayDir), 1);
    /*if(abs(v_rayDir.x) < 0.99 && abs(v_rayDir.y) < 0.1)
        o_color = vec4(1,1,1,1);
    else
        discard;*/
}