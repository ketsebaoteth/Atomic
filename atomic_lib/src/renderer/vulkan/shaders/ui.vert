
#version 450

struct UIInstance {
    vec2 pos;             // Offset 0
    vec2 size;            // Offset 8
    vec4 color;           // Offset 16
    vec4 radius;          // Offset 32
    uint shapeType;       // Offset 48
    float strokeWidth;    // Offset 52
    uint strokePosition;  // Offset 56 (0=Inner, 1=Center, 2=Outer)
    float dotGap;         // Offset 60
    float dotSize;        // Offset 64
    float _pad[3];        // Offset 68 (Takes exactly 12 bytes!)
    vec4 strokeColor;     // Offset 80 (Now matches C++ perfectly!)
};

layout(std430, binding = 0) readonly buffer UIBuffer {
    UIInstance instances[];
};

layout(push_constant) uniform Globals {
    vec2 resolution;
} globals;

layout(location = 0) out vec4 outColor;
layout(location = 1) out vec2 outUV;
layout(location = 2) out vec2 outSize;
layout(location = 3) out vec4 outRadius;
layout(location = 4) flat out uint outShapeType;

layout(location = 5) out float outStrokeWidth;
layout(location = 6) out vec4 outStrokeColor;
layout(location = 7) out float outDotGap;  // 0 = solid
layout(location = 8) out float outDotSize;
layout(location = 9) flat out uint outStrokePos;

vec2 positions[6] = vec2[](
    vec2(0.0, 0.0), vec2(1.0, 0.0), vec2(0.0, 1.0),
    vec2(0.0, 1.0), vec2(1.0, 0.0), vec2(1.0, 1.0)
);

void main() {
    UIInstance data = instances[gl_InstanceIndex];
    vec2 p = positions[gl_VertexIndex];

    // 1. Calculate how much we need to expand the quad to prevent clipping
    float expansion = 0.0;
    if (data.strokePosition == 1) expansion = data.strokeWidth * 0.5; // Center
    if (data.strokePosition == 2) expansion = data.strokeWidth;       // Outer

    vec2 expandedPos = data.pos - vec2(expansion);
    vec2 expandedSize = data.size + vec2(expansion * 2.0);

    // 2. Map the UV relative to the ORIGINAL shape bounds, even though the quad is larger!
    // This allows outUV to safely go below 0.0 and above 1.0
    vec2 screenPos = expandedPos + (p * expandedSize);
    outUV = (screenPos - data.pos) / data.size; 

    // 3. Pass data dow


    outColor = data.color;
    outSize = data.size; // Pass original size
    outRadius = data.radius;
    outShapeType = data.shapeType;
    outStrokeWidth = data.strokeWidth;
    outStrokeColor = data.strokeColor;
    outDotGap = data.dotGap;
    outDotSize = data.dotSize;
    outStrokePos = data.strokePosition;

    vec2 normalizedPos = screenPos / globals.resolution;
    gl_Position = vec4(normalizedPos * 2.0 - 1.0, 0.0, 1.0);
}
