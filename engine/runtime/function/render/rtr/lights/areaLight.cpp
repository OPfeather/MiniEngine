#pragma once
#include "areaLight.h"

namespace ff {

	AreaLight::AreaLight() noexcept {
		mIsDirectionalLight = true;
		mShadow = AreaLightShadow::create();
	}

	AreaLight::~AreaLight() noexcept {}
}