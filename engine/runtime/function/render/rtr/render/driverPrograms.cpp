#include "driverPrograms.h"

namespace ff {

	//1 需要对很多功能进行#define的操作，从而决定打开哪些代码段
	//2 占位字符串的替换,比如POSITION_LOCATION占位字符串替换为0
	DriverProgram::DriverProgram(const Parameters::Ptr& parameters) noexcept {
		//1 shader版本字符串
		std::string versionString = "#version 330 core\n";

		//3 prefix字符串，define的各类操作都会加入到prefix当中，从而决定后续代码当中哪些功能可以被打开
		std::string prefixVertex;
		std::string prefixFragment;

		prefixVertex.append(parameters->mHasNormal ? "#define HAS_NORMAL\n" : "");
		prefixVertex.append(parameters->mHasUV ? "#define HAS_UV\n" : "");
		prefixVertex.append(parameters->mHasColor ? "#define HAS_COLOR\n" : "");

		prefixFragment.append(parameters->mHasNormal ? "#define HAS_NORMAL\n" : "");
		prefixFragment.append(parameters->mHasUV ? "#define HAS_UV\n" : "");
		prefixFragment.append(parameters->mHasColor ? "#define HAS_COLOR\n" : "");
		prefixFragment.append(parameters->mHasDiffuseMap ? "#define HAS_DIFFUSE_MAP\n" : "");
		prefixFragment.append(parameters->mHasEnvCubeMap ? "#define HAS_ENV_MAP\n" : "");
		prefixFragment.append(parameters->mHasSpecularMap ? "#define HAS_SPECULAR_MAP\n" : "");
		prefixFragment.append(parameters->mHasNormalMap ? "#define HAS_NORMAL_MAP\n" : "");


		//4 从parameters里面取出来vs/fs基础功能代码
		auto vertexString = parameters->mVertex;
		auto fragmentString = parameters->mFragment;

		//replaceAttributeLocations(vertexString);
		//replaceAttributeLocations(fragmentString);

		//版本，扩展，前缀prefix（define各种功能的开启）+ 本体shader
		vertexString = versionString  + prefixVertex + vertexString;
		fragmentString = versionString  + prefixFragment + fragmentString;

		auto vertex = vertexString.c_str();
		auto fragment = fragmentString.c_str();

		std::cout << vertex << std::endl;
		std::cout << fragment << std::endl;
		std::cout << "----------------------------------------------------" << std::endl;
		std::cout << std::endl;
		std::cout << std::endl;

		//shader的编译与链接
		uint32_t vertexID = 0, fragID = 0;
		char infoLog[512];
		int  successFlag = 0;

		vertexID = glCreateShader(GL_VERTEX_SHADER);
		glShaderSource(vertexID, 1, &vertex, NULL);
		glCompileShader(vertexID);

		//获取错误信息
		glGetShaderiv(vertexID, GL_COMPILE_STATUS, &successFlag);
		if (!successFlag)
		{
			glGetShaderInfoLog(vertexID, 512, NULL, infoLog);
			std::cout << infoLog << std::endl;
		}

		fragID = glCreateShader(GL_FRAGMENT_SHADER);
		glShaderSource(fragID, 1, &fragment, NULL);
		glCompileShader(fragID);

		glGetShaderiv(fragID, GL_COMPILE_STATUS, &successFlag);
		if (!successFlag)
		{
			glGetShaderInfoLog(fragID, 512, NULL, infoLog);
			std::cout << infoLog << std::endl;
		}

		//链接
		mID = glCreateProgram();
		glAttachShader(mID, vertexID);
		glAttachShader(mID, fragID);
		glLinkProgram(mID);

		glGetProgramiv(mID, GL_LINK_STATUS, &successFlag);
		if (!successFlag)
		{
			glGetProgramInfoLog(mID, 512, NULL, infoLog);
			std::cout << infoLog << std::endl;
		}
		glDeleteShader(vertexID);
		glDeleteShader(fragID);
	}

	DriverProgram::~DriverProgram() noexcept {
		glDeleteProgram(mID);
	}

