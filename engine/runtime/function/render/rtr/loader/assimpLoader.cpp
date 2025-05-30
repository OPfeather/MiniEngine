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
		std::cerr << "�޷����ļ�: " << filename << std::endl;
		return false;
	}

	prtLi.clear();
	prtLi.resize(3); // �����ļ�����3������

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

// ��ָ���п�ʼ����ȡn�����ݣ����浽vector<float>
std::vector<float> readNLinesFromFile(const std::string& filename, int n, int& lastReadLine) {
	std::ifstream file(filename);
	std::vector<float> data;

	if (!file.is_open()) {
		std::cerr << "�޷����ļ�: " << filename << std::endl;
		return data;
	}

	std::string line;
	int startLine = 0;

	// ��һ������ʼ�кţ���0��ʼ������
	if (std::getline(file, line)) {
		std::istringstream ss(line);
		ss >> startLine;
	}
	else {
		std::cerr << "�ļ�Ϊ�ջ��޷���ȡ��һ��" << std::endl;
		return data;
	}

	// ������ʼ�к�ǰ����
	for (int i = 0; i < startLine; ++i) {
		if (!std::getline(file, line)) {
			std::cerr << "�ļ����������޷�������ʼ��" << std::endl;
			return data;
		}
	}

	// ��ȡn������
	for (int i = 0; i < n; ++i) {
		if (std::getline(file, line)) {
			std::stringstream ss(line);
			float value;
			while (ss >> value) {
				data.push_back(value);
			}
			lastReadLine = startLine + i + 1; // ��������ȡ����
		}
		else {
			std::cerr << "���ݲ��㣬ʵ��ֻ��ȡ�� " << i << " ��" << std::endl;
			break;
		}
	}

	file.close();
	return data;
}

// �����ļ��ĵ�һ�У������µ�����
void updateFirstLine(const std::string& filename, int newFirstLine) {
	std::ifstream fileIn(filename);
	std::ofstream fileOut("temp.txt");

	if (!fileIn.is_open() || !fileOut.is_open()) {
		std::cerr << "�޷����ļ�����д�����" << std::endl;
		return;
	}

	// ���µ�һ��Ϊ�µ�����
	fileOut << newFirstLine << std::endl;

	// ��ʣ������ݸ��Ƶ����ļ�
	std::string line;
	std::getline(fileIn, line); // ����ԭ��һ��
	while (std::getline(fileIn, line)) {
		fileOut << line << std::endl;
	}

	fileIn.close();
	fileOut.close();

	// �� temp.txt �滻ԭ�ļ�
	std::remove(filename.c_str());
	std::rename("temp.txt", filename.c_str());
}

namespace ff {

	AssimpResult::Ptr AssimpLoader::load(const std::string& path, MaterialType materialType) noexcept {
		AssimpResult::Ptr result = AssimpResult::create();

		//��ǰģ������Mesh�õ���material�����¼���������������棬˳����aiScene���
		//mMaterials��˳����ͬ
		std::vector<Material::Ptr> materials;

		//���ɸ��ڵ�
		Object3D::Ptr rootObject = Group::create();

		//��ʼ���ж�ȡ
		Assimp::Importer importer;
		const aiScene* scene = importer.ReadFile(
			path, 
			aiProcess_Triangulate | aiProcess_FlipUVs | aiProcess_CalcTangentSpace);

		if (!scene || scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE || !scene->mRootNode)
		{
			std::cout << "Error:model read fail!" << std::endl;
			return nullptr;
		}

		//ģ�Ͷ�ȡ��pathһ���������ģ�assets/models/superMan/man.fbx
		//ȡ������·����assets/models/superMan/
		std::size_t lastIndex = path.find_last_of("\\");
		std::string rootPath = path.substr(0, lastIndex + 1);

		processMaterial(scene, rootPath, materials, materialType);
		processNode(scene->mRootNode, scene, rootObject, materials);

		result->mObject = rootObject;
		return result;
	}

