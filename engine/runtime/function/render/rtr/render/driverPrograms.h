#pragma once
#include "../global/base.h"
#include "../objects/renderableObject.h"
#include "../material/material.h"
#include "../lights/light.h"

namespace ff {

	class DriverPrograms;
	class DriverProgram {
		friend DriverPrograms;
	public:

		//����material��object��Ϣ����
		struct Parameters {
			using Ptr = std::shared_ptr<Parameters>;
			static Ptr create() { return std::make_shared<Parameters>(); }

			std::string		mVertex;//vs�Ĵ���
			std::string		mFragment;//fs�Ĵ���

			bool			mInstancing{ false };//�Ƿ�����ʵ������,Todo
			bool			mHasNormal{ false };//���λ��Ƶ�ģ���Ƿ��з���
			bool			mHasUV{ false };//���λ��Ƶ�ģ���Ƿ���uv
			bool			mHasColor{ false };//���λ��Ƶ�ģ���Ƿ��ж�����ɫ
			bool			mHasDiffuseMap{ false };//���λ��Ƶ�ģ����ʹ�õĲ����Ƿ���diffuseMap
			bool			mHasEnvCubeMap{ false };//���λ��Ƶ�ģ����ʹ�õĲ����Ƿ��л�����ͼ
			bool			mHasSpecularMap{ false };//���λ��Ƶ�ģ����ʹ�õĲ����Ƿ��о��淴����ͼ
			bool			mHasNormalMap{ false };//���λ��Ƶ�ģ����ʹ�õĲ����Ƿ��о��淴����ͼ
			LightType       mLightType = DIRECTION_LIGHT;
			uint32_t		mDepthPacking{ 0 };

			bool		mDenoise{ false };
			bool		mTaa{ false };
		};

		using Ptr = std::shared_ptr<DriverProgram>;
		static Ptr create(const Parameters::Ptr& parameters) {
			return std::make_shared <DriverProgram>(parameters);
		}

		//1��shader��prefixDefine
		//2��attribute location���滻
		//3���汾��չ�ȴ�����ں�
		//4�������յ�shader��������
		DriverProgram(const Parameters::Ptr& parameters) noexcept;

		~DriverProgram() noexcept;

		auto getID() const noexcept { return mID; }

		auto getCacheKey() const noexcept { return mCacheKey; }

		void use() const
		{ 
			glUseProgram(mID); 
		}
		// utility uniform functions
		// ------------------------------------------------------------------------
		void setBool(const std::string &name, bool value) const
		{         
			glUniform1i(glGetUniformLocation(mID, name.c_str()), (int)value); 
		}
		// ------------------------------------------------------------------------
		void setInt(const std::string &name, int value) const
		{ 
			glUniform1i(glGetUniformLocation(mID, name.c_str()), value); 
		}
		// ------------------------------------------------------------------------
		void setFloat(const std::string &name, float value) const
		{ 
			glUniform1f(glGetUniformLocation(mID, name.c_str()), value); 
		}
		// ------------------------------------------------------------------------
		void setVec2(const std::string &name, const glm::vec2 &value) const
		{ 
			glUniform2fv(glGetUniformLocation(mID, name.c_str()), 1, &value[0]); 
		}
		void setVec2(const std::string &name, float x, float y) const
		{ 
			glUniform2f(glGetUniformLocation(mID, name.c_str()), x, y); 
		}
		// ------------------------------------------------------------------------
		void setVec3(const std::string &name, const glm::vec3 &value) const
		{ 
			glUniform3fv(glGetUniformLocation(mID, name.c_str()), 1, &value[0]); 
		}
		void setVec3(const std::string &name, float x, float y, float z) const
		{ 
			glUniform3f(glGetUniformLocation(mID, name.c_str()), x, y, z); 
		}
		// ------------------------------------------------------------------------
		void setVec4(const std::string &name, const glm::vec4 &value) const
		{ 
			glUniform4fv(glGetUniformLocation(mID, name.c_str()), 1, &value[0]); 
		}
		void setVec4(const std::string &name, float x, float y, float z, float w) const
		{ 
			glUniform4f(glGetUniformLocation(mID, name.c_str()), x, y, z, w); 
		}
		// ------------------------------------------------------------------------
		void setMat2(const std::string &name, const glm::mat2 &mat) const
		{
			glUniformMatrix2fv(glGetUniformLocation(mID, name.c_str()), 1, GL_FALSE, &mat[0][0]);
		}
		// ------------------------------------------------------------------------
		void setMat3(const std::string &name, const glm::mat3 &mat) const
		{
			glUniformMatrix3fv(glGetUniformLocation(mID, name.c_str()), 1, GL_FALSE, &mat[0][0]);
		}
		// ------------------------------------------------------------------------
		void setMat4(const std::string &name, const glm::mat4 &mat) const
		{
			glUniformMatrix4fv(glGetUniformLocation(mID, name.c_str()), 1, GL_FALSE, &mat[0][0]);
			
		}

	private:
		void replaceAttributeLocations(std::string& shader) noexcept;

	private:
		uint32_t	mID{ 0 };//driverProgram �Լ���id��
		HashType	mCacheKey{ 0 };//��parameters(���涨��Ľṹ��)�����ϼ����������hashֵ
		uint32_t	mRefCount{ 0 };//��������ж������ñ�Program��renderItem
	};


	class DriverPrograms {
	public:
		using Ptr = std::shared_ptr<DriverPrograms>;
		static Ptr create() {
			return std::make_shared <DriverPrograms>();
		}

		DriverPrograms() noexcept;

		~DriverPrograms() noexcept;

		DriverProgram::Ptr acquireProgram(const DriverProgram::Parameters::Ptr& parameters, HashType cacheKey) noexcept;

		DriverProgram::Parameters::Ptr getParameters(
			const Material::Ptr& material,
			const Object3D::Ptr& object,
			LightType lightType,
			std::string vsCode, std::string fsCode,
			bool denoise = false, bool taa = false);

		HashType getProgramCacheKey(const DriverProgram::Parameters::Ptr& parameters) noexcept;

		void release(const DriverProgram::Ptr& program) noexcept;

	private:
		//key-paramters���ɵĹ�ϣֵ��value-�ñ�parameters���ɵ�driverProgram
		std::unordered_map<HashType, DriverProgram::Ptr> mPrograms{};
	};
}