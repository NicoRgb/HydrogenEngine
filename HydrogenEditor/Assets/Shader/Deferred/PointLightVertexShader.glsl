#version 450

layout(location = 0) in vec3 inPos;
layout(location = 1) in vec2 inUV;
layout(location = 2) in vec3 inNormal;

layout(binding = 5) uniform CameraBuffer
{
    mat4 viewProj;
    vec3 viewPos;
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
    gl_Position = ubo.viewProj * light.model * vec4(inPos, 1.0);
}
