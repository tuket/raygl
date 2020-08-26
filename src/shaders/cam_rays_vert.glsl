layout(location = 0) in vec2 a_pos;

uniform vec2 u_fovFactor;
uniform mat4 u_viewMtx;
uniform uint u_sampleInd;
uniform uint u_numSamples;
uniform uvec2 u_resolution;

out vec3 v_rayOri;
out vec3 v_rayDir;

void main()
{
    // randomize the sample inside the pixel
    vec2 jitter = hammersleyVec2(u_sampleInd, u_numSamples);
    jitter = 0 * (jitter - 0.5) / vec2(u_resolution);

    vec3 dir;
    dir.xy = (a_pos + jitter) * u_fovFactor;
    dir.z = -1;

    v_rayDir = mat3(u_viewMtx) * dir;
    v_rayOri = u_viewMtx[3].xyz;
    //v_rayDir = vec3(a_pos, 0);
    gl_Position = vec4(a_pos, 0, 1);
}