#version 450

#define MAX_TEXTURES 128
layout(binding = 1) uniform sampler2D albedoMaps[MAX_TEXTURES];
layout(binding = 2) uniform sampler2D normalMaps[MAX_TEXTURES];
layout(binding = 3) uniform sampler2D ormMaps[MAX_TEXTURES];
layout(binding = 4) uniform sampler2D emissiveMaps[MAX_TEXTURES];

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

    if (PushConstants.normalIndex > MAX_TEXTURES)
    {
        outNormal = vec4(normalize(fragNormal), 1.0);
    }
    else
    {
        vec3 localNormal = texture(normalMaps[PushConstants.normalIndex], fragUV).rgb * 2.0 - 1.0;
        
        vec3 N = normalize(fragNormal);
        vec3 T = normalize(fragTangent);
        T = normalize(T - dot(T, N) * N);
        vec3 B = cross(N, T);
        mat3 TBN = mat3(T, B, N);
        
        outNormal = vec4(normalize(TBN * localNormal), 1.0);
    }

    if (PushConstants.albedoIndex > MAX_TEXTURES)
    {
        outAlbedoRough.rgb = PushConstants.tint.rgb;
    }
    else
    {
        outAlbedoRough.rgb = texture(albedoMaps[PushConstants.albedoIndex], fragUV).rgb * PushConstants.tint.rgb;
    }

    if (PushConstants.ormIndex > MAX_TEXTURES)
    {
        outAlbedoRough.a = PushConstants.roughness;
        outMaterial.r = PushConstants.metallic;
        outMaterial.g = 1.0;
    }
    else
    {
        vec4 orm = texture(ormMaps[PushConstants.ormIndex], fragUV);
        outAlbedoRough.a = orm.g;
        outMaterial.r = orm.b;
        outMaterial.g = orm.r;
    }

    if (PushConstants.emissiveIndex > MAX_TEXTURES)
    {
        outEmissive = PushConstants.emissive;
    }
    else
    {
        outEmissive = texture(emissiveMaps[PushConstants.emissiveIndex], fragUV);
    }
}
