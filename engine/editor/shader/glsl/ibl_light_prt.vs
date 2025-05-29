#version 330 core
layout (location = 0) in vec3 aVertexPosition;

out vec3 vPosWorld;

uniform mat4 uProjectionMatrix;
uniform mat4 uViewMatrix;

void main()
{
    vPosWorld = aVertexPosition;  
    gl_Position = (uProjectionMatrix * uViewMatrix * vec4(vPosWorld, 1.0)).xyww;
}