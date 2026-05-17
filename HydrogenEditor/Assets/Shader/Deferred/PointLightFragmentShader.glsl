#version 450

layout(location = 0) out vec4 outColor;
layout(location = 1) out vec4 outBright;

layout(binding = 0) uniform sampler2D gPosition;
layout(binding = 1) uniform sampler2D gNormal;
layout(binding = 2) uniform sampler2D gAlbedoRough;
layout(binding = 3) uniform sampler2D gMaterial;
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

void main()
{
    vec2 uv = gl_FragCoord.xy / textureSize(gPosition, 0);

    vec3 fragPos = texture(gPosition, uv).rgb;
    vec3 normal = texture(gNormal, uv).rgb;
    vec3 albedo = texture(gAlbedoRough, uv).rgb;

    vec3 L = light.position - fragPos;
    float dist = length(L);

    if (dist > light.radius)
    {
        discard;
    }

    L = normalize(L);
    vec3 V = normalize(ubo.viewPos - fragPos);
    vec3 H = normalize(L + V);

    float atten = pow(clamp(1.0 - (dist * dist) / (light.radius * light.radius), 0.0, 1.0), 2.0);

    // diffuse
    float diff = max(dot(normal, L), 0.0);
    
    // specular
    float spec = pow(max(dot(normal, H), 0.0), 32.0);

    vec3 diffuseTerm = albedo * light.color.rgb * diff;
    vec3 specularTerm = light.color.rgb * spec;

    vec3 result = (diffuseTerm + specularTerm) * light.color.a * atten;

    outColor = vec4(result, 1.0);
    outBright = vec4(0.0);
}
