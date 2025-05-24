#ifdef DIRECTION_LIGHT
uniform vec3 uLightDir;
#endif

#ifdef AREA_LIGHT
uniform vec3 uLightDir;
uniform vec3 points[4];
uniform sampler2D LTC1; // for inverse M
uniform sampler2D LTC2; // GGX norm, fresnel, 0(unused), sphere

const float LUT_SIZE  = 64.0; // ltc_texture size
const float LUT_SCALE = (LUT_SIZE - 1.0)/LUT_SIZE;
const float LUT_BIAS  = 0.5/LUT_SIZE;
#endif

#ifdef POINT_LIGHT
uniform vec3 uLightPos;
#endif
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

#ifdef AREA_LIGHT
// Vector form without project to the plane (dot with the normal)
// Use for proxy sphere clipping
vec3 IntegrateEdgeVec(vec3 v1, vec3 v2)
{
    // Using built-in acos() function will result flaws
    // Using fitting result for calculating acos()
    float x = dot(v1, v2);
    float y = abs(x);

    float a = 0.8543985 + (0.4965155 + 0.0145206*y)*y;
    float b = 3.4175940 + (4.1616724 + y)*y;
    float v = a / b;

    float theta_sintheta = (x > 0.0) ? v : 0.5*inversesqrt(max(1.0 - x*x, 1e-7)) - v;

    return cross(v1, v2)*theta_sintheta;
}

float IntegrateEdge(vec3 v1, vec3 v2)
{
    return IntegrateEdgeVec(v1, v2).z;
}

// P is fragPos in world space (LTC distribution)
vec3 LTC_Evaluate(vec3 N, vec3 V, vec3 P, mat3 Minv, vec3 points[4], bool twoSided)
{
    // construct orthonormal basis around N
    vec3 T1, T2;
    T1 = normalize(V - N * dot(V, N));
    T2 = cross(N, T1);

    // rotate area light in (T1, T2, N) basis
    Minv = Minv * transpose(mat3(T1, T2, N));

    // polygon (allocate 4 vertices for clipping)
    vec3 L[4];
    // transform polygon from LTC back to origin Do (cosine weighted)
    L[0] = Minv * (points[0] - P);
    L[1] = Minv * (points[1] - P);
    L[2] = Minv * (points[2] - P);
    L[3] = Minv * (points[3] - P);

    // use tabulated horizon-clipped sphere
    // check if the shading point is behind the light
    vec3 dir = points[0] - P; // LTC space
    vec3 lightNormal = cross(points[1] - points[0], points[3] - points[0]);
    bool behind = (dot(dir, lightNormal) < 0.0);

    // cos weighted space
    L[0] = normalize(L[0]);
    L[1] = normalize(L[1]);
    L[2] = normalize(L[2]);
    L[3] = normalize(L[3]);

    // integrate
    vec3 vsum = vec3(0.0);
    vsum += IntegrateEdgeVec(L[0], L[1]);
    vsum += IntegrateEdgeVec(L[1], L[2]);
    vsum += IntegrateEdgeVec(L[2], L[3]);
    vsum += IntegrateEdgeVec(L[3], L[0]);

    // form factor of the polygon in direction vsum
    float len = length(vsum);

    float z = vsum.z/len;
    if (behind)
        z = -z;

    vec2 uv = vec2(z*0.5f + 0.5f, len); // range [0, 1]
    uv = uv*LUT_SCALE + LUT_BIAS;

    // Fetch the form factor for horizon clipping
    float scale = texture(LTC2, uv).w;

    float sum = len*scale;
    if (!behind && !twoSided)
        sum = 0.0;

    // Outgoing radiance (solid angle) for the entire polygon
    vec3 Lo_i = vec3(sum, sum, sum);
    return Lo_i;
}

// PBR-maps for roughness (and metallic) are usually stored in non-linear
// color space (sRGB), so we use these functions to convert into linear RGB.
vec3 PowVec3(vec3 v, float p)
{
    return vec3(pow(v.x, p), pow(v.y, p), pow(v.z, p));
}

const float gamma = 2.2;
vec3 ToLinear(vec3 v) { return PowVec3(v, gamma); }
vec3 ToSRGB(vec3 v)   { return PowVec3(v, 1.0/gamma); }
#endif  //AREA_LIGHT

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

