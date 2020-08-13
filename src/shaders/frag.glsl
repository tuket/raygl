layout(location = 0) out vec4 o_color;

in vec3 v_rayOri;
in vec3 v_rayDir;

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

bool sphere_0(
    out vec3 outOri, out vec3 outDir,
    out vec3 colorEmit, out vec3 colorAtten,
    in ori, in dir)
{
    vec3 spherePos = vec3(0,0,0);
    float sphereRad = 1;
    float d = rayVsSphere(ori, dir, spherePos, sphereRad);
    if(d < 0) {
        return false;
    }

    outOri = d * dir;
    vec3 normal = normalize(outOri);
    outDir = reflect(dir, normal);
    outColor = vec3(1, 0, 0);
    return true;
}

const k_numBounces = 16;

void main()
{
    vec3 colorEmit[k_numBounces]
    vec3 colorAtten[k_numBounces];

    vec3 ori = v_rayOri;
    vec3 dir = normalize(v_rayDir);
    for(int i = 0; i < k_numBounces; i++)
    {
        vec3 newOri, newDir;
        if(
            sphere_0(
            newOri, newDir, colorEmit[i], colorAtten[i],
            ori, dir)
        )
        {
            
        }
        else {
            for(i < k_numBounces; i++)
                colorEmit[i] = colorAtten[i] = vec3(0);
        }
    }
    //o_color = vec4(abs(v_rayDir), 1);

    if(rayVsSphere(v_rayOri, dir, vec3(0,0,0), 2) >= 0)
        o_color = vec4(1,1,1,1);
    else o_color = vec4(0,0,0,1);
}