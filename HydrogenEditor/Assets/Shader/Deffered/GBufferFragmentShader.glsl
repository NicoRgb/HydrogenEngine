#version 450

#define MAX_TEXTURES 128
layout(binding = 1) uniform sampler2D albedoTex[MAX_TEXTURES];

layout(push_constant) uniform constants
{
    mat4 model;
    vec4 color;
    int texIndex;
} PushConstants;

layout(location = 0) in vec3 fragPos;
layout(location = 1) in vec2 fragUV;
layout(location = 2) in vec3 fragNormal;

layout(location = 0) out vec4 outPosition;
layout(location = 1) out vec4 outNormal;
layout(location = 2) out vec4 outAlbedo;
layout(location = 3) out vec4 outEmissive;

void main()
{
    outPosition = vec4(fragPos, 1.0);
    outNormal = vec4(normalize(fragNormal), 1.0);
    outAlbedo = texture(albedoTex[PushConstants.texIndex], fragUV) * vec4(PushConstants.color.rgb, 1.0);
    outEmissive = vec4(PushConstants.color.rgb * PushConstants.color.a, 1.0);
}
