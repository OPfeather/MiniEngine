#pragma once
#include "../global/base.h"
#include "../global/constant.h"
#include "../core/object3D.h"
#include "lightShadow.h"

//��Դ�࣬�ṩ��Դ��
namespace ff {
	struct VertexAL {
        glm::vec3 position;
        glm::vec3 normal;
        glm::vec2 texcoord;
    };
	
	enum LightType
	{
		DIRECTION_LIGHT,
		POINT_LIGHT,
		AREA_LIGHT,
		IBL
	};

	class OrthographicMatrix {
	public:
		using Ptr = std::shared_ptr<OrthographicMatrix>;
		static Ptr create(float left, float right, float bottom, float top, float near, float far) {
			return std::make_shared <OrthographicMatrix>(left, right, bottom, top, near, far);
		}

		OrthographicMatrix(float left, float right, float bottom, float top, float near, float far) noexcept;
		glm::mat4 getProjectionMatrix() noexcept { return mProjectionMatrix; }

		~OrthographicMatrix() noexcept;

	private:
		glm::mat4 updateProjectionMatrix() noexcept;

	private:
		float mLeft{ 0.0f };
		float mRight{ 0.0f };
		float mTop{ 0.0f };
		float mBottom{ 0.0f };
		float mNear{ 0.0f };
		float mFar{ 0.0f };
		glm::mat4 mProjectionMatrix = glm::mat4(1.0f);
	};

	class PerspectiveMatrix {
	public:
		using Ptr = std::shared_ptr<PerspectiveMatrix>;
		static Ptr create(float near, float far, float aspect, float fov) {
			return std::make_shared <PerspectiveMatrix>(near, far, aspect, fov);
		}

		PerspectiveMatrix(float near, float far, float aspect, float fov) noexcept;
		glm::mat4 getProjectionMatrix() noexcept { return mProjectionMatrix; }

		~PerspectiveMatrix() noexcept;

	private:
		glm::mat4 updateProjectionMatrix() noexcept;

	private:

		float mFar{ 20.0f };
		float mNear{ 0.1f };
		float mAspect{ 1.3f };
		float mFov{ 45.0f };
		glm::mat4 mProjectionMatrix = glm::mat4(1.0f);
	};

	class Light {
	public:
		using Ptr = std::shared_ptr<Light>;
		static Ptr create(glm:: vec3 lightPos) {
			return std::make_shared<Light>(lightPos);
		}

		Light(glm::vec3 lightPos) noexcept;

		~Light() noexcept;

		void updateViewMatrix() noexcept;
		void updatePosition(glm::vec3 pos) noexcept;

		glm::mat4 getViewMatrix() noexcept { return mViewMatrix;}
		glm::mat4 getProjectionMatrix();

		GLuint loadMinvTexture();
		GLuint loadFGTexture();

	public:
		glm::vec3	mColor = glm::vec3(1.0f);
		float		mIntensity = 1.0f;
		glm::vec3	mPos = glm::vec3(0.0f);
		//LightShadow::Ptr	mShadow{ nullptr };
		glm::mat4 mViewMatrix = glm::mat4(1.0f);
		PerspectiveMatrix::Ptr mPerspectiveMatrix{ nullptr };
		OrthographicMatrix::Ptr mOrthographicMatrix{ nullptr };

		void light_shape_render();

		LightType mType = DIRECTION_LIGHT;
		unsigned int sphereLightVAO = 0;
		unsigned int lightIndexCount = 0;

		//面光源
		unsigned int areaLightVAO = 0;
		glm::vec3 rotation = glm::vec3(0.0f);
		std::vector<VertexAL> edgePos;

		GLuint M_INV = 0;  //M的逆矩阵
		GLuint FG = 0;     //GGX BRDF shadowing and Fresnel
	};
}