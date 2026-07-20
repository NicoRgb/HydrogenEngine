#version 450

layout(location = 0) in vec3 inPos;
layout(location = 1) in vec2 inUV;
layout(location = 2) in vec3 inNormal;

layout(binding = 0, set = 0) uniform UniformBufferObject
{
    mat4 view;
    mat4 proj;
    vec3 viewPos;
    float pad;
} ubo;

layout(push_constant) uniform LightData
{
    mat4 model;
    vec4 color; // a = intensity
    vec3 position;
    float radius;
} light;

void main()
{
    gl_Position = ubo.proj * ubo.view * light.model * vec4(inPos, 1.0);
}
