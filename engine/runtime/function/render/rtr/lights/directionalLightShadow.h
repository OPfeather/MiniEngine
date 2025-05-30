#pragma once 
#include "lightShadow.h"

//ƽ�й�Դ��������Ӱ�ࣨ����ͶӰ����Ӱ�ࣩ
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