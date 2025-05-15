#include "prtMaterial.h"

namespace ff {

	PrtMaterial::PrtMaterial() noexcept {
		mType = MaterialName::PrtMaterial;
		mIsPrtMaterial = true;
		mDrawMode = DrawMode::Triangles;
	}

	PrtMaterial::~PrtMaterial() noexcept {}
}