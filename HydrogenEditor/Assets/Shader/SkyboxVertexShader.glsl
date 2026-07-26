#version 450
#extension GL_KHR_vulkan_glsl : enable

layout(location = 0) out vec3 outViewDir;

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

    vec2 pos = positions[gl_VertexIndex];
    
    gl_Position = vec4(pos, 1.0, 1.0);

    mat4 viewNoTranslation = mat4(mat3(ubo.view));
    mat4 invViewProj = inverse(ubo.proj * viewNoTranslation);

    vec4 unprojected = invViewProj * vec4(pos, 1.0, 1.0);
    outViewDir = unprojected.xyz / unprojected.w;
}
