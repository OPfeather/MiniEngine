#include "assimpLoader.h"
#include "../objects/mesh.h"
#include "../objects/group.h"
#include "../material/meshPhongMaterial.h"
#include "../material/meshPcssMaterial.h"
#include "../material/cubeMaterial.h"
#include "../material/depthMaterial.h"
#include "../material/meshBasicMaterial.h"
#include "../material/kullaContyMaterial.h"
#include "../loader/textureLoader.h"
#include "../loader/cache.h"

bool readLightFileByColumn(const std::string& filename, std::vector<std::vector<float>>& prtLi) {
	std::ifstream file(filename);
	if (!file.is_open()) {
		std::cerr << "无法打开文件: " << filename << std::endl;
		return false;
	}

	prtLi.clear();
	prtLi.resize(3); // 假设文件中有3列数据

	std::string line;
	while (std::getline(file, line)) {
		std::istringstream iss(line);
		float val1, val2, val3;
		if (iss >> val1 >> val2 >> val3) {
			prtLi[0].push_back(val1);
			prtLi[1].push_back(val2);
			prtLi[2].push_back(val3);
		}
	}

	file.close();
	return true;
}

// 从指定行开始，读取n行数据，保存到vector<float>
std::vector<float> readNLinesFromFile(const std::string& filename, int n, int& lastReadLine) {
	std::ifstream file(filename);
	std::vector<float> data;

	if (!file.is_open()) {
		std::cerr << "无法打开文件: " << filename << std::endl;
		return data;
	}

	std::string line;
	int startLine = 0;

	// 第一行是起始行号（从0开始索引）
	if (std::getline(file, line)) {
		std::istringstream ss(line);
		ss >> startLine;
	}
	else {
		std::cerr << "文件为空或无法读取第一行" << std::endl;
		return data;
	}

	// 跳过起始行号前的行
	for (int i = 0; i < startLine; ++i) {
		if (!std::getline(file, line)) {
			std::cerr << "文件不够长，无法跳到起始行" << std::endl;
			return data;
		}
	}

	// 读取n行数据
	for (int i = 0; i < n; ++i) {
		if (std::getline(file, line)) {
			std::stringstream ss(line);
			float value;
			while (ss >> value) {
				data.push_back(value);
			}
			lastReadLine = startLine + i + 1; // 更新最后读取行数
		}
		else {
			std::cerr << "数据不足，实际只读取了 " << i << " 行" << std::endl;
			break;
		}
	}

	file.close();
	return data;
}

// 更新文件的第一行，设置新的行数
void updateFirstLine(const std::string& filename, int newFirstLine) {
	std::ifstream fileIn(filename);
	std::ofstream fileOut("temp.txt");

	if (!fileIn.is_open() || !fileOut.is_open()) {
		std::cerr << "无法打开文件进行写入操作" << std::endl;
		return;
	}

	// 更新第一行为新的行数
	fileOut << newFirstLine << std::endl;

	// 将剩余的数据复制到新文件
	std::string line;
	std::getline(fileIn, line); // 跳过原第一行
	while (std::getline(fileIn, line)) {
		fileOut << line << std::endl;
	}

	fileIn.close();
	fileOut.close();

	// 用 temp.txt 替换原文件
	std::remove(filename.c_str());
	std::rename("temp.txt", filename.c_str());
}

namespace ff {

	AssimpResult::Ptr AssimpLoader::load(const std::string& path, MaterialType materialType) noexcept {
		AssimpResult::Ptr result = AssimpResult::create();

		//当前模型所有Mesh用到的material都会记录在这样的数组里面，顺序按照aiScene里的
		//mMaterials的顺序相同
		std::vector<Material::Ptr> materials;

		//生成根节点
		Object3D::Ptr rootObject = Group::create();

		//开始进行读取
		Assimp::Importer importer;
		const aiScene* scene = importer.ReadFile(
			path, 
			aiProcess_Triangulate | aiProcess_FlipUVs | aiProcess_CalcTangentSpace);

		if (!scene || scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE || !scene->mRootNode)
		{
			std::cout << "Error:model read fail!" << std::endl;
			return nullptr;
		}

		//模型读取的path一般是这样的：assets/models/superMan/man.fbx
		//取出来根路径：assets/models/superMan/
		std::size_t lastIndex = path.find_last_of("\\");
		std::string rootPath = path.substr(0, lastIndex + 1);

		processMaterial(scene, rootPath, materials, materialType);
		processNode(scene->mRootNode, scene, rootObject, materials);

		result->mObject = rootObject;
		return result;
	}

