#version 450

layout(binding = 0) uniform UniformBufferObject
{
    mat4 viewProj;
    vec3 viewPos;
} ubo;

layout(push_constant) uniform constants
{
    mat4 model;
    vec4 color;
    int texIndex;
} PushConstants;

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec2 inTexCoord;
layout(location = 2) in vec3 inNormal;

layout(location = 0) out vec3 fragPos;
layout(location = 1) out vec2 fragUV;
layout(location = 2) out vec3 fragNormal;

void main()
{
    vec4 worldPos = PushConstants.model * vec4(inPosition, 1.0);
    fragPos = worldPos.xyz;
    fragUV = inTexCoord;
    fragNormal = mat3(transpose(inverse(PushConstants.model))) * inNormal;
    
    gl_Position = ubo.viewProj * worldPos;
}
