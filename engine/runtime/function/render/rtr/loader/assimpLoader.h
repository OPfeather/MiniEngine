#pragma once 
#include "../global/base.h"
#include "../objects/renderableObject.h"
#include "assimp/Importer.hpp"
#include "assimp/scene.h"
#include "assimp/postprocess.h"
#include "../material/material.h"
#include "../textures/texture.h"


namespace ff {

	struct AssimpResult {
		using Ptr = std::shared_ptr<AssimpResult>;
		static Ptr create() {
			return std::make_shared<AssimpResult>();
		}

		//最终解析完毕之后的RootNode
		Object3D::Ptr	mObject{ nullptr };
	};

	class AssimpLoader {
	public:
		AssimpLoader() noexcept {}

		~AssimpLoader() noexcept {}

		static AssimpResult::Ptr load(const std::string& path, MaterialType materialType = MaterialType::MeshPhongMaterialType) noexcept;

	private:
		static void processNode(
			const aiNode* node,
			const aiScene* scene,
			Object3D::Ptr parentObject,
			const std::vector<Material::Ptr>& materials);

		static void processMaterial(
			const aiScene* scene,
			const std::string& rootPath,
			std::vector<Material::Ptr>& materials, MaterialType materialType = MaterialType::MeshPhongMaterialType);

		static Texture::Ptr processTexture(
			const aiTextureType& type,
			const aiScene* scene,
			const aiMaterial* material,
			const std::string& rootPath);

		static Object3D::Ptr processMesh(
			const aiMesh* mesh,
			const aiScene* scene,
			const glm::mat4 localTransform,
			const std::vector<Material::Ptr>& material);

		static glm::vec3 getGLMVec3(aiVector3D value) noexcept;

		static glm::quat getGLMQuat(aiQuaternion value) noexcept;

		static glm::mat4 getGLMMat4(aiMatrix4x4 value) noexcept;
	};
}