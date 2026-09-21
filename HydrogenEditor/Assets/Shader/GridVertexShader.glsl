#version 450

#extension GL_KHR_vulkan_glsl : enable

layout(binding = 0, set = 0) uniform UniformBufferObject
{
    mat4 prevViewProj;
    mat4 view;
    mat4 proj;
    vec3 viewPos;
    float pad;
} ubo;

layout(location = 0) out vec3 fragRayDir;

void main()
{
    vec2 positions[3] = vec2[](
        vec2(-1.0, -1.0),
        vec2( 3.0, -1.0),
        vec2(-1.0,  3.0)
    );

    mat4 viewProj = ubo.proj * ubo.view;
    mat4 invViewProj = inverse(viewProj);

    vec4 farPlaneTarget = invViewProj * vec4(positions[gl_VertexIndex], 1.0, 1.0);
    vec3 worldPosFar = farPlaneTarget.xyz / farPlaneTarget.w;

    fragRayDir = worldPosFar - ubo.viewPos;

    gl_Position = vec4(positions[gl_VertexIndex], 0.0, 1.0);
}
