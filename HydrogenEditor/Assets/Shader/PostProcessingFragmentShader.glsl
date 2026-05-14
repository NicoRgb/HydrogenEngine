#version 450

layout(set = 0, binding = 0) uniform sampler2D hdrScene;

layout(location = 0) in vec2 fragUV;
layout(location = 0) out vec4 outColor;

void main()
{
    vec3 hdrColor = texture(hdrScene, fragUV).rgb;
    vec3 mapped = hdrColor / (hdrColor + vec3(1.0));
    
    outColor = vec4(mapped, 1.0);
}
