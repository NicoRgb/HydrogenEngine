#version 450

layout(binding = 0, set = 0) uniform CameraBuffer
{
    mat4 prevViewProj;
    mat4 view;
    mat4 proj;
    vec3 viewPos;
} ubo;

layout(location = 0) out vec3 fragTexCoord;

void main()
{
    const vec2 positions[3] = vec2[](
        vec2(-1.0, -1.0),
        vec2( 3.0, -1.0),
        vec2(-1.0,  3.0)
    );

    vec2 ndc = positions[gl_VertexIndex];

    gl_Position = vec4(ndc, 1.0, 1.0);

    vec4 viewRay = inverse(ubo.proj) * vec4(ndc, 1.0, 1.0);
    vec3 viewDir = normalize(viewRay.xyz / viewRay.w);
    mat3 invViewRotation = transpose(mat3(ubo.view));

    fragTexCoord = normalize(invViewRotation * viewDir);
}
