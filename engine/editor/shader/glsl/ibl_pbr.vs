layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aNormal;
layout (location = 2) in vec2 aTexCoords;

out vec2 vTextureCoord;
out vec3 vPosWorld;
out vec3 vNormalWorld;

uniform mat4 uProjectionMatrix;
uniform mat4 uViewMatrix;
uniform mat4 uModelMatrix;

void main()
{
    vTextureCoord = aTexCoords;
    vPosWorld = vec3(uModelMatrix * vec4(aPos, 1.0));
    vNormalWorld = uModelMatrix * aNormal;   

    gl_Position =  uProjectionMatrix * uViewMatrix * vec4(vPosWorld, 1.0);
}