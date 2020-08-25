const float PI = 3.14159265359;

float rand(vec2 co) {
    return fract(sin(dot(co, vec2(12.9898,78.233))) * 43758.5453);
}

float radicalInverse_VdC(uint bits) 
{
    bits = (bits << 16u) | (bits >> 16u);
    bits = ((bits & 0x55555555u) << 1u) | ((bits & 0xAAAAAAAAu) >> 1u);
    bits = ((bits & 0x33333333u) << 2u) | ((bits & 0xCCCCCCCCu) >> 2u);
    bits = ((bits & 0x0F0F0F0Fu) << 4u) | ((bits & 0xF0F0F0F0u) >> 4u);
    bits = ((bits & 0x00FF00FFu) << 8u) | ((bits & 0xFF00FF00u) >> 8u);
    return float(bits) * 2.3283064365386963e-10; // / 0x100000000
}

vec2 hammersleyVec2(uint sampleInd, uint numSamples)
{
    return vec2(
        radicalInverse_VdC(sampleInd),
        float(sampleInd) / numSamples);
}

vec3 generateUniformSample(vec3 N, vec2 rnd)
{
    vec3 up = abs(N.y) < 0.99 ? vec3(0, 1, 0) : vec3(1, 0, 0);
    vec3 tanX = normalize(cross(up, N));
    vec3 tanZ = cross(tanX, N);
    
    float phi = 2 * PI * rnd.x;
    float h = rnd.y;
    vec3 v = sin(phi) * tanX + cos(phi) * tanZ;
    v += h * N;
    v = normalize(v);
    return v;
}