#pragma once 
#include "lightShadow.h"

//平行光源产生的阴影类（正交投影的阴影类）
namespace ff {

	class DirectionalLightShadow:public LightShadow {
	public:
		using Ptr = std::shared_ptr<DirectionalLightShadow>;
		static Ptr create() {
			return std::make_shared<DirectionalLightShadow>();
		}

		DirectionalLightShadow()noexcept;

		~DirectionalLightShadow()noexcept;
		
	};
}