#include "kullaContyMaterial.h"

namespace ff {

	KullaContyMaterial::KullaContyMaterial() noexcept {
		mType = MaterialName::KullaContyMaterial;
		mIsKullaContyMaterial = true;
		mDrawMode = DrawMode::Triangles;
	}

	KullaContyMaterial::~KullaContyMaterial() noexcept {}
}