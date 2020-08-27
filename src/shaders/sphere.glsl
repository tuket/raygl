layout(location = 0) out vec3 o_rayOri;
layout(location = 1) out vec3 o_rayDir;
layout(location = 2) out vec3 o_attenColor;
layout(location = 3) out vec3 o_emitColor;

in vec2 v_tc;

uniform sampler2D u_seed;
uniform sampler2D u_rayOri;
uniform sampler2D u_rayDir;

uniform uint u_sampleInd;
uniform uint u_numSamples;
uniform vec3 u_spherePos;
uniform float u_sphereRad;
uniform vec3 u_emitColor;
uniform vec3 u_albedo;

const float near = 0.01;
const float far = 1000000;

float rayVsSphere(vec3 ori, vec3 dir, vec3 p, float r)
{
    vec3 op = p - ori;
    //if(op == vec3(0,0,0))
    //    return r;
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
    if(rayDir == vec3(0))
        discard;
    float d = rayVsSphere(rayOri, rayDir, u_spherePos, u_sphereRad);
    if(d > near) {
        gl_FragDepth = d / far;
        // the following values are blended if the depth test passes
        o_rayOri = rayOri + d * rayDir;
        vec3 normal = normalize(o_rayOri - u_spherePos);
        vec2 rnd = hammersleyVec2(u_sampleInd, u_numSamples);
        o_rayDir = generateUniformSample(normal, rnd);
        o_attenColor = u_albedo / PI;
        o_emitColor = u_emitColor;
    }
    else {
        discard;
    }
}