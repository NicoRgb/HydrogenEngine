#version 450
#extension GL_KHR_vulkan_glsl : enable
#extension GL_EXT_nonuniform_qualifier : enable

layout(set = 1, binding = 1) uniform sampler2D textures[];

layout(location = 0) in vec2 fragUV;
layout(location = 1) flat in int fragTextureIndex;

layout(location = 0) out vec4 outColor;

void main()
{
    if (fragTextureIndex >= 0)
    {
        vec4 texColor = texture(textures[nonuniformEXT(fragTextureIndex)], fragUV);
        if (texColor.a < 0.1) discard;
        outColor = texColor;
    }
    else
    {
        outColor = vec4(1.0, 0.0, 0.0, 1.0);
    }
}
