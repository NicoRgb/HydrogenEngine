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

vec3 FresnelSchlick(float cosTheta, vec3 F0)
{
    return F0 + (1.0 - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}

float DistributionGGX(vec3 N, vec3 H, float roughness)
{
    const float PI = 3.14159265359;

    float a = roughness*roughness;
    float a2 = a*a;
    float NdotH = max(dot(N, H), 0.0);
    float NdotH2 = NdotH*NdotH;
	
    float num = a2;
    float denom = (NdotH2 * (a2 - 1.0) + 1.0);
    denom = PI * denom * denom;
	
    return num / denom;
}

float GeometrySchlickGGX(float NdotV, float roughness)
{
    float r = (roughness + 1.0);
    float k = (r*r) / 8.0;

    float num = NdotV;
    float denom = NdotV * (1.0 - k) + k;
	
    return num / denom;
}

float GeometrySmith(vec3 N, vec3 V, vec3 L, float roughness)
{
    float NdotV = max(dot(N, V), 0.0);
    float NdotL = max(dot(N, L), 0.0);
    float ggx2 = GeometrySchlickGGX(NdotV, roughness);
    float ggx1 = GeometrySchlickGGX(NdotL, roughness);
	
    return ggx1 * ggx2;
}

vec3 CalculateDirectionalLight(vec3 N, vec3 V, int lightIndex, vec3 albedo, float roughness, float metallic)
{
    vec3 L = normalize(-lightInfo.lights[lightIndex].direction.xyz);
    vec3 H = normalize(V + L);

    vec3 radiance = lightInfo.lights[lightIndex].color.rgb * lightInfo.lights[lightIndex].color.a;

    vec3 F0 = vec3(0.04); 
    F0 = mix(F0, albedo, metallic);
    vec3 F = FresnelSchlick(max(dot(H, V), 0.0), F0);

    float NDF = DistributionGGX(N, H, roughness);
    float G = GeometrySmith(N, V, L, roughness);
    
    vec3 numerator = NDF * G * F;
    float denominator = 4.0 * max(dot(N, V), 0.0) * max(dot(N, L), 0.0) + 0.0001; // Prevent divide by zero
    vec3 specular = numerator / denominator;
    
    vec3 kS = F;
    vec3 kD = vec3(1.0) - kS;
    kD *= 1.0 - metallic;
    
    const float PI = 3.14159265359;
    vec3 diffuse = kD * albedo / PI;

    float NdotL = max(dot(N, L), 0.0);        
    return (diffuse + specular) * radiance * NdotL;
}

void main()
{
    vec3 fragPos = texture(gPosition, fragUV).rgb;
    vec3 normal = texture(gNormal, fragUV).rgb;
    vec3 N = normalize(normal);
    
    vec4 albedoRough = texture(gAlbedoRough, fragUV);
    vec3 albedo = albedoRough.rgb;
    float roughness = albedoRough.a;

    vec4 material = texture(gMaterial, fragUV);
    float metallic = material.r;
    float ao = material.g;
    
    vec4 emissiveSample = texture(gEmissive, fragUV);
    vec3 emissive = emissiveSample.rgb * emissiveSample.a;

    vec3 viewDir = normalize(ubo.viewPos - fragPos);

    vec3 lighting = vec3(0.0);
    for (uint i = 0; i < lightInfo.lightCount; ++i)
    {
        lighting += CalculateDirectionalLight(N, viewDir, int(i), albedo, roughness, metallic);
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
