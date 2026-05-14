#version 450

layout(set = 0, binding = 0) uniform sampler2D hdrScene;

layout(location = 0) in vec2 fragUV;
layout(location = 0) out vec4 outColor;

vec3 ACESFilm(vec3 x)
{
    float a = 2.51;
    float b = 0.03;
    float c = 2.43;
    float d = 0.59;
    float e = 0.14;
    return clamp((x*(a*x+b))/(x*(c*x+d)+e), 0.0, 1.0);
}

void main()
{
    vec3 hdrColor = texture(hdrScene, fragUV).rgb;
    
    vec3 mapped = ACESFilm(hdrColor);
    outColor = vec4(mapped, 1.0);
}