	void DriverProgram::replaceAttributeLocations(std::string& shader) noexcept {
		//1 通过正则表达式，匹配相应的占位符，比如可以匹配  POSITION_LOCATION
		//2 匹配成功之后，replace功能来将POSITION_LOCATION字符串替换为“0”
		
		//pattern-replace
		std::unordered_map<std::string, std::string> replaceMap = {
			{"POSITION_LOCATION", std::to_string(LOCATION_MAP.at("position"))},
			{"NORMAL_LOCATION", std::to_string(LOCATION_MAP.at("normal"))},
			{"UV_LOCATION", std::to_string(LOCATION_MAP.at("uv"))},
			{"COLOR_LOCATION", std::to_string(LOCATION_MAP.at("color"))},
			{"SKINNING_INDICES_LOCATION", std::to_string(LOCATION_MAP.at("skinIndex"))},
			{"SKINNING_WEIGHTS_LOCATION", std::to_string(LOCATION_MAP.at("skinWeight"))},
			{"TANGENT_LOCATION", std::to_string(LOCATION_MAP.at("tangent"))},
			{"BITANGENT_B_LOCATION", std::to_string(LOCATION_MAP.at("bitangent"))},
			{"PRECOMPUTELT_LOCATION", std::to_string(LOCATION_MAP.at("aPrecomputeLT"))},
		};

		for (const auto& iter : replaceMap) {
			//iter.first = 占位符字符串
			//iter.second = location数字的字符串

			//使用c++的正则表达式进行替换，直接使用占位符的字符串初始化了regex
			std::regex pattern(iter.first);

			//扫描整个shader字符串，只要发现符合pattern的字符串，就要替换为iter.second
			shader = std::regex_replace(shader, pattern, iter.second);
		}
	}


	DriverPrograms::DriverPrograms() noexcept {}

	DriverPrograms::~DriverPrograms() noexcept {}

	DriverProgram::Ptr DriverPrograms::acquireProgram(const DriverProgram::Parameters::Ptr& parameters, HashType cacheKey) noexcept {
		//虽然在当前Material对应的DriverMaterial当中，并没有找到曾经使用过的符合本Paramters的DriverProgram
		//但是从全局角度，其他Material可能曾经使用过符合本Parameters的DriverProgram
		auto iter = mPrograms.find(cacheKey);

		if (iter != mPrograms.end()) {
			return iter->second;
		}

		auto program = DriverProgram::create(parameters);
		program->mCacheKey = cacheKey;
		mPrograms.insert(std::make_pair(cacheKey, program));
		//一旦调用本函数，则外部肯定有一个renderItem需要引用本program
		//可能有多个material都曾经使用过当前这个DriverProgram,假设当前存活两个物体，两个不同的materials。而且假设
		//这两个materials都使用了本DriverProgram，那么RefCount就是2
		program->mRefCount++;

		return program;
	}

	//本函数被调用，意味着外界某个renderItem释放了对本Driverprogram的使用
	void DriverPrograms::release(const DriverProgram::Ptr& program) noexcept {
		if (--program->mRefCount == 0) {
			mPrograms.erase(program->mCacheKey);
		}
	}

	DriverProgram::Parameters::Ptr DriverPrograms::getParameters(
		const Material::Ptr& material,
		const Object3D::Ptr& object,
		std::string vsCode, std::string fsCode
	) {
		auto parameters = DriverProgram::Parameters::create();


		//shaderIter->second 即 shader struct object
		parameters->mVertex = vsCode;
		parameters->mFragment = fsCode;

		if (material == nullptr || object == nullptr)
		{
			return parameters;
		}
		
		auto renderObject = std::static_pointer_cast<RenderableObject>(object);
		auto geometry = renderObject->getGeometry();

		//新建一个parameters
		

		if (geometry->hasAttribute("normal")) {
			parameters->mHasNormal = true;
		}

		if (geometry->hasAttribute("uv")) {
			parameters->mHasUV = true;
		}

		if (geometry->hasAttribute("color")) {
			parameters->mHasColor = true;
		}

		if (material->mDiffuseMap != nullptr) {
			parameters->mHasDiffuseMap = true;
		}

		if (material->mEnvMap != nullptr) {
			parameters->mHasEnvCubeMap = true;
		}

		if (material->mSpecularMap != nullptr) {
			parameters->mHasSpecularMap = true;
		}

		if (material->mNormalMap != nullptr) {
			parameters->mHasNormalMap = true;
		}

		return parameters;
	}

	//将parameters做成字符串，然后进行哈希运算，得到最终的哈希结果
	HashType DriverPrograms::getProgramCacheKey(const DriverProgram::Parameters::Ptr& parameters) noexcept {
		std::hash<std::string> hasher;

		std::string keyString;

		keyString.append(parameters->mVertex);
		keyString.append(parameters->mFragment);
		keyString.append(std::to_string(parameters->mHasNormal));
		keyString.append(std::to_string(parameters->mHasUV));
		keyString.append(std::to_string(parameters->mHasColor));
		keyString.append(std::to_string(parameters->mHasDiffuseMap));
		keyString.append(std::to_string(parameters->mHasEnvCubeMap));
		keyString.append(std::to_string(parameters->mHasSpecularMap));
		keyString.append(std::to_string(parameters->mDepthPacking));

		return hasher(keyString);
	}
}