vec3 MultiScatterBRDF(float NdotL, float NdotV, vec2 uv)
{
  vec3 albedo = texture2D(uGDiffuse, uv).rgb;
  float roughness = GetGBufferuRoughness(uv);
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
  vec3 radiance = uLightRadiance;
#ifdef POINT_LIGHT
  vec3 worldPos = texture2D(uGPosWorld, uv).xyz;
  float distance = length(uLightPos - worldPos);
  float attenuation = 1.0 / (1.0 + 0.09 * distance + 0.032 *distance * distance);
  radiance = radiance * attenuation;
#endif
  vec3 Le = GetGBufferuShadow(uv) * radiance ;
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

#ifdef AREA_LIGHT
vec3 EvalBRDF(vec3 wi, vec3 wo, vec2 uv){
  vec3 albedo = GetGBufferDiffuse(uv).xyz;
  vec3 F0 = vec3(0.04);
  float metallic = GetGBufferuMetallic(uv);
  F0 = mix(F0, albedo, metallic);
  
  vec3 result = vec3(0.0f);

  vec3 N = GetGBufferNormalWorld(uv);
  vec3 V = normalize(wo);
  vec3 P = texture2D(uGPosWorld, uv).xyz;
  float dotNV = clamp(dot(N, V), 0.0f, 1.0f);
  float roughness = GetGBufferuRoughness(uv);

  vec2 ltcUV = vec2(roughness, sqrt(1.0f - dotNV));
  ltcUV = ltcUV*LUT_SCALE + LUT_BIAS;

  // get 4 parameters for inverse_M
  vec4 t1 = texture(LTC1, ltcUV);

  // Get 2 parameters for Fresnel calculation
  vec4 t2 = texture(LTC2, ltcUV);

  mat3 Minv = mat3(
      vec3(t1.x, 0, t1.y),
      vec3(  0,  1,    0),
      vec3(t1.z, 0, t1.w)
  );

  // Evaluate LTC shading
  vec3 diffuse = LTC_Evaluate(N, V, P, mat3(1), points, false);
  vec3 specular = LTC_Evaluate(N, V, P, Minv, points, false);

  // GGX BRDF shadowing and Fresnel
  // t2.x: shadowedF90 (F90 normally it should be 1.0)
  // t2.y: Smith function for Geometric Attenuation Term, it is dot(V or L, H).
  specular *= F0*t2.x + (1.0f - F0) * t2.y;

  result = specular + diffuse * albedo;
  return result;
}

#else
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

  float roughness = GetGBufferuRoughness(uv);
  float NDF = DistributionGGX(N, H, roughness);   
  float G   = GeometrySmith(N, V, L, roughness);
  vec3 F = fresnelSchlick(F0, V, H);

  vec3 numerator    = NDF * G * F; 
  float denominator = 4.0 * max(dot(N, V), 0.0) * max(dot(N, L), 0.0);
  vec3 Fmicro = numerator / max(denominator, 0.001); 

  float NdotL = max(dot(N, L), 0.0);        

  vec3 Fms = MultiScatterBRDF(NdotL, NdotV, uv);
  vec3 BRDF = Fmicro + Fms;
  return BRDF * NdotL;
}
#endif //AREA_LIGHT

#define SAMPLE_NUM 1

void main() {
  float s = InitRand(gl_FragCoord.xy);
  float epsilon = 1e-5f;

  vec3 Lo = vec3(0.0);
  vec3 worldPos = texture2D(uGPosWorld, vTexCoords).xyz;
  vec2 screenUV = GetScreenCoordinate(worldPos);
  vec3 wo = normalize(uCameraPos - worldPos);
  vec3 normal = GetGBufferNormalWorld(vTexCoords);
  float roughness = GetGBufferuRoughness(vTexCoords);

#ifdef AREA_LIGHT
  vec3 wi = normalize(uLightDir);
#else

#ifdef DIRECTION_LIGHT
  vec3 wi = normalize(uLightDir);
#endif //DIRECTION_LIGHT

#ifdef POINT_LIGHT
  vec3 wi = normalize(uLightPos - worldPos);
#endif //POINT_LIGHT
#endif //AREA_LIGHT
  vec3 BRDF = EvalBRDF(wi, wo, screenUV);
  // 直接光照
  Lo = BRDF * EvalDirectionalLight(screenUV);


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
    color = color / (color + vec3(1.0));

  //color = pow(clamp(color, vec3(0.0), vec3(1.0)), vec3(1.0 / 2.2));
  FragColor = vec4(color, 1.0);
}
