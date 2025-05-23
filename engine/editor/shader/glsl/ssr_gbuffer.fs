#ifdef HAS_DIFFUSE_MAP
  uniform sampler2D udiffuseMap;
#else
  uniform vec3 uKd;
#endif

#ifdef HAS_NORMAL_MAP
  uniform sampler2D uNormalTexture;
#endif
uniform sampler2D uShadowMap;
uniform float uMetallic;
uniform float uRoughness;

in vec4 fragPosLightSpace;
in vec2 vTextureCoord;
in vec4 vPosWorld;
in vec3 vNormalWorld;
in float vDepth;

layout(location = 0) out vec4 outColor;
layout(location = 1) out vec4 outDepth;
layout(location = 2) out vec3 outNormal;
layout(location = 3) out vec3 outVRM;
layout(location = 4) out vec3 outWorldPos;

float SimpleShadowMap(vec3 posWorld,float bias){
  vec3 posLight = fragPosLightSpace.xyz / fragPosLightSpace.w;
  vec2 shadowCoord = clamp(posLight.xy * 0.5 + 0.5, vec2(0.0), vec2(1.0));
  float depthSM = texture2D(uShadowMap, shadowCoord).x;
  float depth = posLight.z;
  depth = (depth * 0.5 + 0.5);
  return step(0.0, depthSM - depth + bias);
}

void LocalBasis(vec3 n, out vec3 b1, out vec3 b2) {
  float sign_ = sign(n.z);
  if (n.z == 0.0) {
    sign_ = 1.0;
  }
  float a = -1.0 / (sign_ + n.z);
  float b = n.x * n.y * a;
  b1 = vec3(1.0 + sign_ * n.x * n.x * a, sign_ * b, -sign_ * n.x);
  b2 = vec3(b, sign_ + n.y * n.y * a, -n.y);
}

#ifdef HAS_NORMAL_MAP
vec3 ApplyTangentNormalMap() {
  vec3 t, b;
  LocalBasis(vNormalWorld, t, b);
  vec3 nt = texture2D(uNormalTexture, vTextureCoord).xyz * 2.0 - 1.0;
  nt = normalize(nt.x * t + nt.y * b + nt.z * vNormalWorld);
  return nt;
}
#endif

void main(void) {
#ifdef HAS_DIFFUSE_MAP
  vec3 kd = texture2D(udiffuseMap, vTextureCoord).rgb;
#else
  vec3 kd = uKd;
#endif
  outColor = vec4(kd, 1.0);

  outDepth = vec4(vec3(vDepth), 1.0);

#ifdef HAS_NORMAL_MAP
  outNormal = ApplyTangentNormalMap();
#else
  outNormal = vNormalWorld;
#endif

  outVRM = vec3(SimpleShadowMap(vPosWorld.xyz, 1e-3), uRoughness, uMetallic);
  outWorldPos = vPosWorld.xyz;
}
