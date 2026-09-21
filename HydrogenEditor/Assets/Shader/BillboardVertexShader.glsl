#version 450
#extension GL_KHR_vulkan_glsl : enable

struct BillboardInstance
{
    vec3 worldPosition;
    int textureIndex;
    vec2 scale;
    vec2 padding;
};

layout(set = 0, binding = 0) uniform CameraBuffer
{
    mat4 view;
    mat4 proj;
    vec3 viewPos;
} ubo;

layout(std430, set = 1, binding = 0) readonly buffer InstanceData
{
    BillboardInstance instances[];
};

layout(location = 0) out vec2 fragUV;
layout(location = 1) flat out int fragTextureIndex;

void main()
{
    BillboardInstance instance = instances[gl_InstanceIndex];

    vec2 offsets[6] = vec2[](
        vec2(-0.5, -0.5), vec2(-0.5,  0.5), vec2(0.5, -0.5),
        vec2( 0.5, -0.5), vec2(-0.5,  0.5), vec2(0.5,  0.5)
    );

    vec2 uvs[6] = vec2[](
        vec2(0.0, 1.0), vec2(0.0, 0.0), vec2(1.0, 1.0),
        vec2(1.0, 1.0), vec2(0.0, 0.0), vec2(1.0, 0.0)
    );

    uint vertexIdx = gl_VertexIndex % 6;
    vec2 posOffset = offsets[vertexIdx];
    fragUV = uvs[vertexIdx];
    fragTextureIndex = instance.textureIndex;

    vec4 viewSpacePos = ubo.view * vec4(instance.worldPosition, 1.0);
    viewSpacePos.xy += posOffset * instance.scale; 

    gl_Position = ubo.proj * viewSpacePos;
}
