// This shader performs downsampling on a texture,
// as taken from Call Of Duty method, presented at ACM Siggraph 2014.
// This particular method was customly designed to eliminate
// "pulsating artifacts and temporal stability issues".

// Remember to add bilinear minification filter for this texture!
// Remember to use a floating-point texture format (for HDR)!
// Remember to use edge clamping for this texture!
uniform sampler2D uSrcTexture;
uniform float uScreenWidth;
uniform float uScreenHeight;
// which mip we are writing to, used for Karis average
uniform int uMipLevel = 1;

in vec2 vTexCoord;
layout (location = 0) out vec3 downsample;

vec3 PowVec3(vec3 v, float p)
{
    return vec3(pow(v.x, p), pow(v.y, p), pow(v.z, p));
}

const float invGamma = 1.0 / 2.2;
vec3 ToSRGB(vec3 v)   { return PowVec3(v, invGamma); }

float sRGBToLuma(vec3 col)
{
    //return dot(col, vec3(0.2126f, 0.7152f, 0.0722f));
	return dot(col, vec3(0.299f, 0.587f, 0.114f));
}

float KarisAverage(vec3 col)
{
	// Formula is 1 / (1 + luma)
	float luma = sRGBToLuma(ToSRGB(col)) * 0.25f;
	return 1.0f / (1.0f + luma);
}

// NOTE: This is the readable version of this shader. It will be optimized!
void main()
{
	float x = 1.0 / uScreenWidth;
	float y = 1.0 / uScreenHeight;

	// Take 13 samples around current texel:
	// a - b - c
	// - j - k -
	// d - e - f
	// - l - m -
	// g - h - i
	// === ('e' is the current texel) ===
	vec3 a = texture(uSrcTexture, vec2(vTexCoord.x - 2*x, vTexCoord.y + 2*y)).rgb;
	vec3 b = texture(uSrcTexture, vec2(vTexCoord.x,       vTexCoord.y + 2*y)).rgb;
	vec3 c = texture(uSrcTexture, vec2(vTexCoord.x + 2*x, vTexCoord.y + 2*y)).rgb;

	vec3 d = texture(uSrcTexture, vec2(vTexCoord.x - 2*x, vTexCoord.y)).rgb;
	vec3 e = texture(uSrcTexture, vec2(vTexCoord.x,       vTexCoord.y)).rgb;
	vec3 f = texture(uSrcTexture, vec2(vTexCoord.x + 2*x, vTexCoord.y)).rgb;

	vec3 g = texture(uSrcTexture, vec2(vTexCoord.x - 2*x, vTexCoord.y - 2*y)).rgb;
	vec3 h = texture(uSrcTexture, vec2(vTexCoord.x,       vTexCoord.y - 2*y)).rgb;
	vec3 i = texture(uSrcTexture, vec2(vTexCoord.x + 2*x, vTexCoord.y - 2*y)).rgb;

	vec3 j = texture(uSrcTexture, vec2(vTexCoord.x - x, vTexCoord.y + y)).rgb;
	vec3 k = texture(uSrcTexture, vec2(vTexCoord.x + x, vTexCoord.y + y)).rgb;
	vec3 l = texture(uSrcTexture, vec2(vTexCoord.x - x, vTexCoord.y - y)).rgb;
	vec3 m = texture(uSrcTexture, vec2(vTexCoord.x + x, vTexCoord.y - y)).rgb;

	// Apply weighted distribution:
	// 0.5 + 0.125 + 0.125 + 0.125 + 0.125 = 1
	// a,b,d,e * 0.125
	// b,c,e,f * 0.125
	// d,e,g,h * 0.125
	// e,f,h,i * 0.125
	// j,k,l,m * 0.5
	// This shows 5 square areas that are being sampled. But some of them overlap,
	// so to have an energy preserving downsample we need to make some adjustments.
	// The weights are the distributed, so that the sum of j,k,l,m (e.g.)
	// contribute 0.5 to the final color output. The code below is written
	// to effectively yield this sum. We get:
	// 0.125*5 + 0.03125*4 + 0.0625*4 = 1

	// Check if we need to perform Karis average on each block of 4 samples
	vec3 groups[5];
	switch (uMipLevel)
	{
	case 0:
	  // We are writing to mip 0, so we need to apply Karis average to each block
	  // of 4 samples to prevent fireflies (very bright subpixels, leads to pulsating
	  // artifacts).
	  groups[0] = (a+b+d+e) * (0.125f/4.0f);
	  groups[1] = (b+c+e+f) * (0.125f/4.0f);
	  groups[2] = (d+e+g+h) * (0.125f/4.0f);
	  groups[3] = (e+f+h+i) * (0.125f/4.0f);
	  groups[4] = (j+k+l+m) * (0.5f/4.0f);
	  float kw0 = KarisAverage(groups[0]);
      float kw1 = KarisAverage(groups[1]);
      float kw2 = KarisAverage(groups[2]);
      float kw3 = KarisAverage(groups[3]);
      float kw4 = KarisAverage(groups[4]);
      downsample = (kw0 * groups[0] + kw1* groups[1] + kw2 * groups[2] + kw3* groups[3] + kw4 * groups[4]) / (kw0 + kw1 + kw2 + kw3 + kw4);
	  downsample = max(downsample, 0.0001f);
	  break;
	default:
	  downsample = e*0.125;                // ok
	  downsample += (a+c+g+i)*0.03125;     // ok
	  downsample += (b+d+f+h)*0.0625;      // ok
	  downsample += (j+k+l+m)*0.125;       // ok
	  break;
	}
}