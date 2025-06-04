#pragma once

#include "runtime/function/render/pathtracing/common/util.h"

namespace MiniEngine::PathTracing
{
    //实现的是一个 正交基 (Orthonormal Basis, ONB) 类，用于将局部坐标系（例如在某个表面法线方向）下的向量变换到世界空间
    //构建一个以法线为 z 轴的局部坐标系，将局部空间中采样的方向（比如半球采样）转换为世界坐标
    class ONB
    {
    public:
        vec3 axis[3];//局部坐标系的 3 个正交轴（u, v, w）

        ONB() {}

        vec3 local(float a, float b, float c) const
        {
            return a * axis[0] + b * axis[1] + c * axis[2];
        }

        //提供从局部坐标到世界坐标的变换
        vec3 local(const vec3 &a) const
        {
            return a.x * axis[0] + a.y * axis[1] + a.z * axis[2];
        }

        //构建以 n 为 z 轴方向的局部正交坐标系。
        void buildONB(const vec3 &n)
        {
            axis[2] = normalize(n);
            vec3 a = (fabs(axis[2].x) > 0.9) ? vec3(0, 1, 0) : vec3(1, 0, 0);
            axis[1] = normalize(cross(axis[2], a));
            axis[0] = cross(axis[2], axis[1]);
        }

    public:
    };

}
