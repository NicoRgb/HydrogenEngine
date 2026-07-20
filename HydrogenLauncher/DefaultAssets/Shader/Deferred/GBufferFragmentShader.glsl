#version 450
#extension GL_KHR_vulkan_glsl : enable
#extension GL_EXT_nonuniform_qualifier : enable

layout(binding = 0, set = 1) uniform sampler2D materialTextures[];

layout(push_constant) uniform constants
{
    mat4 model;
    
    int albedoIndex;
    int normalIndex;
    int ormIndex;
    int emissiveIndex;
    
    vec4 tint;
    
    float roughness;
    float metallic;
    float padding0;
    float padding1;
    
    vec4 emissive;
} PushConstants;

layout(location = 0) in vec3 fragPos;
layout(location = 1) in vec2 fragUV;
layout(location = 2) in vec3 fragNormal;
layout(location = 3) in vec3 fragTangent;

layout(location = 0) out vec4 outPosition;
layout(location = 1) out vec4 outNormal;
layout(location = 2) out vec4 outAlbedoRough;
layout(location = 3) out vec4 outMaterial; // r = metallic, g = ao
layout(location = 4) out vec4 outEmissive;

void main()
{
    outPosition = vec4(fragPos, 1.0);
    outMaterial = vec4(0.0, 1.0, 0.0, 0.0);

    if (PushConstants.normalIndex == -1)
    {
        outNormal = vec4(normalize(fragNormal), 1.0);
    }
    else
    {
        vec3 localNormal = texture(materialTextures[nonuniformEXT(PushConstants.normalIndex)], fragUV).rgb * 2.0 - 1.0;
        
        vec3 N = normalize(fragNormal);
        vec3 T = normalize(fragTangent);
        T = normalize(T - dot(T, N) * N);
        vec3 B = cross(N, T);
        mat3 TBN = mat3(T, B, N);
        
        outNormal = vec4(normalize(TBN * localNormal), 1.0);
    }

    if (PushConstants.albedoIndex == -1)
    {
        outAlbedoRough.rgb = PushConstants.tint.rgb;
    }
    else
    {
        outAlbedoRough.rgb = texture(materialTextures[nonuniformEXT(PushConstants.albedoIndex)], fragUV).rgb * PushConstants.tint.rgb;
    }

    if (PushConstants.ormIndex == -1)
    {
        outAlbedoRough.a = PushConstants.roughness;
        outMaterial.r = PushConstants.metallic;
        outMaterial.g = 1.0;
    }
    else
    {
        vec4 orm = texture(materialTextures[nonuniformEXT(PushConstants.ormIndex)], fragUV);
        outAlbedoRough.a = orm.g;
        outMaterial.r = orm.b;
        outMaterial.g = orm.r;
    }

    if (PushConstants.emissiveIndex == -1)
    {
        outEmissive = PushConstants.emissive;
    }
    else
    {
        outEmissive = texture(materialTextures[nonuniformEXT(PushConstants.emissiveIndex)], fragUV);
    }
}
