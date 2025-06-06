#version 330 core
layout (location = 0) in vec3 aVertexPosition;
layout (location = 1) in vec2 aTexCoords;

out vec2 vTextureCoord;

void main()
{
    vTextureCoord = aTexCoords;
	gl_Position = vec4(aVertexPosition, 1.0);
}