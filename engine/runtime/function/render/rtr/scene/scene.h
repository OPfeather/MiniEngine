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
		
		//���ǿ���ָ�������������棬��������һ��ʹ���ĸ�material
		//���ܸ��������Ƿ�ӵ���Լ���material������ǿ��ʹ�����material
		Material::Ptr	mOverrideMaterial = nullptr;

		//��պ�
		CubeTexture::Ptr mBackground = nullptr;

		std::vector<RenderableObject::Ptr> mOpaques{};//�洢��͸�����������ָ��
		std::vector<RenderableObject::Ptr> mTransparents{};//�洢͸�����������ָ��
	};
}