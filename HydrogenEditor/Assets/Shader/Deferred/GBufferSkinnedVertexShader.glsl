#version 450

layout(binding = 0, set = 0) uniform UniformBufferObject
{
    mat4 view;
    mat4 proj;
    vec3 viewPos;
    float pad;
} ubo;

layout(std430, binding = 1, set = 1) readonly buffer DynamicBoneData
{
    mat4 allBones[];
};

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

    int boneBaseIndex;
    float padding1;
    
    vec4 emissive;
} PushConstants;

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec2 inTexCoord;
layout(location = 2) in vec3 inNormal;
layout(location = 3) in vec3 inTangent;
layout(location = 4) in ivec4 inBoneIDs;
layout(location = 5) in vec4 inWeights;

layout(location = 0) out vec3 fragPos;
layout(location = 1) out vec2 fragUV;
layout(location = 2) out vec3 fragNormal;
layout(location = 3) out vec3 fragTangent;

void main()
{
    float totalWeight = inWeights.x + inWeights.y + inWeights.z + inWeights.w;
    
    mat4 boneTransform = mat4(1.0);

    if (totalWeight > 0.0)
    {
        boneTransform  = allBones[PushConstants.boneBaseIndex + inBoneIDs.x] * inWeights.x;
        boneTransform += allBones[PushConstants.boneBaseIndex + inBoneIDs.y] * inWeights.y;
        boneTransform += allBones[PushConstants.boneBaseIndex + inBoneIDs.z] * inWeights.z;
        boneTransform += allBones[PushConstants.boneBaseIndex + inBoneIDs.w] * inWeights.w;
    }

    vec4 skinnedPosition = boneTransform * vec4(inPosition, 1.0);
    vec4 worldPos = PushConstants.model * skinnedPosition;

    fragPos = worldPos.xyz;
    fragUV = inTexCoord;

    mat3 normalMatrix = mat3(transpose(inverse(PushConstants.model)));
    
    fragNormal = normalMatrix * inNormal;
    fragTangent = normalMatrix * inTangent;

    gl_Position = ubo.proj * ubo.view * worldPos;
}