	//1 解析每个Node，如果有Mesh，就解析生成Mesh，并且加入到本Node对应的Group对象里面
	//2 如果系统没有动画并且本Node没有Mesh，则将LocalTransform设置到对应的Group的localMatrix上面
	//3 建设层级架构
	//
	void AssimpLoader::processNode(
		const aiNode* node,
		const aiScene* scene,
		Object3D::Ptr parentObject,
		const std::vector<Material::Ptr>& materials) {

		//make a group for all the meshes in the node
		Group::Ptr group = Group::create();
		for (uint32_t i = 0; i < node->mNumMeshes; ++i) {
			//对于当前node的第i个Mesh，取出来其MeshID（node->mMeshes[i]），用这个MeshID向Scene里面索引aiMesh，拿到具体数据
			aiMesh* mesh = scene->mMeshes[node->mMeshes[i]];
			group->addChild(processMesh(mesh, scene, getGLMMat4(node->mTransformation), materials));
		}

		parentObject->addChild(group);

		for (uint32_t i = 0; i < node->mNumChildren; i++)
		{
			processNode(node->mChildren[i], scene, group, materials);
		}
	}

	void AssimpLoader::processMaterial(
		const aiScene* scene,
		const std::string& rootPath,
		std::vector<Material::Ptr>& materials, MaterialType materialType)
	{
		//循环解析aiScene里面的每一个material
		for (uint32_t id = 0; id < scene->mNumMaterials; ++id) {
			Material::Ptr material{ nullptr };

			aiMaterial* aimaterial = scene->mMaterials[id];

			//用来获取当前的Material是什么类型
			aiShadingMode shadingMode{ aiShadingMode::aiShadingMode_Phong };

			aimaterial->Get(AI_MATKEY_SHADING_MODEL, shadingMode);

			//todo: we may need more material model
			//由于我们只实现了MeshPhongmaterial这种模型通用的材质，所以一律写成PhongMaterial
			switch (materialType) {
			case MaterialType::MeshPhongMaterialType:
				material = MeshPhongMaterial::create();
				break;
			case MaterialType::KullaContyMaterialType: {
				auto kullaContyMaterial = KullaContyMaterial::create();
				kullaContyMaterial->mMetallic = 1.0;
				kullaContyMaterial->mRoughness = 0.95;
				kullaContyMaterial->mBRDFLut = ff::TextureLoader::load("assets/models/ball/GGX_E_LUT.png", nullptr, 0, 0, true);
				//kullaContyMaterial->mBRDFLut = ff::TextureLoader::load("assets/models/ball/GGX_E_MC_LUT.png");
				kullaContyMaterial->mEavgLut = ff::TextureLoader::load("assets/models/ball/GGX_Eavg_LUT.png", nullptr, 0, 0, true);
				material = kullaContyMaterial;
				break;
			}
			case MaterialType::MeshPcssMaterialType:
				material = MeshPcssMaterial::create();
				break;
			default:
				material = MeshPhongMaterial::create();
				break;
			}

			//开始读取每个Material的贴图数据
			material->mDiffuseMap = processTexture(aiTextureType_DIFFUSE, scene, aimaterial, rootPath);

			material->mNormalMap = processTexture(aiTextureType_NORMALS, scene, aimaterial, rootPath);
					
			material->mSpecularMap = processTexture(aiTextureType_SPECULAR, scene, aimaterial, rootPath);

			//读取漫反射颜色（没有DiffuseMap时使用）
			aiColor3D kdColor(0.f, 0.f, 0.f);
			if (aimaterial->Get(AI_MATKEY_COLOR_DIFFUSE, kdColor) == AI_SUCCESS) {
				material->mKd[0] = kdColor.r;
				material->mKd[1] = kdColor.g;
				material->mKd[2] = kdColor.b;
			}
			else {
				std::cout << "No diffuse color (Kd) found." << std::endl;
			}
			
			materials.push_back(material);
		}
	}

	Texture::Ptr AssimpLoader::processTexture(
		const aiTextureType& type,
		const aiScene* scene,
		const aiMaterial* material,
		const std::string& rootPath) {
		//for texture
		aiString aiPath;

		//for now we only need one texture per type, without texture blending
		//todo: multi-texture blending
		//对于当前模型，我们工程里面存储的路径可能是：assets/models/superMan/man.fbx
		//图片数据可能会存放在：assets/models/superMan/textures/文件夹下
		//下方拿到的aiPath是要读取的图片的相对路径，相对于man.fbx
		material->Get(AI_MATKEY_TEXTURE(type, 0), aiPath);

		if (!aiPath.length) {
			return nullptr;
		}

		//有的模型，会把纹理一起打包在模型内部，并没有单独存放。
		//查看对于当前的aiPath对应的图片来讲，是否存在这种打包在模型内部的情况
		const aiTexture* assimpTexture = scene->GetEmbeddedTexture(aiPath.C_Str());
		if (assimpTexture) {
			//如果确实图片打包在了模型内部，则上述代码获取到的aiTexture里面就含有了图片数据
			unsigned char* dataIn = reinterpret_cast<unsigned char*>(assimpTexture->pcData);
			uint32_t widthIn = assimpTexture->mWidth;
			uint32_t heightIn = assimpTexture->mHeight;
			std::string path = aiPath.C_Str();

			return TextureLoader::load(path, dataIn, widthIn, heightIn);
		}
		//因为aiPath是textures/diffuseTexture.jpg
		//拼装后变成：assets/models/superMan/textures/diffuseTexture.jpg

		std::string fullPath = rootPath + aiPath.C_Str();
		return TextureLoader::load(fullPath);
	}

