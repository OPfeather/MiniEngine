layout (location = 0) in vec3 aVertexPosition;
layout (location = 1) in vec2 aTextureCoord;

uniform mat4 uViewMatrix;
uniform mat4 uProjectionMatrix;

out mat4 vWorldToScreen;
out vec2 vTexCoords;

void main(void) {
  vWorldToScreen = uProjectionMatrix * uViewMatrix;
  vTexCoords = aTextureCoord;
  gl_Position = vec4(aVertexPosition, 1.0);
}