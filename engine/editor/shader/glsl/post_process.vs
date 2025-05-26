layout (location = 0) in vec3 aVertexPosition;
layout (location = 1) in vec2 aTextureCoord;

out vec2 vTexCoords;

void main(void) {
  vTexCoords = aTextureCoord;
  gl_Position = vec4(aVertexPosition, 1.0);
}