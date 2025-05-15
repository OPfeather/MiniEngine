#include "brdfMaterial.h"

namespace ff {

	BrdfMaterial::BrdfMaterial() noexcept {
		mType = MaterialName::BrdfMaterial;
		mIsBrdfMaterial = true;
		mDrawMode = DrawMode::Triangles;
	}

	BrdfMaterial::~BrdfMaterial() noexcept {}
}