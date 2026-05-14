#version 450

#define MAX_TEXTURES 128
#define MAX_LIGHTS 16

layout(binding = 1) uniform sampler2D texSampler[MAX_TEXTURES];
layout(binding = 3) uniform sampler2D shadowMaps[MAX_LIGHTS];

layout(binding = 0) uniform CameraInfo
{
    mat4 view;
    mat4 proj;
    vec3 viewPos;
} camInfo;

layout(push_constant) uniform constants
{
    mat4 model;
    int texIndex;
} PushConstants;

struct Light
{
    vec4 position; // w = shadow map index
    vec4 color; // a = intensity
    vec4 direction; // w = 0 for point, w = 1 for directional, w = 2 for spotlight
    mat4 lightSpaceMatrix;
};

layout(std430, binding = 2) readonly buffer SceneLights
{
    uint lightCount;
    Light lights[];
} lightInfo;

layout(location = 0) in vec3 fragPos;
layout(location = 1) in vec3 fragNormal;
layout(location = 2) in vec2 fragTexCoord;
layout(location = 3) in vec3 fragColor;

layout(location = 0) out vec4 outColor;

float DirectionalShadow(vec3 normal, vec3 lightDir, int lightIndex)
{
    vec4 fragPosLightSpace = lightInfo.lights[lightIndex].lightSpaceMatrix * vec4(fragPos, 1.0);
    int shadowMapIndex = int(lightInfo.lights[lightIndex].position.w);

    vec3 projCoords = fragPosLightSpace.xyz / fragPosLightSpace.w;
    projCoords.xy = projCoords.xy * 0.5 + 0.5;

    if (projCoords.z > 1.0 || projCoords.z < 0.0)
        return 0.0;

    float currentDepth = projCoords.z;
    vec3 lightPos = lightInfo.lights[lightIndex].position.xyz;

    float bias = max(0.005 * (1.0 - dot(normal, lightDir)), 0.0005); 

    float shadow = 0.0;
    vec2 texelSize = 1.0 / textureSize(shadowMaps[shadowMapIndex], 0);
    for(int x = -1; x <= 1; ++x)
    {
        for(int y = -1; y <= 1; ++y)
        {
            float pcfDepth = texture(shadowMaps[shadowMapIndex], projCoords.xy + vec2(x, y) * texelSize).r; 
            shadow += currentDepth - bias > pcfDepth ? 1.0 : 0.0;
        }
    }
    return shadow / 9.0;
}

vec3 DirectionalLight(vec3 normal, vec3 viewDir, int lightIndex)
{
    vec3 lightDir = normalize(-lightInfo.lights[lightIndex].direction.xyz);

    // diffuse
    float diff = max(dot(normal, lightDir), 0.0);

    // specular
    vec3 halfwayDir = normalize(lightDir + viewDir);
    float spec = pow(max(dot(normal, halfwayDir), 0.0), 64.0);

    vec3 lightCol = lightInfo.lights[lightIndex].color.rgb;
    
    vec3 ambient = 0.15 * lightCol;
    vec3 diffuse = diff * lightCol;
    vec3 specular = spec * lightCol;

    float shadow = DirectionalShadow(normal, lightDir, lightIndex);

    return ambient + (1.0 - shadow) * (diffuse + specular);
}

vec3 PointLight(vec3 normal, vec3 viewDir, int lightIndex)
{
    vec3 lightPos = lightInfo.lights[lightIndex].position.xyz;

    vec3 L = lightPos - fragPos;
    float distance = length(L);
    L = normalize(L);

    // diffuse
    float diff = max(dot(normal, L), 0.0);

    // specular
    vec3 halfwayDir = normalize(L + viewDir);
    float spec = pow(max(dot(normal, halfwayDir), 0.0), 64.0);

    vec3 lightCol = lightInfo.lights[lightIndex].color.rgb;

    float attenuation = 1.0 / (distance * distance);

    vec3 ambient  = 0.05 * lightCol;
    vec3 diffuse  = diff * lightCol;
    vec3 specular = spec * lightCol;

    return (ambient + diffuse + specular) * attenuation;
}

void main()
{
    vec3 normal = normalize(fragNormal);
    vec3 viewDir = normalize(camInfo.viewPos - fragPos);

    vec3 color = texture(texSampler[PushConstants.texIndex], fragTexCoord).xyz * fragColor;
    vec3 lighting = vec3(0.0);

    for (uint i = 0; i < lightInfo.lightCount; ++i)
    {
        if (lightInfo.lights[i].direction.w == 1.0) // directional
        {
            lighting += DirectionalLight(normal, viewDir, int(i)) 
                        * lightInfo.lights[i].color.a;
        }
        else if (lightInfo.lights[i].direction.w == 0.0) // point
        {
            lighting += PointLight(normal, viewDir, int(i)) 
                        * lightInfo.lights[i].color.a;
        }
    }
    
    outColor = vec4(lighting * color, 1.0);
}
