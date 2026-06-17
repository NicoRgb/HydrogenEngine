#version 450

layout(location = 0) out vec4 outColor;

layout(binding = 0) uniform sampler2D gDepth;

layout(binding = 1) uniform CameraBuffer {
    mat4 viewProj;
    vec3 viewPos;
} camera;

layout(location = 0) in vec3 fragRayDir;

float ComputeGrid(vec2 st, float lineWidth)
{
    vec2 grid = abs(fract(st - 0.5) - 0.5) / fwidth(st);
    float line = min(grid.x, grid.y);
    return 1.0 - min(line / lineWidth, 1.0);
}

float ComputeAxis(float coord, float lineWidth)
{
    float axis = abs(coord) / fwidth(coord);
    return 1.0 - min(axis / lineWidth, 1.0);
}

void main() {
    vec2 uv = gl_FragCoord.xy / textureSize(gDepth, 0);
    vec3 R = normalize(fragRayDir);

    if (R.y == 0.0 || (camera.viewPos.y > 0.0 && R.y > 0.0) || (camera.viewPos.y < 0.0 && R.y < 0.0))
    {
        discard;
    }

    float t = -camera.viewPos.y / R.y;
    vec3 worldPos = camera.viewPos + t * R;

    vec4 clipPos = camera.viewProj * vec4(worldPos, 1.0);
    float gridDepth = clipPos.z / clipPos.w;
    float sceneDepth = texture(gDepth, uv).r;

    if (gridDepth > sceneDepth)
    {
        discard;
    }

    float minorGrid = ComputeGrid(worldPos.xz, 1.0);
    float majorGrid = ComputeGrid(worldPos.xz / 10.0, 1.0);

    float xAxisLine = ComputeAxis(worldPos.z, 2.0);
    float zAxisLine = ComputeAxis(worldPos.x, 2.0);

    float distance = length(worldPos - camera.viewPos);
    
    float maxDistance = 80.0; 
    float distanceFade = clamp(1.0 - (distance / maxDistance), 0.0, 1.0);
    
    float grazingFade = clamp(abs(R.y) * 5.0, 0.0, 1.0); 
    
    float finalFade = distanceFade * grazingFade;

    vec4 minorColor = vec4(0.23, 0.23, 0.23, 0.4);
    vec4 majorColor = vec4(0.29, 0.29, 0.29, 0.7);
    vec4 xAxisColor = vec4(0.85, 0.24, 0.31, 0.8);
    vec4 zAxisColor = vec4(0.27, 0.48, 0.90, 0.8);

    vec4 finalGridFormat = vec4(0.0);
    if (minorGrid > 0.0) finalGridFormat = minorColor * minorGrid;
    if (majorGrid > 0.0) finalGridFormat = mix(finalGridFormat, majorColor, majorGrid);
    if (xAxisLine > 0.0) finalGridFormat = mix(finalGridFormat, xAxisColor, xAxisLine);
    if (zAxisLine > 0.0) finalGridFormat = mix(finalGridFormat, zAxisColor, zAxisLine);

    finalGridFormat.a *= finalFade;

    if (finalGridFormat.a < 0.1) discard;

    outColor = finalGridFormat;
}
