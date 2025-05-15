#pragma once
#include "material.h"
#include "../lights/directionalLight.h"

namespace ff {

	class SsrMaterial :public Material {
	public:
		static constexpr uint32_t	NoPacking = 0;
		static constexpr uint32_t	RGBADepthPacking = 1;

		using Ptr = std::shared_ptr<SsrMaterial>;
		static Ptr create(DirectionalLight::Ptr directionLight, uint32_t packing = NoPacking) { return std::make_shared<SsrMaterial>(directionLight, packing); }

		SsrMaterial(DirectionalLight::Ptr directionLight, uint32_t packing) noexcept;

		void setMaterialType(std::string type);

		~SsrMaterial() noexcept;

		float mShininess{ 32.0f };
		//是否启用深度打包
		uint32_t mPacking{ NoPacking };

		DirectionalLight::Ptr mDirectionLight{ nullptr };

		Texture::Ptr	mSsrShadowMap{ nullptr };   //光源坐标下的深度贴图
		Texture::Ptr	mSsrDepthMap{ nullptr };   //屏幕坐标下的深度贴图
		Texture::Ptr	mSsrDiffuseMap{ nullptr }; //屏幕坐标下的漫反射贴图
		Texture::Ptr	mSsrVisibilityMap{ nullptr };  //屏幕坐标下是否有阴影
		Texture::Ptr	mSsrPosWorldMap{ nullptr };//像素点的世界坐标
		Texture::Ptr	mSsrNormalWorldMap{ nullptr };//像素点的世界坐标法线
	};
}