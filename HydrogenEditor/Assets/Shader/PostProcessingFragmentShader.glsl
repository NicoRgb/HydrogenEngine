#version 450

#extension GL_KHR_vulkan_glsl : enable

layout(set = 0, binding = 0, set = 1) uniform sampler2D hdrScene;
layout(set = 0, binding = 1, set = 1) uniform sampler2D bloomBlur;

layout(location = 0) in vec2 inUV;

layout(location = 0) out vec4 outColor;

void main()
{
    vec3 hdrColor = texture(hdrScene, inUV).rgb;
    vec3 bloomColor = texture(bloomBlur, inUV).rgb;
    hdrColor += bloomColor;

    vec3 mapped = hdrColor / (hdrColor + vec3(1.0));
    
    outColor = vec4(mapped, 1.0);
}
