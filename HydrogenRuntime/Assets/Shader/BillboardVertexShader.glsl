#version 450

layout(location = 0) in vec2 inPos;
layout(location = 1) in vec2 inUV;

layout(binding = 1) uniform CameraInfo
{
    mat4 viewProj;
    vec3 viewPos;
} camera;

layout(push_constant) uniform GizmoData
{
    vec3 worldPosition;
    int textureIndex;
    vec2 scale;
} gizmo;

layout(location = 0) out vec2 fragUV;

void main()
{
    fragUV = inUV;

    vec3 cameraRight = normalize(vec3(camera.viewProj[0][0], camera.viewProj[1][0], camera.viewProj[2][0]));
    vec3 cameraUp = normalize(vec3(camera.viewProj[0][1], camera.viewProj[1][1], camera.viewProj[2][1]));

    vec3 worldOffsetPos = gizmo.worldPosition 
                        + (cameraRight * inPos.x * gizmo.scale.x * 0.5) 
                        + (cameraUp    * inPos.y * gizmo.scale.y * 0.5);

    gl_Position = camera.viewProj * vec4(worldOffsetPos, 1.0);
}
