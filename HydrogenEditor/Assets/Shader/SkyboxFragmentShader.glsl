#version 450

layout(location = 0) out vec4 outColor;
layout(location = 1) out vec4 outBright;

layout(binding = 0) uniform samplerCube skyboxCubemap;

layout(location = 0) in vec3 fragTexCoord;

void main()
{
    vec3 envColor = texture(skyboxCubemap, fragTexCoord).rgb;
    outColor = vec4(envColor, 1.0);
    outBright = vec4(0.0);
}
