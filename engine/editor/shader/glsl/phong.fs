out vec4 FragColor;

in vec4 vFragPosLightSpace;
in vec2 vTextureCoord;
in vec4 vPosWorld;
in vec3 vNormalWorld;

#ifdef HAS_DIFFUSE_MAP
  uniform sampler2D uDiffuseMap;
#else
  uniform vec3 uKd;
#endif //HAS_DIFFUSE_MAP

#ifdef HAS_SPECULAR_MAP
  uniform sampler2D uSpecularMap;
#else
  uniform vec3 uKs;
#endif //HAS_SPECULAR_MAP

uniform sampler2D uShadowMap;
uniform vec3 uLightPos;
uniform vec3 uCameraPos;

#ifdef AREA_LIGHT
// Shadow map related variables
#define NUM_SAMPLES 100
#define BLOCKER_SEARCH_NUM_SAMPLES NUM_SAMPLES
#define PCF_NUM_SAMPLES NUM_SAMPLES
#define NUM_RINGS 10

//Edit Start
#define SHADOW_MAP_SIZE 1024. //shadow map一条边的长度（假设为正方形）
#define FRUSTUM_SIZE 100.   //视锥体一个面一条边的长度（假设为正方形）
#define NEAR_PLANE 1        //光源所用透视矩阵的近平面数据
#define LIGHT_WORLD_SIZE 2.  //光源在世界空间的大小,灯越大，找到遮挡物范围就越大，虚化越严重
#define LIGHT_SIZE_UV LIGHT_WORLD_SIZE / FRUSTUM_SIZE
//Edit End

//#define EPS 1e-4
#define EPS 0
#define PI 3.141592653589793
#define PI2 6.283185307179586

float rand_1to1(float x ) { 
  // -1 -1
  return fract(sin(x)*10000.0);
}

float rand_2to1(vec2 uv ) { 
  // 0 - 1
	const float a = 12.9898, b = 78.233, c = 43758.5453;
	float dt = dot( uv.xy, vec2( a,b ) ), sn = mod( dt, PI );
	return fract(sin(sn) * c);
}

// float unpack(vec4 rgbaDepth) {
//     const vec4 bitShift = vec4(1.0, 1.0/256.0, 1.0/(256.0*256.0), 1.0/(256.0*256.0*256.0));
//     return dot(rgbaDepth, bitShift);
// }

vec2 poissonDisk[NUM_SAMPLES];

void poissonDiskSamples( const in vec2 randomSeed ) {

  float ANGLE_STEP = PI2 * float( NUM_RINGS ) / float( NUM_SAMPLES );
  float INV_NUM_SAMPLES = 1.0 / float( NUM_SAMPLES );

  float angle = rand_2to1( randomSeed ) * PI2;
  float radius = INV_NUM_SAMPLES;
  float radiusStep = radius;

  for( int i = 0; i < NUM_SAMPLES; i ++ ) {
    poissonDisk[i] = vec2( cos( angle ), sin( angle ) ) * pow( radius, 0.75 );
    radius += radiusStep;
    angle += ANGLE_STEP;
  }
}

void uniformDiskSamples( const in vec2 randomSeed ) {

  float randNum = rand_2to1(randomSeed);
  float sampleX = rand_1to1( randNum ) ;
  float sampleY = rand_1to1( sampleX ) ;

  float angle = sampleX * PI2;
  float radius = sqrt(sampleY);

  for( int i = 0; i < NUM_SAMPLES; i ++ ) {
    poissonDisk[i] = vec2( radius * cos(angle) , radius * sin(angle)  );

    sampleX = rand_1to1( sampleY ) ;
    sampleY = rand_1to1( sampleX ) ;

    angle = sampleX * PI2;
    radius = sqrt(sampleY);
  }
}

//Edit Start
//自适应Shadow Bias算法 https://zhuanlan.zhihu.com/p/370951892
//c是控制参数
float getShadowBias(float c, float filterRadiusUV){
  vec3 normal = normalize(vNormalWorld);
  vec3 lightDir = normalize(uLightPos);//定向光
  float fragSize = (1. + ceil(filterRadiusUV)) * (FRUSTUM_SIZE / SHADOW_MAP_SIZE / 2.);
  return fragSize * (1.0 - dot(normal, lightDir)) * c;
}

float calcBias() {
  vec3 lightDir = normalize(uLightPos);//定向光
  vec3 normal = normalize(vNormalWorld);
  float c = 0.001;
  float bias = max(c * (1.0 - dot(normal, lightDir)), c);//相当于加固定偏移c = 0.001
  return bias;
}
//Edit End

//Edit Start
float useShadowMap(sampler2D shadowMap, vec4 shadowCoord, float biasC, float filterRadiusUV){
  //float depth = unpack(texture2D(shadowMap, shadowCoord.xy));
  float depth = texture2D(shadowMap, shadowCoord.xy).r;
  float cur_depth = shadowCoord.z;
  //float bias = getShadowBias(biasC, filterRadiusUV);
  float bias = calcBias();
  if(cur_depth - bias >= depth + EPS){
    return 0.;
  }
  else{
    return 1.0;
  }
}
//Edit End

