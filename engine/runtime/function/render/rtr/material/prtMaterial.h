#pragma once
#include "material.h"

namespace ff {

	class PrtMaterial :public Material {
	public:
		using Ptr = std::shared_ptr<PrtMaterial>;
		static Ptr create() { return std::make_shared<PrtMaterial>(); }

		PrtMaterial() noexcept;

		~PrtMaterial() noexcept;

		std::vector<std::vector<float>> prtLi;
	};
}