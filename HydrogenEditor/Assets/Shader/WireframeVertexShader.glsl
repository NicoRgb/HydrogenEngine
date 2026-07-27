#version 450

layout(location = 0) in vec3 inPosition;

layout(binding = 0, set = 0) uniform CameraBuffer
{
    mat4 view;
    mat4 proj;
    vec3 viewPos;
} ubo;

layout(push_constant) uniform Transform
{
    mat4 model;
    vec3 color;
} pc;

void main()
{
    gl_Position = ubo.proj * ubo.view * pc.model * vec4(inPosition, 1.0);
}
