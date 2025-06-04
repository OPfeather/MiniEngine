#pragma once

#include "runtime/function/render/pathtracing/common/util.h"
#include "runtime/function/render/pathtracing/common/hittable.h"

namespace MiniEngine::PathTracing
{
    //BVH类，构建BVH，判断光线是否击中BVH
    class BVH : public Hittable
    {
    public:
        shared_ptr<Hittable> left;//该 BVH 节点的左右子节点
        shared_ptr<Hittable> right;
        AABB box;//当前 BVH 节点包围的整体 AABB 区域，用于快速裁剪射线是否有必要递归下去

        BVH() = default;
        BVH(const HittableList &list) : BVH(list.objects, 0, list.objects.size()) {}
        BVH(const std::vector<shared_ptr<Hittable>> &src_objects, size_t start, size_t end)
        {
            auto objects = src_objects; // Create a modifiable array of the source scene objects

            int axis = static_cast<int>(linearRand(0, 3));  //随机选择划分轴（x=0, y=1, z=2）
            auto comparator = (axis == 0)   ? compareX
                              : (axis == 1) ? compareY
                                            : compareZ;

            size_t object_span = end - start;

            //若只有一个物体：左右子树都是它自己
            if (object_span == 1)
            {
                left = right = objects[start];
            }
            else if (object_span == 2)
            {
                if (comparator(objects[start], objects[start + 1]))
                {
                    left = objects[start];
                    right = objects[start + 1];
                }
                else
                {
                    left = objects[start + 1];
                    right = objects[start];
                }
            }
            else
            {
                std::sort(objects.begin() + start, objects.begin() + end, comparator);

                auto mid = start + object_span / 2;
                left = make_shared<BVH>(objects, start, mid);
                right = make_shared<BVH>(objects, mid, end);
            }

            AABB box_left, box_right;

            if (!left->aabb(box_left) || !right->aabb(box_right))
                std::cerr << "No bounding box in BVH constructor.\n";

            box = AABB::getSurroundingBox(box_left, box_right);//包围左右节点的大的包围盒
        }

        virtual bool hit(const Ray &r, float t_min, float t_max, HitRecord &rec) const override;
        virtual bool aabb(AABB &bounding_box) const override;

    private:
        inline static bool compare(const shared_ptr<Hittable> a, const shared_ptr<Hittable> b, int axis)
        {
            AABB box_a;
            AABB box_b;

            if (!a->aabb(box_a) || !b->aabb(box_b))
                std::cerr << "No bounding box in bvh node constructor.\n";

            return box_a.min[axis] < box_b.min[axis];
        }

        static bool compareX(const shared_ptr<Hittable> a, const shared_ptr<Hittable> b)
        {
            return compare(a, b, 0);
        }

        static bool compareY(const shared_ptr<Hittable> a, const shared_ptr<Hittable> b)
        {
            return compare(a, b, 1);
        }

        static bool compareZ(const shared_ptr<Hittable> a, const shared_ptr<Hittable> b)
        {
            return compare(a, b, 2);
        }
    };

    bool BVH::aabb(AABB &bounding_box) const
    {
        bounding_box = box;
        return true;
    }

    bool BVH::hit(const Ray &r, float t_min, float t_max, HitRecord &rec) const
    {
        if (!box.hit(r, t_min, t_max))
            return false;

        bool hit_left = left->hit(r, t_min, t_max, rec);
        bool hit_right = right->hit(r, t_min, hit_left ? rec.t : t_max, rec);

        return hit_left || hit_right;
    }

}
