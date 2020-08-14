layout(location = 0) out vec3 o_rayOri;
layout(location = 1) out vec3 o_rayDir;
layout(location = 2) out vec3 o_attenColor;
layout(location = 3) out vec3 o_emitColor;

in vec2 v_tc;

uniform sampler2D u_rayOri;
uniform sampler2D u_rayDir;

uniform vec3 u_spherePos;
uniform float u_sphereRad;
uniform vec3 u_emitColor;
uniform vec3 u_albedo;

float rayVsSphere(vec3 ori, vec3 dir, vec3 p, float r)
{
    vec3 op = p - ori;
    const float D = dot(dir, op);
    const float H2 = dot(op, op) - D*D;
    float K2 = r*r - H2;
    if(K2 < 0)
        return -1;
    float K = sqrt(K2);
    if(D >= K)
        return D - K;
    else
        return D + K;
}

void main()
{
    vec3 rayOri = texture(u_rayOri, v_tc).rgb;
    vec3 rayDir = texture(u_rayDir, v_tc).rgb;
    float d = rayVsSphere(rayOri, rayDir, u_spherePos, u_sphereRad);
    if(d > 0) {
        gl_FragDepth = d;
        // the following values are blended if the depth test passes
        o_rayOri = rayOri + d * rayDir;
        vec3 normal = normalize(o_rayOri - u_spherePos);
        o_rayDir = reflect(rayDir, normal);
        o_attenColor = u_albedo / PI;
        o_emitColor = u_emitColor;
    }
    else {
        discard;
    }
}