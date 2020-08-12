layout(location = 0) in vec2 a_pos;

uniform vec2 u_fov;

out vec3 v_rayDir;

void main()
{
    vec3 dir;
    dir.xy = a_pos * tan(u_fov);
    dir.z = -1;
    v_rayDir = normalize(dir);
    //v_rayDir = vec3(a_pos, 0);
    gl_Position = vec4(a_pos, 0, 1);
}