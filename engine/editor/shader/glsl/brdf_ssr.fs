uniform vec3 uLightDir;//定向光
uniform vec3 uLightPos;
uniform vec3 uCameraPos;
uniform vec3 uLightRadiance;
uniform sampler2D uGDiffuse;
uniform sampler2D uGDepth;
uniform sampler2D uGNormalWorld;
uniform sampler2D uGVRM;
uniform sampler2D uGPosWorld;
uniform sampler2D uBRDFLut;
uniform sampler2D uEavgLut;

in mat4 vWorldToScreen;
in vec2 vTexCoords;

out vec4 FragColor;

#define M_PI 3.1415926535897932384626433832795
#define TWO_PI 6.283185307
#define INV_PI 0.31830988618
#define INV_TWO_PI 0.15915494309

float DistributionGGX(vec3 N, vec3 H, float roughness)
{
   // TODO: To calculate GGX NDF here
   float a2 = roughness * roughness;
    float NdotH = max(dot(N, H), 0.0);
    float NdotH2 = NdotH*NdotH;
 
    float nom = a2;
    float denom = (NdotH2 * (a2 - 1.0) + 1.0);
    denom = M_PI * denom * denom;
 
    return nom / denom;    
}

float GeometrySchlickGGX(float NdotV, float roughness)
{
    // TODO: To calculate Schlick G1 here
    float a = roughness;
    float k = (a * a) / 2.0;
 
    float nom   = NdotV;
    float denom = NdotV * (1.0 - k) + k;
 
    return nom / denom;
}

float GeometrySmith(vec3 N, vec3 V, vec3 L, float roughness)
{
    // TODO: To calculate Smith G here
    float NdotV = max(dot(N, V), 0.0);
    float NdotL = max(dot(N, L), 0.0);
    float ggx2 = GeometrySchlickGGX(NdotV, roughness);
    float ggx1 = GeometrySchlickGGX(NdotL, roughness);
 
    return ggx1 * ggx2;
}

vec3 fresnelSchlick(vec3 F0, vec3 V, vec3 H)
{
    // TODO: To calculate Schlick F here
    return F0 + (1.0 - F0) * pow(1.0 - dot(V, H), 5.0);
}


//https://blog.selfshadow.com/publications/s2017-shading-course/imageworks/s2017_pbs_imageworks_slides_v2.pdf 26页
// r = reflectance    g = edgetint（表示材料边缘的颜色变化）
vec3 AverageFresnel(vec3 r, vec3 g)
{
    return vec3(0.087237) + 0.0230685*g - 0.0864902*g*g + 0.0774594*g*g*g
           + 0.782654*r - 0.136432*r*r + 0.278708*r*r*r
           + 0.19744*g*r + 0.0360605*g*g*r - 0.2586*g*r*r;
}

float Rand1(inout float p) {
  p = fract(p * .1031);
  p *= p + 33.33;
  p *= p + p;
  return fract(p);
}

vec2 Rand2(inout float p) {
  return vec2(Rand1(p), Rand1(p));
}

float InitRand(vec2 uv) {
	vec3 p3  = fract(vec3(uv.xyx) * .1031);
  p3 += dot(p3, p3.yzx + 33.33);
  return fract((p3.x + p3.y) * p3.z);
}

vec3 SampleHemisphereUniform(inout float s, out float pdf) {
  vec2 uv = Rand2(s);
  float z = uv.x;
  float phi = uv.y * TWO_PI;
  float sinTheta = sqrt(1.0 - z*z);
  vec3 dir = vec3(sinTheta * cos(phi), sinTheta * sin(phi), z);
  pdf = INV_TWO_PI;
  return dir;
}

