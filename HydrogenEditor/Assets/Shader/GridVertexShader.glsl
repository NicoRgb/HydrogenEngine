#version 450

#extension GL_KHR_vulkan_glsl : enable

layout(location = 0) out vec3 fragRayDir;

layout(binding = 0, set = 0) uniform CameraBuffer
{
    mat4 view;
    mat4 proj;
    vec3 viewPos;
} ubo;

void main()
{
    vec2 positions[3] = vec2[](
        vec2(-1.0, -1.0),
        vec2( 3.0, -1.0),
        vec2(-1.0,  3.0)
    );

    vec2 uvs[3] = vec2[](
        vec2(0.0, 0.0),
        vec2(2.0, 0.0),
        vec2(0.0, 2.0)
    );

    gl_Position = vec4(positions[gl_VertexIndex], 0.0, 1.0);

    mat4 invViewProj = inverse(ubo.proj * ubo.view);

    vec4 farPlaneTarget = invViewProj * vec4(positions[gl_VertexIndex], 1.0, 1.0);
    vec3 worldPosFar = farPlaneTarget.xyz / farPlaneTarget.w;

    fragRayDir = worldPosFar - ubo.viewPos;
}
