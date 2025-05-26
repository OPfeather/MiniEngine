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

		//根据material和object信息生成
		struct Parameters {
			using Ptr = std::shared_ptr<Parameters>;
			static Ptr create() { return std::make_shared<Parameters>(); }

			std::string		mVertex;//vs的代码
			std::string		mFragment;//fs的代码

			bool			mInstancing{ false };//是否启用实例绘制,Todo
			bool			mHasNormal{ false };//本次绘制的模型是否有法线
			bool			mHasUV{ false };//本次绘制的模型是否有uv
			bool			mHasColor{ false };//本次绘制的模型是否有顶点颜色
			bool			mHasDiffuseMap{ false };//本次绘制的模型所使用的材质是否有diffuseMap
			bool			mHasEnvCubeMap{ false };//本次绘制的模型所使用的材质是否有环境贴图
			bool			mHasSpecularMap{ false };//本次绘制的模型所使用的材质是否有镜面反射贴图
			bool			mHasNormalMap{ false };//本次绘制的模型所使用的材质是否有镜面反射贴图
			LightType       mLightType = DIRECTION_LIGHT;
			uint32_t		mDepthPacking{ 0 };

			bool		mDenoise{ false };
			bool		mTaa{ false };
		};

		using Ptr = std::shared_ptr<DriverProgram>;
		static Ptr create(const Parameters::Ptr& parameters) {
			return std::make_shared <DriverProgram>(parameters);
		}

		//1、shader的prefixDefine
		//2、attribute location的替换
		//3、版本扩展等代码的融合
		//4、对最终的shader编译链接
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
		uint32_t	mID{ 0 };//driverProgram 自己的id号
		HashType	mCacheKey{ 0 };//由parameters(上面定义的结构体)参数合集计算出来的hash值
		uint32_t	mRefCount{ 0 };//控制外界有多少引用本Program的renderItem
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
		//key-paramters做成的哈希值，value-用本parameters生成的driverProgram
		std::unordered_map<HashType, DriverProgram::Ptr> mPrograms{};
	};
}