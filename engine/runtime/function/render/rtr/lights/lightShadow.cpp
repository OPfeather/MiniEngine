#include "lightShadow.h"
#include "light.h"

namespace ff {
	LightShadow::LightShadow(const Camera::Ptr& camera) noexcept {
		mCamera = camera;
	}

	LightShadow::~LightShadow() noexcept {}
}