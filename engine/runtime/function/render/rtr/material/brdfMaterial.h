#pragma once
#include "material.h"

namespace ff {

	class BrdfMaterial :public Material {
	public:
		using Ptr = std::shared_ptr<BrdfMaterial>;
		static Ptr create() { return std::make_shared<BrdfMaterial>(); }

		BrdfMaterial() noexcept;

		~BrdfMaterial() noexcept;

		float mMetallic{ 1.0f };
		float mRoughness{ 0.95f };
		glm::vec3 mLightDir = glm::vec3(1.0f);
		glm::vec3 mLightRadiance = glm::vec3(1.0f);
	};
}