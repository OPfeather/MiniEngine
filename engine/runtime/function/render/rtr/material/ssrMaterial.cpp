#include "ssrMaterial.h"

namespace ff {

	SsrMaterial::SsrMaterial(DirectionalLight::Ptr directionLight, uint32_t packing) noexcept {
		mType = MaterialName::SsrMaterial;
		mIsSsrMaterial = true;
		mDrawMode = DrawMode::Triangles;
		mPacking = packing;
		mDirectionLight = directionLight;
	}

	void SsrMaterial::setMaterialType(std::string type) {
		mType = type;
	}

	SsrMaterial::~SsrMaterial() noexcept {}
}