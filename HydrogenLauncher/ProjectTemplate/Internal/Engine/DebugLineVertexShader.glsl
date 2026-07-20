#version 450

layout(binding = 0) uniform UniformBufferObject
{
	mat4 view;
	mat4 proj;
} ubo;

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec4 inColor;

layout(location = 0) out vec3 fragColor;

void main()
{
	fragColor = vec3(inColor.r, inColor.g, inColor.b);
	gl_Position = ubo.proj * ubo.view * vec4(inPosition, 1.0);
}
