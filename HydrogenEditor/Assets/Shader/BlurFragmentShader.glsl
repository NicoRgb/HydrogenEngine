#version 450

#extension GL_KHR_vulkan_glsl : enable

layout(set = 0, binding = 0, set = 1) uniform sampler2D hdrScene;

layout(push_constant) uniform constants
{
    int horizontal;
} PushConstants;

layout(location = 0) in vec2 inUV;
layout(location = 0) out vec4 outColor;

void main()
{
    float weight[5] = float[] (0.227027, 0.1945946, 0.1216216, 0.054054, 0.016216);
    
    vec2 tex_offset = 1.0 / textureSize(hdrScene, 0);
    vec3 result = texture(hdrScene, inUV).rgb * weight[0];
    if (PushConstants.horizontal == 1)
    {
        for(int i = 1; i < 5; ++i)
        {
            result += texture(hdrScene, inUV + vec2(tex_offset.x * i, 0.0)).rgb * weight[i];
            result += texture(hdrScene, inUV - vec2(tex_offset.x * i, 0.0)).rgb * weight[i];
        }
    }
    else
    {
        for(int i = 1; i < 5; ++i)
        {
            result += texture(hdrScene, inUV + vec2(0.0, tex_offset.y * i)).rgb * weight[i];
            result += texture(hdrScene, inUV - vec2(0.0, tex_offset.y * i)).rgb * weight[i];
        }
    }

    outColor = vec4(result, 1.0);
}
