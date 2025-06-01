out float FragColor;

in vec2 vTexCoords;

uniform mat4 uViewMatrix;
uniform mat4 uProjectionMatrix;
uniform sampler2D uTexNoise;
uniform sampler2D uGNormalWorld;
uniform sampler2D uGPosWorld;

uniform float uScreenWidth;
uniform float uScreenHeight;

uniform vec3 uSamples[64];

// parameters (you'd probably want to use them as uniforms to more easily tweak the effect)
int kernelSize = 64;
float radius = 0.5;
float bias = 0.025;

void main()
{
    // tile noise texture over screen based on screen dimensions divided by noise size
    vec2 noiseScale = vec2(uScreenWidth/4.0, uScreenHeight/4.0); 
    // get input for SSAO algorithm
    vec3 worldPos = texture(uGPosWorld, vTexCoords).xyz;
    vec3 viewPos = (uViewMatrix * vec4(worldPos, 1.0)).xyz;
    vec3 worldNormal = texture(uGNormalWorld, vTexCoords).rgb;
    vec3 viewNormal = normalize(mat3(uViewMatrix) * worldNormal);
    vec3 randomVec = normalize(texture(uTexNoise, vTexCoords * noiseScale).xyz);//x:(-1,1)，y:(-1,1), z:0
    // create TBN change-of-basis matrix: from tangent-space to view-space
    //这里应该是随机生成一个切线空间
    vec3 tangent = normalize(randomVec - viewNormal * dot(randomVec, viewNormal));//减去法线方向分量
    vec3 bitangent = cross(viewNormal, tangent);
    mat3 TBN = mat3(tangent, bitangent, viewNormal);
    // iterate over the sample kernel and calculate occlusion factor
    float occlusion = 0.0;
    for(int i = 0; i < kernelSize; ++i)
    {
        // get sample position
        vec3 samplePos = TBN * uSamples[i]; // from tangent to view-space
        samplePos = viewPos + samplePos * radius; 
        
        // project sample position (to sample texture) (to get position on screen/texture)
        vec4 offset = vec4(samplePos, 1.0);
        offset = uProjectionMatrix * offset; // from view to clip-space
        offset.xyz /= offset.w; // perspective divide
        offset.xyz = offset.xyz * 0.5 + 0.5; // transform to range 0.0 - 1.0
        
        // get sample depth
        float sampleDepth = (uViewMatrix * vec4(texture(uGPosWorld, offset.xy).xyz, 1.0)).z; // get depth value of kernel sample
        
        // range check & accumulate
        //光滑地在第一和第二个参数范围内插值了第三个参数,abs(viewPos.z - sampleDepth)越大，说明两点间的距离越远，对遮蔽影响越小，rangeCheck越小
        float rangeCheck = smoothstep(0.0, 1.0, radius / abs(viewPos.z - sampleDepth));
        occlusion += (sampleDepth >= samplePos.z + bias ? 1.0 : 0.0) * rangeCheck;           
    }
    occlusion = 1.0 - (occlusion / kernelSize);
    
    FragColor = occlusion;
}