#version 330 core
out vec4 FragColor;

in vec3 Normal;
in vec3 FragPos;
in vec2 TexCoord;

uniform vec3 viewPos;
uniform sampler2D diffuse_map;
uniform sampler2D specular_map;

void main()
{
	// light
	vec3 ambientLightColor = vec3(0.2f, 0.2f, 0.2f);
	vec3 lightPos = vec3(0.f, 10.f, 0.f);

	// ambient
	vec3 ambient = ambientLightColor * texture(diffuse_map, TexCoord).rgb;

	// diffuse
	vec3 diffuseLightColor = vec3(0.5f, 0.5f, 0.5f);
	vec3 norm = normalize(Normal);
	vec3 lightDir = normalize(lightPos - FragPos);
	float diff = max(dot(norm, lightDir), 0.0);
	vec3 diffuse = diff * diffuseLightColor * texture(diffuse_map, TexCoord).rgb;

	// specular
	vec3 specularLightColor = vec3(1.0f, 1.0f, 1.0f);
	vec3 viewDir = normalize(viewPos - FragPos);
	vec3 reflectDir = reflect(-lightDir, norm);
	float spec = pow(max(dot(viewDir, reflectDir), 0.0), 16);
	vec3 specular = spec * specularLightColor * texture(specular_map, TexCoord).rgb;

	vec3 result = ambient + diffuse + specular;
	FragColor = vec4(diffuse, 1.0);
}