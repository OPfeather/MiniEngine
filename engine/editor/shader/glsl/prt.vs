//prtVertex.glsl

layout (location = 0) in vec3 aVertexPosition;
layout (location = 1) in vec3 aNormalPosition;
layout (location = 3) in mat3 aPrecomputeLT;

uniform mat4 uModelMatrix;
uniform mat4 uViewMatrix;
uniform mat4 uProjectionMatrix;

uniform mat3 uPrecomputeL[3];

out vec3 vColor;

//L与LT的点积，因为BRDF传的是一个值，而L传的是有RGB三个维度的颜色，所以我们需要把L分为LR、LG、LB分别计算。因为球面谐波函数一共有9个系数，所以我们用mat3类型的三维矩阵记录。
 vec3 L_dot_LT(mat3 LR,mat3 LG,mat3 LB,mat3 LT)
{
  vec3 result=vec3(LR[0][0],LG[0][0],LB[0][0])*LT[0][0]+
    vec3(LR[0][1],LG[0][1],LB[0][1])*LT[0][1]+
    vec3(LR[0][2],LG[0][2],LB[0][2])*LT[0][2]+
    vec3(LR[1][0],LG[1][0],LB[1][0])*LT[1][0]+
    vec3(LR[1][1],LG[1][1],LB[1][1])*LT[1][1]+
    vec3(LR[1][2],LG[1][2],LB[1][2])*LT[1][2]+
    vec3(LR[2][0],LG[2][0],LB[2][0])*LT[2][0]+
    vec3(LR[2][1],LG[2][1],LB[2][1])*LT[2][1]+
    vec3(LR[2][2],LG[2][2],LB[2][2])*LT[2][2];

  return result;
}

void main(void) {
  // 无实际作用，避免aNormalPosition被优化后产生警告
  vNormal = (uModelMatrix * vec4(aNormalPosition, 0.0)).xyz;

  mat3 LR=uPrecomputeL[0];
  mat3 LG=uPrecomputeL[1];
  mat3 LB=uPrecomputeL[2];
  
  vColor=L_dot_LT(LR,LG,LB,aPrecomputeLT);

  gl_Position = uProjectionMatrix * uViewMatrix * uModelMatrix * vec4(aVertexPosition, 1.0);
}
