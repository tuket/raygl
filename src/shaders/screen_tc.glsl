in vec2 a_pos;

out vec2 v_tc;

void main()
{
    gl_Position = vec4(a_pos, 0, 1);
    v_tc = vec2(0.5) + 0.5 * a_pos; 
}