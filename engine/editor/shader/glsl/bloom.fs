out vec4 FragColor;

in vec2 vTexCoords;

uniform sampler2D uScene;
uniform sampler2D uBloomBlur;
uniform float uBloomStrength = 0.04f;


vec3 bloom()
{
    vec3 hdrColor = texture(uScene, vTexCoords).rgb;
    vec3 bloomColor = texture(uBloomBlur, vTexCoords).rgb;
    return mix(hdrColor, bloomColor, uBloomStrength); // linear interpolation
}

void main()
{
    // to bloom or not to bloom
    vec3 result = vec3(0.0);
    result = bloom(); 

    // tone mapping
    result = result / (result + vec3(1.0));
    // also gamma correct while we're at it
    const float gamma = 2.2;
    result = pow(result, vec3(1.0 / gamma));
    FragColor = vec4(result, 1.0);
}