#version 450

#define MAX_TEXTURES 128

layout(set = 0, binding = 1) uniform sampler2D texSampler[MAX_TEXTURES];

layout(binding = 0) uniform UniformBufferObject
{
    mat4 view;
    mat4 proj;
    vec3 viewPos;   // camera position
} ubo;

layout(push_constant) uniform constants
{
    mat4 model;
    int texIndex;
} PushConstants;

struct Light
{
    vec4 position;   // xyz = position, w = radius
    vec4 color;      // rgb = color, w = intensity
};

layout(std430, binding = 2) readonly buffer SceneLights
{
    uint lightCount;
    Light lights[];
} Lights;

layout(location = 0) in vec3 fragPos;
layout(location = 1) in vec3 fragNormal;
layout(location = 2) in vec2 fragTexCoord;
layout(location = 3) in vec3 fragColor;

layout(location = 0) out vec4 outColor;

vec3 AccumulateLights(vec3 fragPos, vec3 normal)
{
    vec3 ambient = vec3(0.05);   // simple global ambient
    vec3 result = ambient;

    vec3 N = normalize(normal);
    vec3 V = normalize(ubo.viewPos - fragPos);

    for (uint i = 0; i < Lights.lightCount; ++i)
    {
        Light light = Lights.lights[i];

        vec3 L = light.position.xyz - fragPos;
        float dist = length(L);

        if (dist > light.position.w)
            continue;

        vec3 Ldir = normalize(L);

        // Smooth attenuation
        float attenuation = 1.0 - (dist / light.position.w);
        attenuation *= attenuation;

        // ---- Diffuse (Lambert) ----
        float diff = max(dot(N, Ldir), 0.0);
        vec3 diffuse = diff * light.color.rgb;

        // ---- Specular (Phong) ----
        vec3 R = reflect(-Ldir, N);
        float spec = pow(max(dot(V, R), 0.0), 32.0);
        vec3 specular = spec * light.color.rgb;

        vec3 contribution =
            (diffuse + specular) *
            light.color.a *      // intensity
            attenuation;

        result += contribution;
    }

    return result;
}

void main()
{
    vec4 albedo = texture(texSampler[PushConstants.texIndex], fragTexCoord);

    vec3 lighting = AccumulateLights(fragPos, fragNormal);

    vec3 finalColor = albedo.rgb * fragColor * lighting;

    outColor = vec4(finalColor, albedo.a);
}
