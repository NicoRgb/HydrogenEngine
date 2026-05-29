#version 450

layout(location = 0) in vec3 inPos;

layout(binding = 1) uniform CameraBuffer
{
    mat4 view;
    mat4 proj;
    vec3 viewPos;
} ubo;

layout(location = 0) out vec3 fragTexCoord;

void main()
{
    fragTexCoord = inPos;

    vec4 pos = ubo.proj * mat4(mat3(ubo.view)) * vec4(inPos, 1.0);
    gl_Position = pos.xyww;
}
