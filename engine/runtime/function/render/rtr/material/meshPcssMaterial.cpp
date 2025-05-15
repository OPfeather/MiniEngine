#include "MeshPcssMaterial.h"

namespace ff {

	MeshPcssMaterial::MeshPcssMaterial() noexcept {
		mType = MaterialName::MeshPcssMaterial;
		mIsMeshPcssMaterial = true;
		mDrawMode = DrawMode::Triangles;
	}

	MeshPcssMaterial::~MeshPcssMaterial() noexcept {}
}