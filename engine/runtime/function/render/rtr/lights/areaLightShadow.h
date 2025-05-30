#pragma once 
#include "lightShadow.h"

//���Դ��������Ӱ��
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