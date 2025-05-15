#pragma once
#include "material.h"

namespace ff {

	class AreaLightMaterial :public Material {
	public:
		using Ptr = std::shared_ptr<AreaLightMaterial>;
		static Ptr create() { return std::make_shared<AreaLightMaterial>(); }

		AreaLightMaterial() noexcept;

		~AreaLightMaterial() noexcept;

		float mRoughness{ 0.1f };
		float mLightIntensity = 4.0;
		glm::vec3 mLightColor = glm::vec3(1.0f, 1.0f, 1.0f);
		glm::vec3 mLightPos[4];  //多边形灯源每个点世界坐标系位置

		Texture::Ptr	mLTC1{ nullptr };// for inverse M
		Texture::Ptr	mLTC2{ nullptr };// GGX norm, fresnel, 0(unused), sphere
	};
}