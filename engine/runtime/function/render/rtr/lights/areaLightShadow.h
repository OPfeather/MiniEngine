#pragma once 
#include "lightShadow.h"

//面光源产生的阴影类
namespace ff {

	class AreaLightShadow :public LightShadow {
	public:
		using Ptr = std::shared_ptr<AreaLightShadow>;
		static Ptr create() {
			return std::make_shared<AreaLightShadow>();
		}

		AreaLightShadow()noexcept;

		~AreaLightShadow()noexcept;

	};
}