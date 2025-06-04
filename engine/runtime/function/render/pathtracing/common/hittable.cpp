#include "runtime/function/render/pathtracing/common/hittable.h"

namespace MiniEngine::PathTracing
{
    //对列表中所有 Hittable 物体进行射线求交，找出最近的一次命中，并记录到 rec
    bool HittableList::hit(const Ray &r, float t_min, float t_max, HitRecord &rec) const
    {
        HitRecord temp_rec;
        bool hit_anything = false;
        auto closest_so_far = t_max;

        for (const auto &object : objects)
        {
            if (object->hit(r, t_min, closest_so_far, temp_rec))
            {
                hit_anything = true;
                closest_so_far = temp_rec.t;
                rec = temp_rec;
            }
        }

        return hit_anything;
    }

    //返回从点 o 出发、沿方向 v 命中列表中物体的平均 PDF，用于光源重要性采样中的路径贡献加权
    float HittableList::getPDF(const vec3 &o, const vec3 &v) const
    {
        auto weight = 1.0 / objects.size();
        auto sum = 0.0;

        for (const auto &object : objects)
            sum += weight * object->getPDF(o, v);

        return sum;
    }

    //从当前物体集合中随机选择一个物体，然后调用它的 random(o) 函数，从它的表面采样一个方向向量（指向采样点）
    //配合 getPDF 用于从多个物体中采样方向，如多个光源合成一个“光源组”
    vec3 HittableList::random(const vec3 &o) const
    {
        auto int_size = static_cast<int>(objects.size());
        return objects[int(linearRand(0, int_size - 1) + 0.5)]->random(o);
    }

    //合并所有子物体的 AABB，构造整个集合的包围盒
    bool HittableList::aabb(AABB &bounding_box) const
    {
        if (objects.empty())
            return false;

        AABB temp_box;
        bool first_box = true;

        for (const auto &object : objects)
        {
            if (!object->aabb(temp_box))
                return false;
            bounding_box = first_box ? temp_box : AABB::getSurroundingBox(bounding_box, temp_box);
            first_box = false;
        }

        return true;
    }
}