	//1 如果当前解析的Mesh收到了骨骼的影响，那么就是一个SkinnedMesh
	//2 如果收到了骨骼影响，那么其Attribute就需要加入skinIdex以及skinWeight
	Object3D::Ptr AssimpLoader::processMesh(
		const aiMesh* mesh,
		const aiScene* scene,
		//当前Mesh所属的Node如果有哪怕一个mesh，就无法设置LocalMatrix
		const glm::mat4 localTransform,
		const std::vector<Material::Ptr>& materials) {

		Object3D::Ptr object = nullptr;
		Material::Ptr material = nullptr;
		Geometry::Ptr geometry = Geometry::create();

		std::vector<glm::mat4> offsetMatrices;//对应上述数组的每一个bone的offsetMatrix

		//一个Mesh所需要的所有attributes
		std::vector<float> positions;
		std::vector<float> normals;
		std::vector<float> tangents;
		std::vector<float> bitangents;

		std::vector<std::vector<float>> uvs;//每一个元素都是第i个channel的纹理坐标数据
		std::vector<uint32_t> numUVComponents;//每一个元素都是第i个channel的itemSize

		std::vector<uint32_t> indices;

		//按照顶点来遍历的
		for (uint32_t i = 0; i < mesh->mNumVertices; ++i) {
			positions.push_back(mesh->mVertices[i].x);
			positions.push_back(mesh->mVertices[i].y);
			positions.push_back(mesh->mVertices[i].z);

			if (mesh->mNormals && mesh->mNormals->Length())
			{
				normals.push_back(mesh->mNormals[i].x);
				normals.push_back(mesh->mNormals[i].y);
				normals.push_back(mesh->mNormals[i].z);
			}

			if (mesh->mTangents && mesh->mTangents->Length()) {
				tangents.push_back(mesh->mTangents[i].x);
				tangents.push_back(mesh->mTangents[i].y);
				tangents.push_back(mesh->mTangents[i].z);
			}

			if (mesh->mBitangents && mesh->mBitangents->Length()) {
				bitangents.push_back(mesh->mBitangents[i].x);
				bitangents.push_back(mesh->mBitangents[i].y);
				bitangents.push_back(mesh->mBitangents[i].z);
			}

			//may have multi-textures, u is the number
			//1 一个模型可能其中的mesh会有多个贴图，贴图可能会有相同功能，比如DiffuseMap可能有多张，那么颜色就会混合
			//也有可能是不同功能，比如NormalMap SpecularMap DiffuseMap。
			// 2 既然有多张贴图可能，同一个顶点采样不同的纹理，可能会有不同的uv坐标
			// 3 纹理坐标会有不同类型，如果采样二位图片，就是简单的二位uv，如果采样环境贴图uvw（str）
			// 
			// 总结：对于同一个Mesh，读取其某个顶点的uv
			// 1 会有多套uv，分布在不同的Channel
			// 2 读取纹理坐标的时候，要判断是uv还是uvw
			//

			//GetNumUVChannels:获取当前Mesh有多少套纹理坐标
			//将不同的Channel的纹理坐标存在了不同的vector<float>里面
			for (uint32_t u = 0; u < mesh->GetNumUVChannels(); ++u) {
				if (u >= uvs.size()) {
					uvs.push_back(std::vector<float>());
				}
				std::vector<float>& uvComponents = uvs[u];

				//查看对于当前这个Channel其纹理坐标是uv还是uvw
				//mNumUVComponents 存储了当前第u个Channel所对应的这一套纹理坐标itemSize
				uint32_t numComponents = mesh->mNumUVComponents[u];

				//uv  or  uvw 存下来的原因，是如果将当前的纹理坐标作为attribute传入geometry，在构建
				//attribute的时候，就需要知道itemSize
				if (u >= numUVComponents.size()) {
					numUVComponents.push_back(numComponents);
				}

				//按照numComponents进行遍历读取，要么循环2次即uv，要么循环3次即uvw
				for (uint32_t c = 0; c < numComponents; c++) {
					//mTextureCoords存储着所有的纹理坐标数据
					//u代表着第u个channel
					//i代表了读取第i个顶点的数据
					//c代表了第c个纹理坐标数据
					uvComponents.push_back(mesh->mTextureCoords[u][i][c]);
				}
			}
		}

		//读取当前Mesh的Index数据
		//在aimesh里面，每一个三角形都是一个Face，遍历所有的Face，将其index取出保存
		for (uint32_t f = 0; f < mesh->mNumFaces; f++)
		{
			aiFace	face = mesh->mFaces[f];

			for (uint32_t id = 0; id < face.mNumIndices; id++)
			{
				//推入每一个Face的每一个顶点的ID
				indices.push_back(face.mIndices[id]);
			}
		}

		int lastReadLine = 1;
		
		if (mesh->mMaterialIndex >= 0 && materials[mesh->mMaterialIndex]->mIsPrtMaterial) {
			std::vector<float> aPrecomputeLT = readNLinesFromFile("assets/textures/GraceCathedral/transport.txt", mesh->mNumVertices, lastReadLine);
			updateFirstLine("assets/textures/GraceCathedral/transport.txt", lastReadLine - 1);
			geometry->setAttribute("aPrecomputeLT", ff::Attributef::create(aPrecomputeLT, 3));
		}

		//make object
		geometry->setAttribute("position", Attributef::create(positions, 3));
		geometry->setAttribute("normal", Attributef::create(normals, 3));

		//TBN
		if (tangents.size()) {
			geometry->setAttribute("tangent", Attributef::create(tangents, 3));
		}

		if (bitangents.size()) {
			geometry->setAttribute("bitangent", Attributef::create(bitangents, 3));
		}

		//geometry里面是key-value，key就是attribute的名字，value就是attribute对象
		//主diffuseMap的uv仍然叫做uv
		//从第二套纹理坐标开始，都叫做uv1 uv2.....
		std::string uvName;

		//循环每一套纹理坐标
		for (uint32_t uvId = 0; uvId < uvs.size(); ++uvId) {
			uvName = "uv";
			if (uvId) {
				uvName += std::to_string(uvId);
			}
	
			//uvName : uv  uv1  uv2 uv3....。目前的引擎系统仅支持uv，还没有扩展多套uv的情况
			//取出uvs[uvId]：第uvId套uv数据vector<float>
			//numUVComponents[uvId]：第uvId套uv的itemSize，可能是2 可能是3
			geometry->setAttribute(uvName, Attributef::create(uvs[uvId], numUVComponents[uvId]));
		}

		geometry->setIndex(Attributei::create(indices, 1));

		//process material
		//从aiMesh里面，可以拿到当前Mesh所使用的Material的index，对应着aiScene->mMaterials数组的编号
		//已经解析好的materials数组跟aiScene->mMaterials是一一对应的
		if (mesh->mMaterialIndex >= 0) {
			material = materials[mesh->mMaterialIndex];
		}

		//注意！！如果没有动画影响，则使用当前节点的Transform矩阵作为其localMatrix
		object = Mesh::create(geometry, material);
		object->setLocalMatrix(localTransform);
		
		geometry->createVAO();
		geometry->bindVAO();
		geometry->setupVertexAttributes();

		// 获取材质
    if (mesh->mMaterialIndex >= 0)
    {
        aiMaterial* material = scene->mMaterials[mesh->mMaterialIndex];
        
        aiColor3D kdColor(0.f, 0.f, 0.f);
        if (material->Get(AI_MATKEY_COLOR_DIFFUSE, kdColor) == AI_SUCCESS)
        {
            std::cout << "Diffuse Color (Kd): "
                      << kdColor.r << ", "
                      << kdColor.g << ", "
                      << kdColor.b << std::endl;
        }
    }

		return object;
	}

	//系列工具函数，将向量、四元数、矩阵从assimp的格式里面转化为glm的格式
	glm::vec3 AssimpLoader::getGLMVec3(aiVector3D value) noexcept
	{
		return glm::vec3(value.x, value.y, value.z);
	}

	glm::quat AssimpLoader::getGLMQuat(aiQuaternion value) noexcept
	{
		return glm::quat(value.w, value.x, value.y, value.z);
	}

	glm::mat4 AssimpLoader::getGLMMat4(aiMatrix4x4 value) noexcept
	{
		glm::mat4 to;
		//the a,b,c,d in assimp is the row ; the 1,2,3,4 is the column
		to[0][0] = value.a1; to[1][0] = value.a2; to[2][0] = value.a3; to[3][0] = value.a4;
		to[0][1] = value.b1; to[1][1] = value.b2; to[2][1] = value.b3; to[3][1] = value.b4;
		to[0][2] = value.c1; to[1][2] = value.c2; to[2][2] = value.c3; to[3][2] = value.c4;
		to[0][3] = value.d1; to[1][3] = value.d2; to[2][3] = value.d3; to[3][3] = value.d4;

		return to;
	}
}