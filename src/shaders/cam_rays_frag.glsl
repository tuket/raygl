layout(location = 0) out vec4 o_rayOri;
layout(location = 1) out vec4 o_rayDir;

in vec3 v_rayOri;
in vec3 v_rayDir;

void main()
{
    o_rayOri.rgb = v_rayOri;
    o_rayDir.rgb = normalize(v_rayDir);
}