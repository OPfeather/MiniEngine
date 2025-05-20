#pragma once
#include "../global/base.h"
#include "../camera/camera.h"
#include "../math/frustum.h"
#include "../render/renderTarget.h"

//��Դ��������Ӱ�࣬Ϊ������Ӱ��ͼ�ṩ��Ҫ�Ĳ���
namespace ff {

	class Light;

	//������һյ��Դ����������Ӱ��صĸ������
	class LightShadow {
	public:
		using Ptr = std::shared_ptr<LightShadow>;
		static Ptr create(const Camera::Ptr& camera) {
			return std::make_shared<LightShadow>(camera);
		}
 
		LightShadow(const Camera::Ptr& camera) noexcept;

		~LightShadow() noexcept;
 
	public:
		Camera::Ptr				mCamera{ nullptr };//���ڹ�Դλ�ã�������Ⱦ�����ͼ�������
		float					mBias{ -0.003f };//Ϊ�˷�ֹShadowAnce�����ƫ����
		float					mRadius{ 1.0f };//��ֹ��Ե��Ӳ��ʱ�򣬲��õ������Ͳ�����Χ�Ĵ�С

		glm::vec2				mMapSize = glm::vec2(512.0, 512.0);//ShadowMap�ֱ��ʵĴ�С 

		//���ڹ�Դ������Ӱ������ϵͳʵ����ƽ�й�ϵͳ�����ǻ�û��ʵ�ֵ��Դ
		//������ȥ�˽���Դ���Ƶ�ͬѧ�����Կ���threejs������ʵ��
		glm::vec2				mFrameExtent = glm::vec2(1.0, 1.0);
		std::vector<glm::vec4>	mViewports = { glm::vec4(0.0f, 0.0f, 1.0f, 1.0f) };

		RenderTarget::Ptr		mRenderTarget{ nullptr };//��ǰ��ShadowMap����Ӧ����ȾĿ��,ShadowMap�ͷ���������ColorAttachment

		//1 ������Ķ��㣬����������ϵ��ת������Դ�������ͶӰ����ϵ�ڣ�NDC������-��û�г���w��
		//2 projectionMatrix * viewMatrix����Դ�����
		//3 ���⻹�ð�NDC�����-1��1��ת��Ϊ0-1
		glm::mat4				mMatrix = glm::mat4(1.0f);

	protected:
		//����������Դ����������Ӿ������
		Frustum::Ptr			mFrustum = Frustum::create();
	};
}