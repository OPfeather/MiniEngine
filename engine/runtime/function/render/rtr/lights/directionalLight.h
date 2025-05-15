#pragma once
#include "light.h"
#include "directionalLightShadow.h"

//平行光源类，产生平行光阴影 
namespace ff {

	class DirectionalLight :public Light {
	public:
		using Ptr = std::shared_ptr<DirectionalLight>;
		static Ptr create() {
			return std::make_shared<DirectionalLight>();
		}

		DirectionalLight() noexcept;

		~DirectionalLight() noexcept;
	};
}