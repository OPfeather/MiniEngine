#include "areaLightShadow.h"
#include "../camera/perspectiveCamera.h"

namespace ff {

	AreaLightShadow::AreaLightShadow()noexcept :
		LightShadow(PerspectiveCamera::create(0.1f, 10000.0f, static_cast<float>(1200) / static_cast<float>(800), 45.0f)) {}

	AreaLightShadow::~AreaLightShadow()noexcept {}
}