vec3 SampleHemisphereCos(inout float s, out float pdf) {
  vec2 uv = Rand2(s);
  float z = sqrt(1.0 - uv.x);
  float phi = uv.y * TWO_PI;
  float sinTheta = sqrt(uv.x);
  vec3 dir = vec3(sinTheta * cos(phi), sinTheta * sin(phi), z);
  pdf = z * INV_PI;
  return dir;
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

vec4 Project(vec4 a) {
  return a / a.w;
}

float GetDepth(vec3 posWorld) {
  float depth = (vWorldToScreen * vec4(posWorld, 1.0)).w;
  return depth;
}

/*
 * Transform point from world space to screen space([0, 1] x [0, 1])
 *
 */
vec2 GetScreenCoordinate(vec3 posWorld) {
  vec2 uv = Project(vWorldToScreen * vec4(posWorld, 1.0)).xy * 0.5 + 0.5;
  return uv;
}

float GetGBufferDepth(vec2 uv) {
  float depth = texture2D(uGDepth, uv).x;
  if (depth < 1e-2) {
    depth = 1000.0;
  }
  return depth;
}

vec3 GetGBufferNormalWorld(vec2 uv) {
  vec3 normal = texture2D(uGNormalWorld, uv).xyz;
  return normal;
}

vec3 GetGBufferPosWorld(vec2 uv) {
  vec3 posWorld = texture2D(uGPosWorld, uv).xyz;
  return posWorld;
}

float GetGBufferuShadow(vec2 uv) {
  float visibility = texture2D(uGVRM, uv).x;
  return visibility;
}

float GetGBufferuRoughness(vec2 uv) {
  float roughness = texture2D(uGVRM, uv).y;
  return roughness;
}

float GetGBufferuMetallic(vec2 uv) {
  float metallic = texture2D(uGVRM, uv).z;
  return metallic;
}

vec3 GetGBufferDiffuse(vec2 uv) {
  vec3 diffuse = texture2D(uGDiffuse, uv).xyz;
  diffuse = pow(diffuse, vec3(2.2));
  return diffuse;
}

vec3 MultiScatterBRDF(float NdotL, float NdotV)
{
  vec3 albedo = texture2D(uGDiffuse, vTexCoords).rgb;
  float roughness = GetGBufferuRoughness(vTexCoords);
  vec3 E_o = texture2D(uBRDFLut, vec2(NdotL, roughness)).xyz;
  vec3 E_i = texture2D(uBRDFLut, vec2(NdotV, roughness)).xyz;//NdotV接近1的地方有亮斑

  vec3 E_avg = texture2D(uEavgLut, vec2(0, roughness)).xyz;
  // copper
  vec3 edgetint = vec3(0.827, 0.792, 0.678);
  vec3 F_avg = AverageFresnel(albedo, edgetint);
  
  // TODO: To calculate fms and missing energy here
  vec3 fms = ( vec3(1.0) - E_o ) * (vec3(1.0) - E_i) / ( M_PI * (vec3(1.0) - E_avg) );//在NdotL或NdotV接近1的地方，有小暗斑
  vec3 fadd = F_avg * E_avg / ( vec3(1.0) - F_avg * ( vec3(1.0) - E_avg ) );
  return fms * fadd;
}

/*
 * Evaluate diffuse bsdf value.
 *
 * wi, wo are all in world space.
 * uv is in screen space, [0, 1] x [0, 1].
 *
 */
vec3 EvalDiffuse(vec3 wi, vec3 wo, vec2 uv) {
  vec3 albedo  = GetGBufferDiffuse(uv);
  vec3 normal = GetGBufferNormalWorld(uv);
  float cos = max(0., dot(normal, wi));
  return albedo * cos * INV_PI;
}


/*
 * Evaluate directional light with shadow map
 * uv is in screen space, [0, 1] x [0, 1].
 *
 */
vec3 EvalDirectionalLight(vec2 uv) {
  vec3 Le = GetGBufferuShadow(uv) * uLightRadiance ;
  return Le;
}

bool RayMarch(vec3 ori, vec3 dir, out vec3 hitPos) {
  float step = 0.05;
  const int totalStepTimes = 500; 
  int curStepTimes = 0;

  vec3 stepDir = normalize(dir) * step;
  vec3 curPos = ori;
  curPos += stepDir;
  for(int curStepTimes = 0; curStepTimes < totalStepTimes; curStepTimes++)
  {
    vec2 screenUV = GetScreenCoordinate(curPos);
    if(screenUV.x > 1.0f || screenUV.y > 1.0f || screenUV.x < 0.0f || screenUV.y < 0.0f) return false;
    float rayDepth = GetDepth(curPos);
    float gBufferDepth = GetGBufferDepth(screenUV);

    if(rayDepth - gBufferDepth > 0.0001){
      hitPos = curPos;
      return true;
    }

    curPos += stepDir;
  }

  return false;
}

vec3 EvalReflect(vec3 wi, vec3 wo, vec2 uv) {
  vec3 worldNormal = GetGBufferNormalWorld(uv);
  vec3 worldPos = texture2D(uGPosWorld, uv).xyz;
  vec3 relfectDir = normalize(reflect(-wo, worldNormal));
  vec3 hitPos;
  if(RayMarch(worldPos, relfectDir, hitPos)){
      vec2 screenUV = GetScreenCoordinate(hitPos);
      return GetGBufferDiffuse(screenUV);
  }
  else{
    return vec3(0.); 
  }
}

// ----------------------------------------------------------------------------
// http://holger.dammertz.org/stuff/notes_HammersleyOnHemisphere.html
// efficient VanDerCorpus calculation.
float RadicalInverse_VdC(uint bits) 
{
     bits = (bits << 16u) | (bits >> 16u);
     bits = ((bits & 0x55555555u) << 1u) | ((bits & 0xAAAAAAAAu) >> 1u);
     bits = ((bits & 0x33333333u) << 2u) | ((bits & 0xCCCCCCCCu) >> 2u);
     bits = ((bits & 0x0F0F0F0Fu) << 4u) | ((bits & 0xF0F0F0F0u) >> 4u);
     bits = ((bits & 0x00FF00FFu) << 8u) | ((bits & 0xFF00FF00u) >> 8u);
     return float(bits) * 2.3283064365386963e-10; // / 0x100000000
}
// ----------------------------------------------------------------------------
vec2 Hammersley(uint i, uint N)
{
	return vec2(float(i)/float(N), RadicalInverse_VdC(i));
}
// ----------------------------------------------------------------------------
vec3 ImportanceSampleGGX(vec2 Xi, vec3 N, float roughness)
{
	float a = roughness*roughness;
	
	float phi = 2.0 * M_PI * Xi.x;
	float cosTheta = sqrt((1.0 - Xi.y) / (1.0 + (a*a - 1.0) * Xi.y));
	float sinTheta = sqrt(1.0 - cosTheta*cosTheta);
	
	// from spherical coordinates to cartesian coordinates - halfway vector
	vec3 H;
	H.x = cos(phi) * sinTheta;
	H.y = sin(phi) * sinTheta;
	H.z = cosTheta;
	
	// from tangent-space H vector to world-space sample vector
	vec3 up          = abs(N.z) < 0.999 ? vec3(0.0, 0.0, 1.0) : vec3(1.0, 0.0, 0.0);
	vec3 tangent   = normalize(cross(up, N));
	vec3 bitangent = cross(N, tangent);
	
	vec3 sampleVec = tangent * H.x + bitangent * H.y + N * H.z;
	return normalize(sampleVec);
}

vec3 EvalBRDF(vec3 wi, vec3 wo, vec2 uv) {
  vec3 F0 = vec3(0.04);
  vec3 albedo = GetGBufferDiffuse(uv).xyz;
  float metallic = GetGBufferuMetallic(uv);
  F0 = mix(F0, albedo, metallic);

  vec3 L = normalize(wi);
  vec3 V = normalize(wo);
  vec3 N = GetGBufferNormalWorld(uv).xyz;
  vec3 H = normalize(V + L);
  float NdotV = max(dot(N, V), 0.0);

  //vec3 worldPos = texture2D(uGPosWorld, uv).xyz;
  //float distance = length(uLightPos - worldPos);
  //float attenuation = 1.0 / (distance * distance);
  //vec3 radiance = uLightRadiance;

  float roughness = GetGBufferuRoughness(uv);
  float NDF = DistributionGGX(N, H, roughness);   
  float G   = GeometrySmith(N, V, L, roughness);
  vec3 F = fresnelSchlick(F0, V, H);

  vec3 numerator    = NDF * G * F; 
  float denominator = 4.0 * max(dot(N, V), 0.0) * max(dot(N, L), 0.0);
  vec3 Fmicro = numerator / max(denominator, 0.001); 

  float NdotL = max(dot(N, L), 0.0);        

  vec3 Fms = MultiScatterBRDF(NdotL, NdotV);
  vec3 BRDF = Fmicro + Fms;
  return BRDF * NdotL;
    return F0; 
}

#define SAMPLE_NUM 10

void main() {
  float s = InitRand(gl_FragCoord.xy);
  float epsilon = 1e-5f;

  vec3 Lo = vec3(0.0);
  vec3 worldPos = texture2D(uGPosWorld, vTexCoords).xyz;
  vec2 screenUV = GetScreenCoordinate(worldPos);
  vec3 wi = normalize(uLightDir);
  vec3 wo = normalize(uCameraPos - worldPos);
  vec3 normal = GetGBufferNormalWorld(vTexCoords);
  float roughness = GetGBufferuRoughness(vTexCoords);

  //L = GetGBufferDiffuse(vTexCoords);

  vec3 BRDF = EvalBRDF(wi, wo, screenUV);

  // 直接光照
  Lo = BRDF * uLightRadiance * EvalDirectionalLight(screenUV);
  
  // Screen Space Ray Tracing 的反射测试
  //Lo = EvalReflect(wi, wo, screenUV);
  //L = vec3(texture(uGPosWorld, screenUV).xyz);

  vec3 L_ind = vec3(0.0);
  for(int i = 0; i < SAMPLE_NUM; i++){
    //漫反射材质
    // float pdf;
    // vec3 localDir = SampleHemisphereCos(s, pdf);
    // vec3 normal = GetGBufferNormalWorld(screenUV);
    // vec3 b1, b2;
    // LocalBasis(normal, b1, b2);
    // vec3 dir = normalize(mat3(b1, b2, normal) * localDir);
    // vec3 position_1;
    // if(RayMarch(worldPos, dir, position_1)){
    //     vec2 hitScreenUV = GetScreenCoordinate(position_1);
    //     L_ind += EvalDiffuse(dir, wo, screenUV) / pdf * EvalDiffuse(wi, dir, hitScreenUV) * EvalDirectionalLight(hitScreenUV);
    // }

    vec3 N = normal;
    vec3 V = wo;
    vec2 Xi = Hammersley(uint(i), uint(SAMPLE_NUM));
    vec3 H = ImportanceSampleGGX(Xi, N, roughness);
    vec3 L  = normalize(2.0 * dot(V, H) * H - V);
    
    float NdotL = max(dot(N, L), 0.0);
    if(NdotL > 0.0)
    {
        // sample from the environment's mip level based on roughness/pdf
        float D   = DistributionGGX(N, H, roughness);
        float NdotH = max(dot(N, H), 0.0);
        float HdotV = max(dot(H, V), 0.0);
        float pdf = D * NdotH / (4.0 * HdotV) + 0.0001; 

        vec3 position_1;
        if(RayMarch(worldPos, L, position_1)){
            vec2 hitScreenUV = GetScreenCoordinate(position_1);
            L_ind += EvalBRDF(L, wo, screenUV) / pdf * EvalBRDF(wi, -L, hitScreenUV) * EvalDirectionalLight(hitScreenUV);
        }
    }
  }

    L_ind /= float(SAMPLE_NUM);

    vec3 color = Lo + L_ind;

  //vec3 color = pow(clamp(L, vec3(0.0), vec3(1.0)), vec3(1.0 / 2.2));
  FragColor = vec4(color, 1.0);
}
