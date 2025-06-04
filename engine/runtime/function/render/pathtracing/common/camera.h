#pragma once

#include "runtime/function/render/pathtracing/common/util.h"

namespace MiniEngine::PathTracing
{
    //这个相机类用于生成从相机出发的射线 Ray，并支持控制视野角度、焦距、光圈等参数，从而实现物理真实的相机模糊效果（如背景虚化、焦外模糊）
    class Camera
    {
    public:
        Camera(vec3 lookfrom,//相机位置
               vec3 lookat,
               vec3 up,
               float fov,
               float aperture,//光圈大小（影响景深）
               float focus_dist,//对焦距离（虚拟屏幕到物体距离）
               float aspect_ratio)//画面宽高比
        {
            auto theta = radians(fov);
            auto h = tan(theta / 2);
            f32 viewport_height = 2.0 * h;  //焦距focus_dist为1时的成像平面大小，实际大小要乘上焦距
            f32 viewport_width = aspect_ratio * viewport_height;
            auto focal_length = 1.0;

            w = normalize(lookfrom - lookat);
            u = normalize(cross(up, w));
            v = cross(w, u);

            origin = lookfrom;
            horizontal = focus_dist * viewport_width * u;
            vertical = focus_dist * viewport_height * v;
            lower_left_corner = origin - horizontal / f32(2) - vertical / f32(2) - focus_dist * w;

            lens_radius = aperture / f32(2);
        }

        //从相机生成一条射线，对应视口中 (s,t) 像素坐标
        Ray getRay(f32 s, f32 t) const
        {
            vec3 rd = lens_radius * vec3(diskRand(1.f), 0);//在镜头光圈中采样一个随机点，rd.z = 0，因为光圈是平的。
            vec3 offset = u * rd.x + v * rd.y;//将随机采样到的光圈点坐标（rd）从“局部平面空间”转换为世界空间中的偏移量

            //光线是从 offset 偏移后的位置出发的。
            //不同像素会从光圈上的不同点发射，从而在景深范围外物体上形成模糊
            return Ray(origin + offset, lower_left_corner + s * horizontal + t * vertical - origin - offset);
        }

    private:
        vec3 origin;//相机位置（世界坐标系）
        vec3 lower_left_corner;// 视口左下角
        //成像平面的高宽在世界坐标系下的坐标，用来确定从相机向像素发出的光的世界坐标方向
        //比如最终的图像像素是w*h,那么每个uv对应的世界坐标就可以通过horizontal和vertical计算出来
        //这成像平面与投影矩阵无关，不是近平面，而是真实相机的模拟，所以求世界坐标也不需要视口变换和投影矩阵的逆变换
        vec3 horizontal;// 视口的宽度向量（世界坐标系下）
        vec3 vertical;//视口的高度向量（世界坐标系下）
        vec3 u, v, w; // 相机局部坐标系（三个正交轴）
        //小光圈（lens_radius 小） → 景深范围大 → 画面清晰
        //大光圈（lens_radius 大） → 景深范围小 → 背景模糊
        float lens_radius;// 光圈半径（决定模糊程度）
    };
}