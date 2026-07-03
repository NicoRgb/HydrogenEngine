#version 450

layout(location = 0) in vec3 inPos;
layout(location = 1) in vec2 inUV;
layout(location = 2) in vec3 inNormal;
layout(location = 3) in vec3 inTangent;

layout(binding = 0, set = 0) uniform UniformBuffer
{
    mat4 view;
    mat4 proj;
    vec3 viewPos;
    float pad;
} cameraInfo;

layout(push_constant) uniform constants
{
    mat4 model;
    vec4 color;
    int texIndex;
} PushConstants;

void main()
{
    vec4 worldPos = PushConstants.model * vec4(inPos, 1.0);
    gl_Position = cameraInfo.proj * cameraInfo.view * worldPos;
}
