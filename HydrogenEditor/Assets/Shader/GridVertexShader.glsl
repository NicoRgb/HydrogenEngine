#version 450

layout(location = 0) in vec2 inPos;
layout(location = 1) in vec2 inUV;

layout(binding = 1) uniform CameraInfo
{
    mat4 viewProj;
    vec3 viewPos;
} camera;

layout(location = 0) out vec3 fragRayDir;

void main()
{
    mat4 invViewProj = inverse(camera.viewProj);

    vec4 farPlaneTarget = invViewProj * vec4(inPos, 1.0, 1.0);
    vec3 worldPosFar = farPlaneTarget.xyz / farPlaneTarget.w;

    fragRayDir = worldPosFar - camera.viewPos;

    gl_Position = vec4(inPos, 0.0, 1.0);
}
