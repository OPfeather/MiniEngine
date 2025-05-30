#pragma once
#include "light.h"
#include "areaLightShadow.h"

//ƽ�й�Դ�࣬����ƽ�й���Ӱ 
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