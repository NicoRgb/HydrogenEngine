#version 450

#define MAX_TEXTURES 128
#define MAX_LIGHTS 16

layout(binding = 0) uniform sampler2D gPosition;
layout(binding = 1) uniform sampler2D gNormal;
layout(binding = 2) uniform sampler2D gAlbedoRough;
layout(binding = 3) uniform sampler2D gMaterial;
layout(binding = 4) uniform sampler2D gEmissive;

struct DirectionalLight
{
    vec4 color; // a = intensity
    vec4 direction;
};

layout(std430, binding = 5) readonly buffer SceneLights
{
    uint lightCount;
    DirectionalLight lights[];
} lightInfo;

layout(binding = 6) uniform UniformBufferObject
{
    mat4 viewProj;
    vec3 viewPos;
} ubo;

layout(location = 0) in vec2 fragUV;

layout(location = 0) out vec4 outColor;
layout(location = 1) out vec4 outBright;

vec3 CalculateDirectionalLight(vec3 normal, vec3 viewDir, int lightIndex, vec3 albedo)
{
    vec3 lightDir = normalize(-lightInfo.lights[lightIndex].direction.xyz);

    float diff = max(dot(normal, lightDir), 0.0);

    vec3 halfwayDir = normalize(lightDir + viewDir);
    float spec = pow(max(dot(normal, halfwayDir), 0.0), 64.0);

    vec3 lightCol = lightInfo.lights[lightIndex].color.rgb;

    vec3 diffuse = diff * albedo * lightCol;
    vec3 specular = spec * lightCol;

    return diffuse + specular;
}

void main()
{
    vec3 fragPos = texture(gPosition, fragUV).rgb;
    vec3 normal = texture(gNormal, fragUV).rgb;
    vec3 albedo = texture(gAlbedoRough, fragUV).rgb;
    vec4 material = texture(gMaterial, fragUV);
    float ao = material.g;

    vec4 emissiveSample = texture(gEmissive, fragUV);
    vec3 emissive = emissiveSample.rgb * emissiveSample.a;

    vec3 viewDir = normalize(ubo.viewPos - fragPos);

    vec3 lighting = vec3(0.0);
    for (uint i = 0; i < lightInfo.lightCount; ++i)
    {
        lighting += CalculateDirectionalLight(normal, viewDir, int(i), albedo) * lightInfo.lights[i].color.a;
    }

    vec3 ambient = vec3(0.03) * albedo * ao;    
    vec3 finalColor = lighting + ambient + emissive;

    outColor = vec4(finalColor, 1.0);

    float brightness = dot(outColor.rgb, vec3(0.2126, 0.7152, 0.0722));
    if(brightness > 1.0)
        outBright = vec4(outColor.rgb, 1.0);
    else
        outBright = vec4(0.0, 0.0, 0.0, 1.0);
}
