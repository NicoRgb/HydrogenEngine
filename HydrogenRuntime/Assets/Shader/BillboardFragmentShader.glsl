#version 450

#define MAX_TEXTURES 128
layout(binding = 0) uniform sampler2D textures[MAX_TEXTURES];

layout(location = 0) in vec2 fragUV;

layout(location = 0) out vec4 outColor;

layout(push_constant) uniform GizmoData
{
    vec3 worldPosition;
    int textureIndex;
    vec2 scale;
} gizmo;

void main()
{
    vec4 texColor = texture(textures[gizmo.textureIndex], fragUV);
    
    if(texColor.a < 0.1) discard; 

    outColor = texColor;
    outColor.a = 0.25;
}
