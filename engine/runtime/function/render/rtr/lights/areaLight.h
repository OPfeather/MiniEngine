#pragma once
#include "light.h"
#include "areaLightShadow.h"

//平行光源类，产生平行光阴影 
namespace ff {

	class AreaLight :public Light {
	public:
		using Ptr = std::shared_ptr<AreaLight>;
		static Ptr create() {
			return std::make_shared<AreaLight>();
		}

		AreaLight() noexcept;

		~AreaLight() noexcept;
	};
}