in  vec2 vTexCoords;
out vec4 FragColor;

uniform sampler2D uCurrentColor;
uniform sampler2D uPreviousColor;
uniform sampler2D uVelocityMap;
uniform float uScreenWidth;
uniform float uScreenHeight;
uniform int uFrameCount;

#ifdef DENOISE
uniform sampler2D uGNormalWorld;
uniform sampler2D uGPosWorld;

#define SIGMA_POSITION 32.0
#define SIGMA_NORMAL 0.1
#define SIGMA_COLOR 0.6
#define SIGMA_PLANE 0.1
#endif //DENOISE

#ifdef TAA
uniform sampler2D uCurrentDepth;
#endif //TAA

vec3 RGB2YCoCgR(vec3 rgbColor)
{
    vec3 YCoCgRColor;

    YCoCgRColor.y = rgbColor.r - rgbColor.b;
    float temp = rgbColor.b + YCoCgRColor.y / 2;
    YCoCgRColor.z = rgbColor.g - temp;
    YCoCgRColor.x = temp + YCoCgRColor.z / 2;

    return YCoCgRColor;
}

vec3 YCoCgR2RGB(vec3 YCoCgRColor)
{
    vec3 rgbColor;

    float temp = YCoCgRColor.x - YCoCgRColor.z / 2;
    rgbColor.g = YCoCgRColor.z + temp;
    rgbColor.b = temp - YCoCgRColor.y / 2;
    rgbColor.r = rgbColor.b + YCoCgRColor.y;

    return rgbColor;
}

float Luminance(vec3 color)
{
    return 0.25 * color.r + 0.5 * color.g + 0.25 * color.b;
}

vec3 ToneMap(vec3 color)
{
    return color / (1 + Luminance(color));
}

vec3 UnToneMap(vec3 color)
{
    return color / (1 - Luminance(color));
}

vec3 clipAABB(vec3 nowColor, vec3 preColor)
{
    vec3 aabbMin = nowColor, aabbMax = nowColor;
    vec2 deltaRes = vec2(1.0 / uScreenWidth, 1.0 / uScreenHeight);
    vec3 m1 = vec3(0), m2 = vec3(0);

    for(int i=-1;i<=1;++i)
    {
        for(int j=-1;j<=1;++j)
        {
            vec2 newUV = vTexCoords + deltaRes * vec2(i, j);
            vec3 C = RGB2YCoCgR(ToneMap(texture(uCurrentColor, newUV).rgb));
            //vec3 C = texture(uCurrentColor, newUV).rgb;
            m1 += C;
            m2 += C * C;
        }
    }

    // Variance clip
    const int N = 9;
    const float VarianceClipGamma = 1.0f;
    vec3 mu = m1 / N;
    vec3 sigma = sqrt(abs(m2 / N - mu * mu));
    aabbMin = mu - VarianceClipGamma * sigma;
    aabbMax = mu + VarianceClipGamma * sigma;

    // clip to center
    vec3 p_clip = 0.5 * (aabbMax + aabbMin);
    vec3 e_clip = 0.5 * (aabbMax - aabbMin);

    vec3 v_clip = preColor - p_clip;
    vec3 v_unit = v_clip.xyz / e_clip;
    vec3 a_unit = abs(v_unit);
    float ma_unit = max(a_unit.x, max(a_unit.y, a_unit.z));

    if (ma_unit > 1.0)
        return p_clip + v_clip / ma_unit;
    else
        return preColor;
}

