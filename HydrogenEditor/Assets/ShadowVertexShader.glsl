#version 450

layout(binding = 0) uniform UniformBufferObject
{
    mat4 lightSpaceMatrix;
} ubo;

layout(push_constant) uniform constants
{
    mat4 model;
} PushConstants;

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inColor;
layout(location = 2) in vec2 inTexCoord;
layout(location = 3) in vec3 inNormal;

void main()
{
    gl_Position = ubo.lightSpaceMatrix * PushConstants.model * vec4(inPosition, 1.0);
}
