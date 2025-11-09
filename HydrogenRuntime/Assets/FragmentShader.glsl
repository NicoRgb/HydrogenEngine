#version 450

#define MAX_TEXTURES 128

layout(set = 0, binding = 1) uniform sampler2D texSampler[MAX_TEXTURES];

layout(push_constant) uniform constants
{
	mat4 model;
	int texIndex;
} PushConstants;

layout(location = 0) in vec3 fragColor;
layout(location = 1) in vec2 fragTexCoord;

layout(location = 0) out vec4 outColor;

void main()
{
	outColor = texture(texSampler[PushConstants.texIndex], fragTexCoord);
}
