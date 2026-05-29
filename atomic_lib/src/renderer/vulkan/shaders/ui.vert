#version 450

// struct UIInstance {
//     vec2 pos;             // Offset 0
//     vec2 size;            // Offset 8
//     vec4 color;           // Offset 16
//     vec4 radius;          // Offset 32
//     uint shapeType;       // Offset 48
//     float strokeWidth;    // Offset 52
//     uint strokePosition;  // Offset 56 (0=Inner, 1=Center, 2=Outer)
//     float dotGap;         // Offset 60
//     float dotSize;        // Offset 64
//
//     uint textureIndex;    // Offset 68 -> Explicitly occupies the 4-byte alignment hole!
//
//     vec2 uvMin;           // Offset 72 -> Correctly aligned on an 8-byte boundary
//     vec2 uvMax;           // Offset 80
//     vec4 strokeColor;     // Offset 88
// };

struct GradientStop {
    vec4 color;
    float position;
    float _pad0;
    float _pad1;
    float _pad2;
};

struct UIInstance {
    vec2 pos;
    vec2 size;

    vec4 backgroundColor;
    vec4 radius;

    float opacity;
    uint shapeType;

    float strokeWidth;
    uint strokePosition;
    float dotGap;
    float dotSize;

    uint textureIndex;
    uint _padTex;

    vec2 uvMin;
    vec2 uvMax;

    vec4 strokeColor;

    uint gradientType;

    float gradientDirection;

    vec2 gradientCenter;

    float gradientRadius;

    uint gradientStopOffset;
    uint gradientStopCount;

    uint _padGradient;
    
    vec4 clipRect;
};

layout(std430, binding = 0) readonly buffer UIBuffer {
    UIInstance instances[];
};

layout(push_constant) uniform Globals {
    vec2 resolution;
} globals;

// Shader Interface Outputs
layout(location = 0) out vec4 outColor;
layout(location = 1) out vec2 outUV;
layout(location = 2) out vec2 outSize;
layout(location = 3) out vec4 outRadius;
layout(location = 4) flat out uint outShapeType;

layout(location = 5) out float outStrokeWidth;
layout(location = 6) out vec4 outStrokeColor;
layout(location = 7) out float outDotGap;  
layout(location = 8) out float outDotSize;
layout(location = 9) flat out uint outStrokePos;
layout(location = 10) out vec2 outUVMin;
layout(location = 11) out vec2 outUVMax;
layout(location = 12) flat out uint outTextureIndex;
layout(location = 13) flat out uint outGradientType;
layout(location = 14) out float outGradientDirection;
layout(location = 15) out vec2 outGradientCenter;
layout(location = 16) out float outGradientRadius;
layout(location = 17) flat out uint outGradientStopOffset;
layout(location = 18) flat out uint outGradientStopCount;
layout(location = 19) out float outOpacity;
layout(location = 20) flat out vec4 outClipRect;

const vec2 positions[6] = vec2[](
    vec2(0.0, 0.0), vec2(1.0, 0.0), vec2(0.0, 1.0),
    vec2(0.0, 1.0), vec2(1.0, 0.0), vec2(1.0, 1.0)
);

void main() {
    UIInstance data = instances[gl_InstanceIndex];
    vec2 p = positions[gl_VertexIndex];

    float expansion = 0.0;
    if (data.shapeType != 3) { // Skip stroke expansion if drawing text (ShapeType == 3)
        if (data.strokePosition == 1) expansion = data.strokeWidth * 0.5; // Center
        if (data.strokePosition == 2) expansion = data.strokeWidth;       // Outer
    }

    vec2 expandedPos = data.pos - vec2(expansion);
    vec2 expandedSize = data.size + vec2(expansion * 2.0);

    vec2 screenPos = expandedPos + (p * expandedSize);
    outUV = (screenPos - data.pos) / data.size; 

    outColor        = data.backgroundColor;
    outSize         = data.size;
    outRadius       = data.radius;
    outShapeType    = data.shapeType;
    outOpacity      = data.opacity;
    outStrokeWidth  = data.strokeWidth;
    outStrokeColor  = data.strokeColor;
    outDotGap       = data.dotGap;
    outDotSize      = data.dotSize;
    outStrokePos    = data.strokePosition;
    outUVMin        = data.uvMin;
    outUVMax        = data.uvMax;
    outTextureIndex = data.textureIndex;
    outGradientType       = data.gradientType;
    outGradientDirection  = data.gradientDirection;
    outGradientCenter     = data.gradientCenter;
    outGradientRadius     = data.gradientRadius;
    outGradientStopOffset = data.gradientStopOffset;
    outGradientStopCount  = data.gradientStopCount;
    outClipRect           = data.clipRect;

    vec2 normalizedPos = screenPos / globals.resolution;
    gl_Position = vec4(normalizedPos * 2.0 - 1.0, 0.0, 1.0);
}
