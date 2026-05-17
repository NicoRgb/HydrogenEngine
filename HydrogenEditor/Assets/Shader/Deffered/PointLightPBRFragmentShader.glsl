#version 450

layout(location = 0) out vec4 outColor;
layout(location = 1) out vec4 outBright;

layout(binding = 0) uniform sampler2D gPosition;
layout(binding = 1) uniform sampler2D gNormal;
layout(binding = 2) uniform sampler2D gAlbedoRough;
layout(binding = 3) uniform sampler2D gMaterial; // r = metallic, g = ao
layout(binding = 4) uniform sampler2D gEmissive;

layout(binding = 5) uniform CameraBuffer
{
    mat4 viewProj;
    vec3 viewPos;
} ubo;

layout(push_constant) uniform LightData
{
    mat4 model;
    vec4 color; // a = intensity
    vec3 position;
    float radius;
} light;

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

void main()
{
    vec2 uv = gl_FragCoord.xy / textureSize(gPosition, 0);

    vec3 fragPos = texture(gPosition, uv).rgb;

    vec3 L = light.position - fragPos;
    float dist = length(L);

    if (dist > light.radius)
    {
        discard;
    }

    L = normalize(L);

    vec3 normal = texture(gNormal, uv).rgb;

    vec4 albedoRough = texture(gAlbedoRough, uv);
    vec3 albedo = albedoRough.rgb;
    float roughness = albedoRough.a;

    vec4 material = texture(gMaterial, uv);
    float metallic = material.r;
    float ao = material.g;

    vec3 N = normal;
    vec3 V = normalize(ubo.viewPos - fragPos);
    vec3 H = normalize(V + L);

    float atten = pow(clamp(1.0 - (dist * dist) / (light.radius * light.radius), 0.0, 1.0), 2.0); 
    vec3 radiance = light.color.rgb * atten;

    vec3 F0 = vec3(0.04); 
    F0 = mix(F0, albedo, metallic);
    vec3 F = FresnelSchlick(max(dot(H, V), 0.0), F0);

    float NDF = DistributionGGX(N, H, roughness);
    float G = GeometrySmith(N, V, L, roughness);

    vec3 numerator = NDF * G * F;
    float denominator = 4.0 * max(dot(N, V), 0.0) * max(dot(N, L), 0.0) + 0.0001;
    vec3 specular = numerator / denominator;

    vec3 kS = F;
    vec3 kD = vec3(1.0) - kS;
    kD *= 1.0 - metallic;

    const float PI = 3.14159265359;
  
    float NdotL = max(dot(N, L), 0.0);
    vec3 Lo = (kD * albedo / PI + specular) * radiance * NdotL;

    vec3 ambient = vec3(0.03) * albedo * ao; // temporary
    vec3 color = ambient + Lo;

    outColor = vec4(color, 1.0);
    outBright = vec4(0.0);
}