//Edit Start
float PCF(sampler2D shadowMap, vec4 coords, float biasC, float filterRadiusUV) {
  //uniformDiskSamples(coords.xy);
  poissonDiskSamples(coords.xy); //使用xy坐标作为随机种子生成
  float visibility = 0.0;
  for(int i = 0; i < NUM_SAMPLES; i++){
    vec2 offset = poissonDisk[i] * filterRadiusUV;
    visibility += useShadowMap(shadowMap, coords + vec4(offset, 0., 0.), biasC, filterRadiusUV);
  }
  return visibility / float(NUM_SAMPLES);
}
//Edit End

//Edit Start
float findBlocker(sampler2D shadowMap, vec2 uv, float zReceiver) {
  int blockerNum = 0;
  float blockDepth = 0.;

  float posZFromLight = vFragPosLightSpace.z;

  float searchRadius = LIGHT_SIZE_UV * (posZFromLight - NEAR_PLANE) / posZFromLight;

  poissonDiskSamples(uv);
  for(int i = 0; i < NUM_SAMPLES; i++){
    //float shadowDepth = unpack(texture2D(shadowMap, uv + poissonDisk[i] * searchRadius));
    float shadowDepth = texture2D(shadowMap, uv + poissonDisk[i] * searchRadius).r;
    if(zReceiver - calcBias() > shadowDepth + EPS){
      blockerNum++;
      blockDepth += shadowDepth;
    }
  }

  if(blockerNum < EPS)
    return -1.;
  else
    return blockDepth / float(blockerNum);
}
//Edit End

//Edit Start
float PCSS(sampler2D shadowMap, vec4 coords, float biasC){
  float zReceiver = coords.z;

  // STEP 1: avgblocker depth 
  float avgBlockerDepth = findBlocker(shadowMap, coords.xy, zReceiver);

  if(avgBlockerDepth < EPS)
    return 1.0;

  // STEP 2: penumbra size
  float penumbra = (zReceiver - avgBlockerDepth) * LIGHT_SIZE_UV / avgBlockerDepth / 10;//除以20效果好点，否则半径太大，虚化太严重
  float filterRadiusUV = penumbra;

  // STEP 3: filtering
  return PCF(shadowMap, coords, biasC, filterRadiusUV);
}
//Edit End

#else
float SimpleShadowMap(float bias){
  vec3 posLight = vFragPosLightSpace.xyz / vFragPosLightSpace.w;
  vec2 shadowCoord = clamp(posLight.xy * 0.5 + 0.5, vec2(0.0), vec2(1.0));
  float depthSM = texture2D(uShadowMap, shadowCoord).x;
  float depth = posLight.z;
  depth = (depth * 0.5 + 0.5);
  return step(0.0, depthSM - depth + bias);
}
#endif //AREA_LIGHT

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



void main(void) {
   // light
  vec3 ambientLightColor = vec3(0.2f, 0.2f, 0.2f);
  
#ifdef HAS_DIFFUSE_MAP
  vec3 kd = texture2D(uDiffuseMap, vTextureCoord).rgb;
#else
  vec3 kd = uKd;
#endif
  // ambient
  vec3 ambient = ambientLightColor * kd;

  // diffuse
  vec3 diffuseLightColor = vec3(0.5f, 0.5f, 0.5f);

  vec3 norm = normalize(vNormalWorld);

#ifdef POINT_LIGHT
  vec3 lightDir = normalize(uLightPos - vPosWorld.xyz);
#else
  vec3 lightDir = normalize(uLightPos);
#endif
  
  float diff = max(dot(norm, lightDir), 0.0);
  vec3 diffuse = diff * diffuseLightColor * kd;

  // specular
  vec3 specularLightColor = vec3(1.0f, 1.0f, 1.0f);
  vec3 viewDir = normalize(uCameraPos - vPosWorld.xyz);
  vec3 reflectDir = reflect(-lightDir, norm);
  float spec = pow(max(dot(viewDir, reflectDir), 0.0), 16);

#ifdef HAS_SPECULAR_MAP
  vec3 ks = texture2D(uSpecularMap, vTextureCoord).rgb;
#else
  vec3 ks = uKs;
#endif //HAS_SPECULAR_MAP
  vec3 specular = spec * specularLightColor * ks;

  // shadow
  float visibility = 1.;
#ifdef AREA_LIGHT
  vec3 shadowCoord = vFragPosLightSpace.xyz / vFragPosLightSpace.w;
  //把[-1,1]的NDC坐标转换为[0,1]的坐标
  shadowCoord.xyz = (shadowCoord.xyz + 1.0) / 2.0;

  // 有PCF时的Shadow Bias
  float pcfBiasC = 0.02;

  visibility = PCSS(uShadowMap, vec4(shadowCoord, 1.0), pcfBiasC);
#else
  visibility = SimpleShadowMap(1e-3);
#endif //AREA_LIGHT

  vec3 result = ambient + (diffuse + specular) * visibility;
  FragColor = vec4(result, 1.0);

}