#ifdef DENOISE
vec3 JointBilateralFilter()
{
    float epsilon = 1e-5f;
    vec2 deltaRes = vec2(1.0 / uScreenWidth, 1.0 / uScreenHeight);
    vec3 center_postion  = texture2D(uGPosWorld, vTexCoords).xyz;
    vec3 center_color = texture2D(uCurrentColor, vTexCoords).xyz;
    vec3 center_normal  = normalize(texture2D(uGNormalWorld, vTexCoords).xyz);

    float total_weight = 0;
    vec3 final_color = vec3(0);

    for(int i=-3;i<=3;++i)
    {
        for(int j=-3;j<=3;++j)
        {
            vec2 newUV = vTexCoords + deltaRes * vec2(i, j);
            newUV = clamp(newUV, vec2(0.0), vec2(1.0));

            vec3 postion  = texture2D(uGPosWorld, newUV).xyz;
            vec3 color = texture2D(uCurrentColor, newUV).xyz;
            vec3 normal  = normalize(texture2D(uGNormalWorld, newUV).xyz);

            float position_distance = distance(center_postion, postion);
            float d_position = position_distance * position_distance / (2 * SIGMA_POSITION * SIGMA_POSITION);

            float color_distance = distance(center_color, color);
            float d_color = color_distance * color_distance / (2 * SIGMA_COLOR * SIGMA_COLOR);
            
            float normal_angle = acos(clamp(dot(center_normal, normal), -1.0, 1.0));
            float d_normal = normal_angle * normal_angle / (2 * SIGMA_NORMAL * SIGMA_NORMAL);

            float plane_distance = .0f;
            if(position_distance > epsilon)
            {
                plane_distance = dot(center_normal, ((postion - center_postion) / position_distance));
            }
            float d_plane = plane_distance * plane_distance / (2 * SIGMA_PLANE * SIGMA_PLANE);

            float weight = exp(-d_position - d_color - d_normal - d_plane);
            total_weight += weight;
            final_color += color * weight;
        }
    }
    return final_color/total_weight;
}
vec3 Reprojection()
{
    vec3 nowColor = JointBilateralFilter();
    if(uFrameCount == 0)
    {
        return nowColor;
    }

    vec2 velocity = texture(uVelocityMap, vTexCoords).rg;
    vec2 offsetUV = vTexCoords - velocity;
    //上一帧在屏幕外
    if(offsetUV.x < 0 || offsetUV.x > 1 || offsetUV.y < 0 || offsetUV.y > 1)
    {
        return nowColor;
    }
    vec3 preColor = texture(uPreviousColor, offsetUV).rgb;

    nowColor = RGB2YCoCgR(ToneMap(nowColor));
    preColor = RGB2YCoCgR(ToneMap(preColor));

    preColor = clipAABB(nowColor, preColor);

    preColor = UnToneMap(YCoCgR2RGB(preColor));
    nowColor = UnToneMap(YCoCgR2RGB(nowColor));

    float c = 0.2;
    return c * nowColor + (1-c) * preColor;
}
#endif //DENOISE

#ifdef TAA
vec2 getClosestOffset()
{
    vec2 deltaRes = vec2(1.0 / uScreenWidth, 1.0 / uScreenHeight);
    float closestDepth = 1000.0f;
    vec2 closestUV = vTexCoords;

    for(int i=-1;i<=1;++i)
    {
        for(int j=-1;j<=1;++j)
        {
            vec2 newUV = vTexCoords + deltaRes * vec2(i, j);

            float depth = texture2D(uCurrentDepth, newUV).x;

            if(depth < closestDepth)
            {
                closestDepth = depth;
                closestUV = newUV;
            }
        }
    }

    return closestUV;
}

#endif //TAA

void main()
{
#ifdef TAA
#ifdef DENOISE
    vec3 nowColor = JointBilateralFilter();
#else
    vec3 nowColor = texture(uCurrentColor, vTexCoords).rgb;
#endif //DENOISE
    if(uFrameCount == 0)
    {
        FragColor = vec4(nowColor, 1.0);
        return;
    }

    // 周围3x3内距离最近的速度向量
    vec2 velocity = texture(uVelocityMap, getClosestOffset()).rg;
    vec2 offsetUV = clamp(vTexCoords - velocity, vec2(0.0), vec2(1.0));
    vec3 preColor = texture(uPreviousColor, offsetUV).rgb;

    nowColor = RGB2YCoCgR(ToneMap(nowColor));
    preColor = RGB2YCoCgR(ToneMap(preColor));

    preColor = clipAABB(nowColor, preColor);

    preColor = UnToneMap(YCoCgR2RGB(preColor));
    nowColor = UnToneMap(YCoCgR2RGB(nowColor));

    float c = 0.05;
    FragColor = vec4(c * nowColor + (1-c) * preColor, 1.0);
#else

#ifdef DENOISE
    FragColor = vec4(Reprojection(), 1.0);
#else
    FragColor = vec4(texture(uCurrentColor, vTexCoords).rgb, 1.0);
#endif //DENOISE
    
#endif //TAA
}