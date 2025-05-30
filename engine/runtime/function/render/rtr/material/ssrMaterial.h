#pragma once
#include "material.h"
#include "../lights/directionalLight.h"

namespace ff {

	class SsrMaterial :public Material {
	public:
		static constexpr uint32_t	NoPacking = 0;
		static constexpr uint32_t	RGBADepthPacking = 1;

		using Ptr = std::shared_ptr<SsrMaterial>;
		static Ptr create(DirectionalLight::Ptr directionLight, uint32_t packing = NoPacking) { return std::make_shared<SsrMaterial>(directionLight, packing); }

		SsrMaterial(DirectionalLight::Ptr directionLight, uint32_t packing) noexcept;

		void setMaterialType(std::string type);

		~SsrMaterial() noexcept;

		float mShininess{ 32.0f };
		//�Ƿ�������ȴ��
		uint32_t mPacking{ NoPacking };

		DirectionalLight::Ptr mDirectionLight{ nullptr };

		Texture::Ptr	mSsrShadowMap{ nullptr };   //��Դ�����µ������ͼ
		Texture::Ptr	mSsrDepthMap{ nullptr };   //��Ļ�����µ������ͼ
		Texture::Ptr	mSsrDiffuseMap{ nullptr }; //��Ļ�����µ���������ͼ
		Texture::Ptr	mSsrVisibilityMap{ nullptr };  //��Ļ�������Ƿ�����Ӱ
		Texture::Ptr	mSsrPosWorldMap{ nullptr };//���ص����������
		Texture::Ptr	mSsrNormalWorldMap{ nullptr };//���ص���������귨��
	};
}