layout (location = 0) in vec3 aVertexPosition;
layout (location = 1) in vec3 aNormalPosition;
layout (location = 2) in vec2 aTextureCoord;

uniform mat4 uLightVP;
uniform mat4 uModelMatrix;
uniform mat4 uViewMatrix;
uniform mat4 uProjectionMatrix;

out vec4 fragPosLightSpace;
out vec2 vTextureCoord;
out vec3 vNormalWorld;
out vec4 vPosWorld;
out float vDepth;

#ifdef TAA
uniform mat4 uPreModelMatrix;
uniform mat4 uPreViewMatrix;
uniform mat4 uPreProjectionMatrix;
uniform float uScreenWidth;
uniform float uScreenHeight;
uniform int uFrameCount;

out vec4 vCurrentPos;
out vec4 vPreviousPos;

const vec2 Halton_2_3[8] =
{
    vec2(0.0f, -1.0f / 3.0f),
    vec2(-1.0f / 2.0f, 1.0f / 3.0f),
    vec2(1.0f / 2.0f, -7.0f / 9.0f),
    vec2(-3.0f / 4.0f, -1.0f / 9.0f),
    vec2(1.0f / 4.0f, 5.0f / 9.0f),
    vec2(-1.0f / 4.0f, -5.0f / 9.0f),
    vec2(3.0f / 4.0f, 1.0f / 9.0f),
    vec2(-7.0f / 8.0f, 7.0f / 9.0f)
};
#endif //TAA

void main(void) {
  vec4 posWorld = uModelMatrix * vec4(aVertexPosition, 1.0);
  vPosWorld = posWorld.xyzw / posWorld.w;
  vec4 normalWorld = uModelMatrix * vec4(aNormalPosition, 0.0);
  vNormalWorld = normalize(normalWorld.xyz);
  vTextureCoord = aTextureCoord;
  fragPosLightSpace = uLightVP * posWorld;

#ifdef TAA
  float deltaWidth = 1.0/uScreenWidth, deltaHeight = 1.0/uScreenHeight;
  vec2 jitter = vec2(
      Halton_2_3[uFrameCount % 8].x * deltaWidth,
      Halton_2_3[uFrameCount % 8].y * deltaHeight
  );
  mat4 jitterMat = uProjectionMatrix;
  jitterMat[2][0] += jitter.x;
  jitterMat[2][1] += jitter.y;

  vCurrentPos = uProjectionMatrix * uViewMatrix * uModelMatrix * vec4(aVertexPosition, 1.0);
  vPreviousPos = uPreProjectionMatrix * uPreViewMatrix * uPreModelMatrix * vec4(aVertexPosition, 1.0);
  gl_Position = jitterMat * uViewMatrix * uModelMatrix * vec4(aVertexPosition, 1.0);

#else
  gl_Position = uProjectionMatrix * uViewMatrix * uModelMatrix * vec4(aVertexPosition, 1.0);
#endif //TAA
  vDepth = gl_Position.w;

}