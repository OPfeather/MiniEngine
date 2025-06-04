#pragma once

#include "runtime/function/render/pathtracing/common/util.h"
#include "runtime/function/render/pathtracing/acc_struct/aabb.h"
#include "runtime/function/render/render_mesh.h"

namespace MiniEngine::PathTracing
{
    class Material;

    //记录射线命中信息
    struct HitRecord
    {
        MiniEngine::Vertex hit_point;//命中点的属性，如位置 Position、法线 Normal、纹理坐标等
        shared_ptr<Material> mat_ptr;//命中处的材质指针（用于计算 BRDF、反射/折射）
        float t;  //射线走过的时间t
        bool front_face;//是否命中了表面“外侧”（true）还是“内侧”

        //用于确定和设置法线方向是否要反转
        //无论是从内外命中，法线总是指向射线反方向，方便统一处理反射
        inline void setFaceNormal(const Ray &r, const vec3 &outward_normal)
        {
            front_face = dot(r.direction, outward_normal) < 0;
            hit_point.Normal = front_face ? outward_normal : -outward_normal;
        }
    };

    //抽象类，表示“可以被射线命中的对象”，是所有几何体（三角形、球、矩形、BVH等）的基类
    class Hittable
    {
    public:
        virtual bool hit(const Ray &r, float t_min, float t_max, HitRecord &rec) const = 0;
        virtual bool aabb(AABB &bounding_box) const = 0;

        virtual float getArea() const
        {
            return 0.0;
        }

        virtual float getPDF(const vec3 &o, const vec3 &v) const
        {
            return 0.0;
        }

        virtual vec3 random(const vec3 &o) const
        {
            return vec3(1, 0, 0);
        }
    };

    //一组 Hittable 的集合（场景容器）
    //用来管理多个 Hittable 物体（如一堆三角形、多个球），并对它们统一做射线求交。可以把它看成一个小场景或一个物体组合。
    class HittableList : public Hittable
    {
    public:
        vector<shared_ptr<Hittable>> objects;

        HittableList() {}
        HittableList(shared_ptr<Hittable> object) { add(object); }

        void clear() { objects.clear(); }
        //添加一个物体
        void add(shared_ptr<Hittable> object) { objects.push_back(object); }

        //对列表中所有物体遍历求交，找最近的
        virtual bool hit(const Ray &r, float t_min, float t_max, HitRecord &rec) const override;
        //合并所有子物体的 AABB，返回一个大的包围盒
        virtual bool aabb(AABB &bounding_box) const override;
        //从多个物体中组合 PDF
        virtual float getPDF(const vec3 &o, const vec3 &v) const override;
        //从组合中随机挑选一个物体进行采样
        virtual vec3 random(const vec3 &o) const override;
    };

}
