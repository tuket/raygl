#line 2

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
//const float near = -3;
const float far = 1000000;

float rayVsSphere(vec3 ori, vec3 dir, vec3 p, float r)
{
    vec3 op = p - ori;
    if(op == vec3(0,0,0))
        return r;
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

vec3 importanceSampleGgx_H(vec2 rnd, float rough2, vec3 N)
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

float fresnelSchlick(float cosTheta, float F0)
{
    return F0 + (1.0 - F0) * pow(1.0 - cosTheta, 5.0);
}

float fresnelExact(float c, // cosThetaM
    float n1, float n2) // refraction index
{
    float n12 = n1 / n2;
    float g = sqrt(n12*n12 -1 + c*c);
    float num1 = g-c;
    float den1 = g+c;
    float num2 = c * (g+c) - 1;
    float den2 = c * (g-c) + 1;
    return 0.5 * num1*num1 / (den1*den1) *
        (1 + num2*num2 / (den2*den2));
}

float geometryGgx(float VdotH, float VdotN, float rough2)
{
    float rough4 = rough2 * rough2;
    float cosV2 = VdotN * VdotN;
    float tanV2 = (1 - cosV2) / cosV2;
    return 2 / (1 + sqrt(1 + rough4 * tanV2));
}

void main()
{
    /*vec3 rayOri = v_rayOri;
    vec3 rayDir = normalize(v_rayDir);
    int nearest = -1;
    float nearestDepth = far;
    for(int i = 0; i < s_sphereObjs.length(); i++)
    {
        float d = rayVsSphere(rayOri, rayDir,
            s_sphereObjs[i].pos_rad.xyz, s_sphereObjs[i].pos_rad.w);
        if(d > near && d < nearestDepth) {
            nearest = i;
            nearestDepth = d;
        }
    }
    if(nearest == -1) {
        discard;
    }
    o_color = s_sphereObjs[nearest].emitColor_metallic.rgb;
    return;*/




    // compute emision and attenuations of bounces
    float emitColors[k_numBounces];
    float attenuations[k_numBounces];

    vec3 initRayDir = normalize(v_rayDir);
    for(int c = 0; c < 3; c++)
    {
        vec3 rayOri = v_rayOri;
        vec3 rayDir = initRayDir;
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
                    nearest = i;
                    nearestDepth = d;
                }
            }
            if(nearest == -1) {
                break;
            }
            emitColors[bounce] = s_sphereObjs[nearest].emitColor_metallic[c];
            //emitColors[bounce] = nearestDepth;

            vec3 intersecPoint = rayOri + nearestDepth * rayDir;
            vec3 spherePos = s_sphereObjs[nearest].pos_rad.xyz;
            vec3 V = -rayDir;
            vec3 N = normalize(intersecPoint - spherePos);

            vec2 rnd2 = hammersleyVec2(
                3 * (u_sampleInd * k_numBounces + bounce) + c,
                u_numSamples * k_numBounces * 3);
            float rnd = rand(rnd2);

            float metallic = s_sphereObjs[nearest].emitColor_metallic.a;
            float albedo = s_sphereObjs[nearest].albedo_rough2[c];
            float rough2 = s_sphereObjs[nearest].albedo_rough2.w;
            rayOri = rayOri + nearestDepth * rayDir;
            vec3 H = importanceSampleGgx_H(rnd2, rough2, N);
            float cosThetaH = -dot(H, rayDir);
            float F0 = mix(0.04, albedo, metallic);
            float F = fresnelSchlick(cosThetaH, F0);


            if(rnd < F) // ray is reflected
            {
                vec3 L = reflect(rayDir, H);
                float NoL = dot(N, L);
                float NoV = dot(N, V);
                float NoH = dot(N, H);
                float G1 = geometryGgx(dot(V, H), NoL, rough2);
                float G2 = geometryGgx(dot(L, H), NoV, rough2);
                attenuations[bounce] =
                    (F * G1 * G2) /
                    (4 * NoL * NoV * NoH);
                rayDir = L;
            }
            else if(metallic >= 0.0) // opaque object, ray is diffused
            {
                attenuations[bounce] = s_sphereObjs[nearest].albedo_rough2[c];
                attenuations[bounce] /= PI;
                rayDir = generateUniformSample(N, rnd2);
            }
            else { // transparent object, ray is refracted
                attenuations[bounce] = 0;
            }
        }
        for(; bounce < k_numBounces; bounce++) {
            attenuations[bounce] = 0;
            emitColors[bounce] = 0;
        }

        // accumutate bounces color
        float color = 0;
        for(bounce--; bounce >= 0; bounce--)
        {
            color *= attenuations[bounce];
            color += emitColors[bounce];
        }
        //o_color[c] = attenuations[0];
        o_color[c] = color;
        //o_color = emitColors[0];
    }
}