#pragma once

#include "runtime/function/render/pathtracing/common/util.h"
#include "runtime/function/render/pathtracing/common/hittable.h"

namespace MiniEngine::PathTracing
{
    class PDF
    {
    public:
        virtual ~PDF() {}

        //评估某个归一化方向 direction 的 概率密度 p(ω)
        virtual float value(const vec3 &direction) const = 0;
        //按自身分布随机生成一个方向并返回其 世界空间向量
        virtual vec3 generate() const = 0;
    };

    //余弦加权半球分布
    class CosinePDF : public PDF
    {
    public:
        ONB onb;//余弦分布先在局部空间采样，随后通过 ONB 把向量变到世界空间

        CosinePDF(const vec3 &normal)
        {
            onb.buildONB(normal);
        }

        virtual float value(const vec3 &direction) const override
        {
            auto cosine = dot(normalize(direction), onb.axis[2]);
            return (cosine <= 0) ? 0 : cosine / PI;
        }

        virtual vec3 generate() const override
        {
            return onb.local(cosineRand());
        }
    };

    //面向 几何体 / 光源 的 pdf
    class HittablePDF : public PDF
    {
    public:
        vec3 o;// 采样起点（着色点）
        shared_ptr<Hittable> ptr;// 要采样的几何体（通常是一组光源）

        HittablePDF(shared_ptr<Hittable> p, const vec3 &origin) : ptr(p), o(origin) {}

        virtual float value(const vec3 &direction) const override
        {
            return ptr->getPDF(o, direction);
        }

        virtual vec3 generate() const override
        {
            return ptr->random(o);
        }
    };

    //多分布混合
    class MixturePDF : public PDF
    {
    public:
        shared_ptr<PDF> p[2];
        float weight;

        MixturePDF(shared_ptr<PDF> p0, shared_ptr<PDF> p1, float w)
        {
            p[0] = p0;
            p[1] = p1;
            weight = w;
        }

        virtual float value(const vec3 &direction) const override
        {
            return (1.0 - weight) * p[0]->value(direction) + weight * p[1]->value(direction);
        }

        virtual vec3 generate() const override
        {
            if (linearRand(0.f,1.f) < (1.0 - weight))
                return p[0]->generate();
            else
                return p[1]->generate();
        }

    };
}