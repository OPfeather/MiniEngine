#version 330 core

out vec4 FragColor;
uniform vec3 uLightRadiance;

void main()
{
	FragColor = vec4(vec3(uLightRadiance) ,1);
}