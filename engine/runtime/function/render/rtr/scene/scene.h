#pragma once
#include "../global/base.h"
#include "../core/object3D.h"
#include "../material/material.h"
#include "../textures/cubeTexture.h"
#include "../objects/renderableObject.h"

namespace ff {

	class Scene :public Object3D {
	public:
		using Ptr = std::shared_ptr<Scene>;
		static Ptr create() {
			return std::make_shared <Scene>();
		}

		Scene() noexcept;

		~Scene() noexcept;
		
		//我们可以指定整个场景里面，所有物体一起使用哪个material
		//不管各个物体是否拥有自己的material，都被强制使用这个material
		Material::Ptr	mOverrideMaterial = nullptr;
		MaterialType mSceneMaterialType = MeshBasicMaterialType;


		//天空盒
		CubeTexture::Ptr mBackground = nullptr;

		//预计算
		Texture::Ptr	mBRDFLut{ nullptr };
		Texture::Ptr	mEavgLut{ nullptr };

		float metallic = 1.0;
		float roughness = 0.9;

		std::vector<RenderableObject::Ptr> mOpaques{};//存储非透明物体的智能指针
		std::vector<RenderableObject::Ptr> mTransparents{};//存储透明物体的智能指针
	};
}