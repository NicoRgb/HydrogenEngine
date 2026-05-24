#version 450

layout(binding = 0) uniform UniformBufferObject
{
    mat4 viewProj;
    vec3 viewPos;
} ubo;

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
    uint objectID;
    float padding0;
    vec4 emissive;
} PushConstants;

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec2 inTexCoord;
layout(location = 2) in vec3 inNormal;
layout(location = 3) in vec3 inTangent;

layout(location = 0) out vec3 fragPos;
layout(location = 1) out vec2 fragUV;
layout(location = 2) out vec3 fragNormal;
layout(location = 3) out vec3 fragTangent;

void main()
{
    vec4 worldPos = PushConstants.model * vec4(inPosition, 1.0);
    fragPos = worldPos.xyz;
    fragUV = inTexCoord;

    mat3 normalMatrix = mat3(transpose(inverse(PushConstants.model)));
    
    fragNormal = normalMatrix * inNormal;
    fragTangent = normalMatrix * inTangent;

    gl_Position = ubo.viewProj * worldPos;
}
