#version 450

#define MAX_TEXTURES 128
layout(binding = 1) uniform sampler2D albedoMaps[MAX_TEXTURES];

layout(push_constant) uniform constants
{
    mat4 model;
    
    int albedoIndex;
    vec3 tint;
    float roughness;
    float metallic;
    float emissive;
} PushConstants;

layout(location = 0) in vec3 fragPos;
layout(location = 1) in vec2 fragUV;
layout(location = 2) in vec3 fragNormal;

layout(location = 0) out vec4 outPosition;
layout(location = 1) out vec4 outNormal;
layout(location = 2) out vec4 outAlbedoRough;
layout(location = 3) out vec4 outMaterial;

void main()
{
    outPosition = vec4(fragPos, 1.0);
    outNormal = vec4(normalize(fragNormal), 1.0);
    outAlbedoRough.rgb = texture(albedoMaps[PushConstants.albedoIndex], fragUV).rgb * PushConstants.tint;
    outAlbedoRough.a = PushConstants.roughness;
    outMaterial.r = PushConstants.metallic;
    outMaterial.g = PushConstants.emissive;
    outMaterial.ba = vec2(0.0);
}
