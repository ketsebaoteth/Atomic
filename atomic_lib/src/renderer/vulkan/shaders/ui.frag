#version 450

#define SHAPE_ROUNDED_RECT 0
#define SHAPE_CIRCLE 1
#define SHAPE_TEXT 2
#define SHAPE_IMAGE 3

#define GRADIENT_NONE 0
#define GRADIENT_LINEAR 1
#define GRADIENT_RADIAL 2

struct GradientStop {
    vec4 color;
    float position;
    float _pad0;
    float _pad1;
    float _pad2;
};

layout(std430, binding = 3) readonly buffer GradientBuffer {
    GradientStop gradientStops[];
};

layout(binding = 1) uniform sampler2D fontAtlas; 
layout(binding = 2) uniform sampler2D uiTexture[16];

layout(location = 0) in vec4 inColor;
layout(location = 1) in vec2 inUV;
layout(location = 2) in vec2 inSize;
layout(location = 3) in vec4 inRadius;
layout(location = 4) flat in uint inShapeType;

layout(location = 5) in float inStrokeWidth;
layout(location = 6) in vec4 inStrokeColor;
layout(location = 7) in float inDotGap;
layout(location = 8) in float inDotSize;
layout(location = 9) flat in uint inStrokePos;
layout(location = 10) in vec2 inUVMin;
layout(location = 11) in vec2 inUVMax;
layout(location = 12) flat in uint inTextureIndex;
layout(location = 13) flat in uint inGradientType;
layout(location = 14) in float inGradientDirection;
layout(location = 15) in vec2 inGradientCenter;
layout(location = 16) in float inGradientRadius;
layout(location = 17) flat in uint inGradientStopOffset;
layout(location = 18) flat in uint inGradientStopCount;
layout(location = 19) in float inOpacity;
layout(location = 20) flat in vec4 inClipRect;

layout(location = 0) out vec4 fColor;

float roundedBoxSDF(vec2 p, vec2 b, vec4 r){
  r.xy = (p.x > 0.0) ? r.zw : r.xy;
  r.x  = (p.y > 0.0) ? r.y  : r.x;
  vec2 q = abs(p) - b + r.x;
  return min(max(q.x, q.y), 0.0) + length(max(q, 0.0)) - r.x;
}

vec4 sampleGradient(float t)
{
    if (inGradientStopCount == 0)
        return inColor;

    GradientStop first =
        gradientStops[inGradientStopOffset];

    if (inGradientStopCount == 1)
        return first.color;

    if (t <= first.position)
        return first.color;

    for (uint i = 0;
         i < inGradientStopCount - 1;
         ++i)
    {
        GradientStop a =
            gradientStops[
                inGradientStopOffset + i];

        GradientStop b =
            gradientStops[
                inGradientStopOffset + i + 1];

        if (t >= a.position &&
            t <= b.position)
        {
            float localT =
                (t - a.position) /
                (b.position - a.position);

            return mix(
                a.color,
                b.color,
                smoothstep(0.0, 1.0, localT)
            );
        }
    }

    GradientStop last =
        gradientStops[
            inGradientStopOffset
            + inGradientStopCount - 1];

    return last.color;
}

vec4 computeGradient()
{
    if (inGradientType == GRADIENT_NONE)
        return inColor;

    float t = 0.0;

    // ----------------------------
    // Linear
    // ----------------------------

    if (inGradientType == GRADIENT_LINEAR)
    {
        vec2 dir = vec2(
            cos(inGradientDirection),
            sin(inGradientDirection)
        );

        vec2 centeredUV =
            inUV - vec2(0.5);

        t =
            dot(centeredUV, dir)
            + 0.5;
    }

    // ----------------------------
    // Radial
    // ----------------------------

    else if (inGradientType == GRADIENT_RADIAL)
    {
        float dist =
            distance(
                inUV,
                inGradientCenter
            );

        t =
            dist / inGradientRadius;
    }

    t = clamp(t, 0.0, 1.0);

    return sampleGradient(t);
}

void main(){

  if (inClipRect.z > inClipRect.x) {
      if (gl_FragCoord.x < inClipRect.x || gl_FragCoord.y < inClipRect.y || 
          gl_FragCoord.x > inClipRect.z || gl_FragCoord.y > inClipRect.w) {
          discard;
      }
  }

  if (inShapeType == SHAPE_TEXT) {
    vec2 texCoord = mix(inUVMin, inUVMax, inUV);

    float mask = texture(fontAtlas, texCoord).r;

    if (mask < 0.01) discard;

    vec4 baseColor = computeGradient();

    fColor = vec4(
        baseColor.rgb,
        baseColor.a * mask * inOpacity
    );

    return;
  }

  if(inShapeType == SHAPE_IMAGE){
    vec2 texCoord = mix(inUVMin, inUVMax, inUV);
    vec4 texColor = texture(uiTexture[inTextureIndex], texCoord);
    fColor = vec4(texColor.rgb, texColor.a * inOpacity);
    return;
  }

  vec2 center = inSize * 0.5;
  vec2 p = (inUV * inSize) - center;

  float d;
  switch(inShapeType) {
    case SHAPE_ROUNDED_RECT: 
      d = roundedBoxSDF(p, center, inRadius); 
      break;
    case SHAPE_CIRCLE: 
      d = length(p) - (inSize.x * 0.5); 
      break;
    default: 
      d = roundedBoxSDF(p, center, inRadius); 
      break;
  }

  float fillAlpha = 1.0 - smoothstep(-1.0, 1.0, d);

  vec4 baseColor = computeGradient();

  vec4 fragColor = vec4(
      baseColor.rgb,
      baseColor.a * fillAlpha
  );

  if (inStrokeWidth > 0.0) {
    float startEdge, endEdge;

    if (inStrokePos == 0) {
      startEdge = -inStrokeWidth;
      endEdge = 0.0;
    } else if (inStrokePos == 1) {
      startEdge = -inStrokeWidth * 0.5;
      endEdge = inStrokeWidth * 0.5;
    } else {
      startEdge = 0.0;
      endEdge = inStrokeWidth;
    }

    float strokeAlpha =
        smoothstep(-1.0, 1.0, d - startEdge) -
        smoothstep(-1.0, 1.0, d - endEdge);

    if (inDotGap > 0.0) {
      float s;

      if (abs(p.x) * inSize.y > abs(p.y) * inSize.x) {
        s = p.y;
      } else {
        s = p.x;
      }

      if (mod(s, inDotGap + inDotSize) > inDotSize) {
        strokeAlpha = 0.0;
      }
    }

    fragColor = mix(
        fragColor,
        inStrokeColor,
        strokeAlpha * inStrokeColor.a
    );
  }

  fColor = vec4(
      fragColor.rgb,
      fragColor.a * inOpacity
  );
}
