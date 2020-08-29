layout(location = 0) out vec3 o_color;

in vec3 v_rayOri;
in vec3 v_rayDir;

uniform int u_sampleInd;
uniform int u_numSamples;

struct SphereObj {
    vec3 pos;
    float rad;
    vec3 emitColor;
    float padding_0;
    vec3 albedo;
    float padding_1;
};
layout(std430, binding = 0) buffer block_sphereObjs {
    SphereObj s_sphereObjs[];
};

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
    // compute amision and albedos of bounces
    if(u_sampleInd == 1000)
    {
    vec3 emitColors[k_numBounces];
    vec3 albedos[k_numBounces];

    vec3 rayOri = v_rayOri;
    vec3 rayDir = normalize(v_rayDir);
    int bounce;
    for(bounce = 0; bounce < k_numBounces; bounce++)
    {
        int nearest = -1;
        float nearestDepth = far;
        for(int i = 0; i < s_sphereObjs.length(); i++)
        {
            float d = rayVsSphere(rayOri, rayDir,
                s_sphereObjs[i].pos, s_sphereObjs[i].rad);
            if(d > near && d < nearest) {
                nearest = i;
                nearestDepth = d;
            }
        }
        if(nearest == -1)
            break;

        emitColors[bounce] = s_sphereObjs[nearest].emitColor;
        albedos[bounce] = s_sphereObjs[nearest].albedo;

        rayOri = rayOri + nearestDepth * rayDir;
        vec3 normal = normalize(rayOri - s_sphereObjs[nearest].pos);
        vec2 rnd = hammersleyVec2(u_sampleInd, u_numSamples);
        rayDir = generateUniformSample(normal, rnd);
    }

    // accumutate bounces color
    vec3 color = vec3(0,0,0);
    for(bounce--; bounce >= 0; bounce--)
    {
        color *= albedos[bounce] / PI;
        color += emitColors[bounce];
    }
    }

    o_color = vec3(s_sphereObjs[1].rad*0.5);
}