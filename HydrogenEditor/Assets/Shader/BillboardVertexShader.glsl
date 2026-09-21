#version 450

layout(binding = 0, set = 0) uniform UniformBufferObject
{
    mat4 prevViewProj;
    mat4 view;
    mat4 proj;
    vec3 viewPos;
    float pad;
} ubo;

layout(push_constant) uniform GizmoData
{
    vec3 worldPosition;
    int textureIndex;
    vec2 scale;
} gizmo;

layout(location = 0) out vec2 fragUV;

void main()
{
    vec2 positions[3] = vec2[](
        vec2(-1.0, -1.0),
        vec2( 3.0, -1.0),
        vec2(-1.0,  3.0)
    );

    vec2 uvs[3] = vec2[](
        vec2(0.0, 0.0),
        vec2(2.0, 0.0),
        vec2(0.0, 2.0)
    );

    vec2 inPos = positions[gl_VertexIndex];
    fragUV = uvs[gl_VertexIndex];

    mat4 viewProj = ubo.proj * ubo.view;
    vec3 cameraRight = normalize(vec3(viewProj[0][0], viewProj[1][0], viewProj[2][0]));
    vec3 cameraUp = normalize(vec3(viewProj[0][1], viewProj[1][1], viewProj[2][1]));

    vec3 worldOffsetPos = gizmo.worldPosition 
                        + (cameraRight * inPos.x * gizmo.scale.x * 0.5) 
                        + (cameraUp    * inPos.y * gizmo.scale.y * 0.5);

    gl_Position = viewProj * vec4(worldOffsetPos, 1.0);
}
