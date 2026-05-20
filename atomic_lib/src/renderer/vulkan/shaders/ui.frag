#version 450
#define SHAPE_ROUNDED_RECT 0
#define SHAPE_CIRCLE 1
#define SHAPE_TEXT 2
#define SHAPE_IMAGE 3

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

layout(location = 0) out vec4 fColor;

float roundedBoxSDF(vec2 p, vec2 b, vec4 r){
  r.xy = (p.x > 0.0) ? r.zw : r.xy;
  r.x  = (p.y > 0.0) ? r.y  : r.x;
  vec2 q = abs(p) - b + r.x;
  return min(max(q.x, q.y), 0.0) + length(max(q, 0.0)) - r.x;
}

void main(){
  if (inShapeType == SHAPE_TEXT) {
    vec2 texCoord = mix(inUVMin, inUVMax, inUV);
    
    float mask = texture(fontAtlas, texCoord).r;
    
    if (mask < 0.01) discard;
    
    fColor = vec4(inColor.rgb, inColor.a * mask);
    return;
  }

  if(inShapeType == SHAPE_IMAGE){
    vec2 texCoord = mix(inUVMin, inUVMax, inUV);
    vec4 texColor = texture(uiTexture[inTextureIndex], texCoord);
    fColor = texColor;
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
  vec4 fragColor = vec4(inColor.rgb, inColor.a * fillAlpha);

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

    float strokeAlpha = smoothstep(-1.0, 1.0, d - startEdge) - smoothstep(-1.0, 1.0, d - endEdge);

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
    fragColor = mix(fragColor, inStrokeColor, strokeAlpha * inStrokeColor.a);
  }
  
  fColor = fragColor;
}