	//1 ����ÿ��Node�������Mesh���ͽ�������Mesh�����Ҽ��뵽��Node��Ӧ��Group��������
	//2 ���ϵͳû�ж������ұ�Nodeû��Mesh����LocalTransform���õ���Ӧ��Group��localMatrix����
	//3 ����㼶�ܹ�
	//
	void AssimpLoader::processNode(
		const aiNode* node,
		const aiScene* scene,
		Object3D::Ptr parentObject,
		const std::vector<Material::Ptr>& materials) {

		//make a group for all the meshes in the node
		Group::Ptr group = Group::create();
		for (uint32_t i = 0; i < node->mNumMeshes; ++i) {
			//���ڵ�ǰnode�ĵ�i��Mesh��ȡ������MeshID��node->mMeshes[i]���������MeshID��Scene��������aiMesh���õ���������
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
		//ѭ������aiScene�����ÿһ��material
		for (uint32_t id = 0; id < scene->mNumMaterials; ++id) {
			Material::Ptr material{ nullptr };

			aiMaterial* aimaterial = scene->mMaterials[id];

			//������ȡ��ǰ��Material��ʲô����
			aiShadingMode shadingMode{ aiShadingMode::aiShadingMode_Phong };

			aimaterial->Get(AI_MATKEY_SHADING_MODEL, shadingMode);

			//todo: we may need more material model
			//��������ֻʵ����MeshPhongmaterial����ģ��ͨ�õĲ��ʣ�����һ��д��PhongMaterial
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

			//��ʼ��ȡÿ��Material����ͼ����
			material->mDiffuseMap = processTexture(aiTextureType_DIFFUSE, scene, aimaterial, rootPath);

			material->mNormalMap = processTexture(aiTextureType_NORMALS, scene, aimaterial, rootPath);
					
			material->mSpecularMap = processTexture(aiTextureType_SPECULAR, scene, aimaterial, rootPath);

			//��ȡ��������ɫ��û��DiffuseMapʱʹ�ã�
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
		//���ڵ�ǰģ�ͣ����ǹ�������洢��·�������ǣ�assets/models/superMan/man.fbx
		//ͼƬ���ݿ��ܻ����ڣ�assets/models/superMan/textures/�ļ�����
		//�·��õ���aiPath��Ҫ��ȡ��ͼƬ�����·���������man.fbx
		material->Get(AI_MATKEY_TEXTURE(type, 0), aiPath);

		if (!aiPath.length) {
			return nullptr;
		}

		//�е�ģ�ͣ��������һ������ģ���ڲ�����û�е�����š�
		//�鿴���ڵ�ǰ��aiPath��Ӧ��ͼƬ�������Ƿ�������ִ����ģ���ڲ������
		const aiTexture* assimpTexture = scene->GetEmbeddedTexture(aiPath.C_Str());
		if (assimpTexture) {
			//���ȷʵͼƬ�������ģ���ڲ��������������ȡ����aiTexture����ͺ�����ͼƬ����
			unsigned char* dataIn = reinterpret_cast<unsigned char*>(assimpTexture->pcData);
			uint32_t widthIn = assimpTexture->mWidth;
			uint32_t heightIn = assimpTexture->mHeight;
			std::string path = aiPath.C_Str();

			return TextureLoader::load(path, dataIn, widthIn, heightIn);
		}
		//��ΪaiPath��textures/diffuseTexture.jpg
		//ƴװ���ɣ�assets/models/superMan/textures/diffuseTexture.jpg

		std::string fullPath = rootPath + aiPath.C_Str();
		return TextureLoader::load(fullPath);
	}

	//1 �����ǰ������Mesh�յ��˹�����Ӱ�죬��ô����һ��SkinnedMesh
	//2 ����յ��˹���Ӱ�죬��ô��Attribute����Ҫ����skinIdex�Լ�skinWeight
	Object3D::Ptr AssimpLoader::processMesh(
		const aiMesh* mesh,
		const aiScene* scene,
		//��ǰMesh������Node���������һ��mesh�����޷�����LocalMatrix
		const glm::mat4 localTransform,
		const std::vector<Material::Ptr>& materials) {

		Object3D::Ptr object = nullptr;
		Material::Ptr material = nullptr;
		Geometry::Ptr geometry = Geometry::create();

		std::vector<glm::mat4> offsetMatrices;//��Ӧ���������ÿһ��bone��offsetMatrix

		//һ��Mesh����Ҫ������attributes
		std::vector<float> positions;
		std::vector<float> normals;
		std::vector<float> tangents;
		std::vector<float> bitangents;

		std::vector<std::vector<float>> uvs;//ÿһ��Ԫ�ض��ǵ�i��channel��������������
		std::vector<uint32_t> numUVComponents;//ÿһ��Ԫ�ض��ǵ�i��channel��itemSize

		std::vector<uint32_t> indices;

		//���ն�����������
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
			//1 һ��ģ�Ϳ������е�mesh���ж����ͼ����ͼ���ܻ�����ͬ���ܣ�����DiffuseMap�����ж��ţ���ô��ɫ�ͻ���
			//Ҳ�п����ǲ�ͬ���ܣ�����NormalMap SpecularMap DiffuseMap��
			// 2 ��Ȼ�ж�����ͼ���ܣ�ͬһ�����������ͬ���������ܻ��в�ͬ��uv����
			// 3 ����������в�ͬ���ͣ����������λͼƬ�����Ǽ򵥵Ķ�λuv���������������ͼuvw��str��
			// 
			// �ܽ᣺����ͬһ��Mesh����ȡ��ĳ�������uv
			// 1 ���ж���uv���ֲ��ڲ�ͬ��Channel
			// 2 ��ȡ���������ʱ��Ҫ�ж���uv����uvw
			//

			//GetNumUVChannels:��ȡ��ǰMesh�ж�������������
			//����ͬ��Channel��������������˲�ͬ��vector<float>����
			for (uint32_t u = 0; u < mesh->GetNumUVChannels(); ++u) {
				if (u >= uvs.size()) {
					uvs.push_back(std::vector<float>());
				}
				std::vector<float>& uvComponents = uvs[u];

				//�鿴���ڵ�ǰ���Channel������������uv����uvw
				//mNumUVComponents �洢�˵�ǰ��u��Channel����Ӧ����һ����������itemSize
				uint32_t numComponents = mesh->mNumUVComponents[u];

				//uv  or  uvw ��������ԭ�����������ǰ������������Ϊattribute����geometry���ڹ���
				//attribute��ʱ�򣬾���Ҫ֪��itemSize
				if (u >= numUVComponents.size()) {
					numUVComponents.push_back(numComponents);
				}

				//����numComponents���б�����ȡ��Ҫôѭ��2�μ�uv��Ҫôѭ��3�μ�uvw
				for (uint32_t c = 0; c < numComponents; c++) {
					//mTextureCoords�洢�����е�������������
					//u�����ŵ�u��channel
					//i�����˶�ȡ��i�����������
					//c�����˵�c��������������
					uvComponents.push_back(mesh->mTextureCoords[u][i][c]);
				}
			}
		}

		//��ȡ��ǰMesh��Index����
		//��aimesh���棬ÿһ�������ζ���һ��Face���������е�Face������indexȡ������
		for (uint32_t f = 0; f < mesh->mNumFaces; f++)
		{
			aiFace	face = mesh->mFaces[f];

			for (uint32_t id = 0; id < face.mNumIndices; id++)
			{
				//����ÿһ��Face��ÿһ�������ID
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

		//geometry������key-value��key����attribute�����֣�value����attribute����
		//��diffuseMap��uv��Ȼ����uv
		//�ӵڶ����������꿪ʼ��������uv1 uv2.....
		std::string uvName;

		//ѭ��ÿһ����������
		for (uint32_t uvId = 0; uvId < uvs.size(); ++uvId) {
			uvName = "uv";
			if (uvId) {
				uvName += std::to_string(uvId);
			}
	
			//uvName : uv  uv1  uv2 uv3....��Ŀǰ������ϵͳ��֧��uv����û����չ����uv�����
			//ȡ��uvs[uvId]����uvId��uv����vector<float>
			//numUVComponents[uvId]����uvId��uv��itemSize��������2 ������3
			geometry->setAttribute(uvName, Attributef::create(uvs[uvId], numUVComponents[uvId]));
		}

		geometry->setIndex(Attributei::create(indices, 1));

		//process material
		//��aiMesh���棬�����õ���ǰMesh��ʹ�õ�Material��index����Ӧ��aiScene->mMaterials����ı��
		//�Ѿ������õ�materials�����aiScene->mMaterials��һһ��Ӧ��
		if (mesh->mMaterialIndex >= 0) {
			material = materials[mesh->mMaterialIndex];
		}

		//ע�⣡�����û�ж���Ӱ�죬��ʹ�õ�ǰ�ڵ��Transform������Ϊ��localMatrix
		object = Mesh::create(geometry, material);
		object->setLocalMatrix(localTransform);
		
		geometry->createVAO();
		geometry->bindVAO();
		geometry->setupVertexAttributes();

		// ��ȡ����
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

	//ϵ�й��ߺ���������������Ԫ���������assimp�ĸ�ʽ����ת��Ϊglm�ĸ�ʽ
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