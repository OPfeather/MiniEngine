out vec4 FragColor;

in VS_OUT {
    vec3 FragPos;
    vec3 Normal;
    vec2 TexCoords;
    vec4 FragPosLightSpace;
} fs_in;

#ifdef HAS_DIFFUSE_MAP
  uniform sampler2D diffuseTexture;
#else
  uniform vec3 Kd;
#endif

uniform sampler2D shadowMap;

uniform vec3 lightPos;//定向光
uniform vec3 viewPos;

uniform vec3 uLightIntensity;

// Shadow map related variables
#define NUM_SAMPLES 100
#define BLOCKER_SEARCH_NUM_SAMPLES NUM_SAMPLES
#define PCF_NUM_SAMPLES NUM_SAMPLES
#define NUM_RINGS 10

//Edit Start
#define SHADOW_MAP_SIZE 4096. //shadow map一条边的长度（假设为正方形）
#define FILTER_RADIUS 3.
#define FRUSTUM_SIZE 100.   //视锥体一个面一条边的长度（假设为正方形）
#define NEAR_PLANE 1        //光源所用透视矩阵的近平面数据
#define LIGHT_WORLD_SIZE 1.  //光源在世界空间的大小
#define LIGHT_SIZE_UV LIGHT_WORLD_SIZE / FRUSTUM_SIZE
//Edit End

//#define EPS 1e-4
#define EPS 0
#define PI 3.141592653589793
#define PI2 6.283185307179586

#define LIGHT_WIDTH 10.0
#define CAMERA_WIDTH 240.0


float rand_1to1(highp float x ) { 
  // -1 -1
  return fract(sin(x)*10000.0);
}

highp float rand_2to1(vec2 uv ) { 
  // 0 - 1
	const highp float a = 12.9898, b = 78.233, c = 43758.5453;
	highp float dt = dot( uv.xy, vec2( a,b ) ), sn = mod( dt, PI );
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
  vec3 normal = normalize(fs_in.Normal);
  vec3 lightDir = normalize(lightPos - fs_in.FragPos);
  float fragSize = (1. + ceil(filterRadiusUV)) * (FRUSTUM_SIZE / SHADOW_MAP_SIZE / 2.);
  return fragSize * (1.0 - dot(normal, lightDir)) * c;
}

float calcBias() {
  vec3 lightDir = normalize(lightPos - fs_in.FragPos);
  vec3 normal = normalize(fs_in.Normal);
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

  float posZFromLight = fs_in.FragPosLightSpace.z;

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
  float penumbra = (zReceiver - avgBlockerDepth) * LIGHT_SIZE_UV / avgBlockerDepth / 3;//除以3效果好点，否则半径太大，虚化太严重
  float filterRadiusUV = penumbra;

  // STEP 3: filtering
  return PCF(shadowMap, coords, biasC, filterRadiusUV);
}
//Edit End

vec3 blinnPhong(float visibility) {
  
#ifdef HAS_DIFFUSE_MAP
  vec3 color = texture2D(diffuseTexture, fs_in.TexCoords).rgb;
#else
  vec3 color =  Kd;
#endif
  color = pow(color, vec3(2.2));

  vec3 ambient = 0.0001 * color;

  vec3 lightDir = normalize(lightPos);
  vec3 normal = normalize(fs_in.Normal);
  float diff = max(dot(lightDir, normal), 0.0);
  vec3 light_atten_coff =
      uLightIntensity / pow(length(lightPos - fs_in.FragPos), 2.0);
  vec3 diffuse = diff * light_atten_coff * color;

  vec3 viewDir = normalize(viewPos - fs_in.FragPos);
  vec3 halfDir = normalize((lightDir + viewDir));
  float spec = pow(max(dot(halfDir, normal), 0.0), 32.0);
  vec3 specular = light_atten_coff * spec;

  vec3 radiance = (ambient + visibility * (diffuse + specular));
  vec3 phongColor = pow(radiance, vec3(1.0 / 2.2));
  return phongColor;
}

void main(void) {
  //Edit Start
  //vPositionFromLight为光源空间下投影的裁剪坐标，除以w结果为NDC坐标
  vec3 shadowCoord = fs_in.FragPosLightSpace.xyz / fs_in.FragPosLightSpace.w;
  //把[-1,1]的NDC坐标转换为[0,1]的坐标
  shadowCoord.xyz = (shadowCoord.xyz + 1.0) / 2.0;

  float visibility = 1.;

  // 无PCF时的Shadow Bias
  float nonePCFBiasC = 0.1;
  // 有PCF时的Shadow Bias
  float pcfBiasC = 0.02;
  // PCF的采样范围，因为是在Shadow Map上采样，需要除以Shadow Map大小，得到uv坐标上的范围
  float filterRadiusUV = FILTER_RADIUS / SHADOW_MAP_SIZE;

  // 硬阴影无PCF，最后参数传0
  //visibility = useShadowMap(shadowMap, vec4(shadowCoord, 1.0), nonePCFBiasC, 0.);
  //visibility = PCF(shadowMap, vec4(shadowCoord, 1.0), pcfBiasC, filterRadiusUV);
  visibility = PCSS(shadowMap, vec4(shadowCoord, 1.0), pcfBiasC);

  vec3 phongColor = blinnPhong(visibility);

  gl_FragColor = vec4(phongColor, 1.0);
  //gl_FragColor = vec4(phongColor, 1.0);
  //Edit End
}