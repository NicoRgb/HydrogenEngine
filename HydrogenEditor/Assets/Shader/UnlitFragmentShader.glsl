#version 450

#define MAX_TEXTURES 128

layout(binding = 1) uniform sampler2D texSampler[MAX_TEXTURES];

layout(push_constant) uniform constants
{
    mat4 model;
    vec4 color;    // rgb = color tint, a = emissive strength (e.g., 10.0)
    int texIndex;
} PushConstants;

layout(location = 0) in vec3 fragPos;
layout(location = 1) in vec3 fragNormal;
layout(location = 2) in vec2 fragTexCoord;

layout(location = 0) out vec4 outColor;

void main()
{
    vec4 sampledColor = texture(texSampler[PushConstants.texIndex], fragTexCoord);
    
    vec3 baseColor = sampledColor.rgb * PushConstants.color.rgb;
    vec3 finalColor = baseColor * PushConstants.color.a;

    outColor = vec4(finalColor, 1.0);
}
