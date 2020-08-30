layout(location = 0) out vec3 o_color;

in vec3 v_rayOri;
in vec3 v_rayDir;

uniform int u_sampleInd;
uniform int u_numSamples;

struct SphereObj {
    vec4 pos_rad;
    vec4 emitColor_metallic;
    vec4 albedo_rough2;
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

vec3 importanceSampleGgx(vec2 rnd, float rough2, vec3 N)
{
    float phi = 2 * PI * rnd.x;
    float rough4 = rough2 * rough2;
    float cosTheta = sqrt((1 - rnd.y) / (1 + (rough4 - 1) * rnd.y));
    float sinTheta = sqrt(1 - cosTheta * cosTheta);

    vec3 H = vec3 (
        sinTheta * cos(phi),
        cosTheta,
        sinTheta * sin(phi));

    vec3 up = abs(N.y) < 0.99 ? vec3(0,1,0) : vec3(0,0,1);
    vec3 tanX = normalize(cross(up, N));
    vec3 tanZ = cross(tanX, N);

    return tanX * H.x + N * H.y + tanZ * H.z;
}

void main()
{
    // compute amision and albedos of bounces
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
                s_sphereObjs[i].pos_rad.xyz, s_sphereObjs[i].pos_rad.w);
            if(d > near && d < nearestDepth) {
                //o_color = vec3(1,0,0);
                //return;
                nearest = i;
                nearestDepth = d;
            }
        }
        if(nearest == -1)
            break;

        emitColors[bounce] = s_sphereObjs[nearest].emitColor_metallic.rgb;
        albedos[bounce] = s_sphereObjs[nearest].albedo_rough2.rgb;

        rayOri = rayOri + nearestDepth * rayDir;
        vec3 normal = normalize(rayOri - s_sphereObjs[nearest].pos_rad.xyz);
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
    o_color = color;

    //o_color = vec3(emitColors[0]);
}