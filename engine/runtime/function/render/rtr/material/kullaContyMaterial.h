#pragma once
#include "material.h"

namespace ff {

	class KullaContyMaterial :public Material {
	public:
		using Ptr = std::shared_ptr<KullaContyMaterial>;
		static Ptr create() { return std::make_shared<KullaContyMaterial>(); }

		KullaContyMaterial() noexcept;

		~KullaContyMaterial() noexcept;

		float mMetallic{ 1.0f };
		float mRoughness{ 0.95f };
		glm::vec3 mLightDir = glm::vec3(1.0f);  //世界坐标系方向
		glm::vec3 mLightPos = glm::vec3(3.0f, 4.0f, 5.0f);  //世界坐标系位置
		glm::vec3 mLightRadiance = glm::vec3(1.0f);

		Texture::Ptr	mBRDFLut{ nullptr };
		Texture::Ptr	mEavgLut{ nullptr };
	};
}