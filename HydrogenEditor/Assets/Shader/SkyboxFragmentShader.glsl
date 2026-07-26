#version 450
#extension GL_KHR_vulkan_glsl : enable

layout(location = 0) in vec3 inViewDir;

layout(location = 0) out vec4 outColor;

layout(binding = 0, set = 1) uniform samplerCube u_SkyboxCubemap;

void main()
{
    vec3 dir = normalize(inViewDir);
    outColor = texture(u_SkyboxCubemap, dir);
}
