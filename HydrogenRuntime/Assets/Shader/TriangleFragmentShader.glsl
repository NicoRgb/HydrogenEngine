#version 450

layout(location = 0) in vec3 fragColor;

layout(location = 0) out vec4 outColor;

layout(binding = 0, set = 0) uniform UniformBuffer
{
    vec4 color;
} ubo;

layout(binding = 0, set = 1) uniform PassUniformBuffer
{
    vec4 color;
} pass;

void main()
{
    outColor = vec4(pass.color.r, ubo.color.gba);
}
