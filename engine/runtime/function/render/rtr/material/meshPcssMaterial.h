#pragma once
#include "material.h"

namespace ff {

	class MeshPcssMaterial :public Material {
	public:
		using Ptr = std::shared_ptr<MeshPcssMaterial>;
		static Ptr create() { return std::make_shared<MeshPcssMaterial>(); }

		MeshPcssMaterial() noexcept;

		~MeshPcssMaterial() noexcept;

		float mShininess{ 32.0f };
	};
}