#include "textureLoader.h"
#include <stb_image.h>
#include "../global/config.h"
#include "cache.h"

namespace ff {

	TextureLoader::TextureLoader() noexcept {}

	TextureLoader::~TextureLoader() noexcept {}

	Texture::Ptr TextureLoader::load(const std::string& path, unsigned char* dataIn, uint32_t widthIn, uint32_t heightIn, bool is_flip) {
		Texture::Ptr texture = nullptr;
		std::string filePath = path;

		//���·��Ϊ�գ���ʹ��Ĭ��ͼƬ
		if (filePath.empty()) {
			filePath = DefaultTexturePath;
		}

		//����Ƿ��Ѿ����ɹ�source����������˾ʹ�cache����ȡ����
		Source::Ptr source = Cache::getInstance()->getSource(path);

		if (source) {
			texture = Texture::create(source->mWidth, source->mHeight);
			texture->mSource = source;
		}
		else {
			source = Source::create();
			//�������ݶ���������,����false��
			source->mNeedsUpdate = false;

			//ʹ���������ͣ�����ֱ�Ӷ�data���и��ģ������ͬ����source��Data����
			auto& data = source->mData;

			int			picType = 0;
			int width = 0, height = 0;

			//������ȡ������ͼƬ���ݴ�С
			uint32_t dataSize{ 0 };

			//��ȡ������ͼƬ����ָ��
			unsigned char* bits{ nullptr };

			//Ҫô��Ӳ�̶�ȡ��Ҫô�����������������ݻ���ģ�����ݣ���ȡ
			if (dataIn == nullptr) {
				//if nofile, use default
				std::fstream file(filePath);
				if (!file.is_open()) {
					filePath = DefaultTexturePath;
				}
				else {
					file.close();
				}

				if (is_flip)
				{
					stbi_set_flip_vertically_on_load(true);
				}
				
				bits = stbi_load(filePath.c_str(), &width, &height, &picType, toStbImageFormat(TextureFormat::RGBA));
				stbi_set_flip_vertically_on_load(false);
			}
			else {
				//��¼���������ݵĴ�С
				uint32_t dataInSize = 0;

				//һ��fbxģ���п��ܴ������jpg������ѹ����ʽ��ͼƬ����£�height����Ϊ0��width�ʹ���������ͼƬ�Ĵ�С
				if (!heightIn) {
					dataInSize = widthIn;
				}
				else {
					dataInSize = widthIn * heightIn;
				}

				//���������õ���dataIn��������չ����λͼ���ݣ��п�����һ��jpg png�ȸ�ʽ��ͼƬ������
				bits = stbi_load_from_memory(dataIn, dataInSize, &width, &height, &picType, toStbImageFormat(TextureFormat::RGBA));
			}

			dataSize = width * height * toByteSize(TextureFormat::RGBA);

			//�����������̣�����׼���������еı�Ҫ���ݣ����������source��data(Vector<Byte>)
			if (dataSize && bits) {
				data.resize(dataSize);

				//��bits��data�ĵ�ַ��ͷ������dataSize��Byte������
				memcpy(data.data(), bits, dataSize);
			}

			//��ʱ��bitsָ�����stbimage�������ڴ����ݣ���delete��
			stbi_image_free(bits);

			source->mWidth = width;
			source->mHeight = height;
			texture = Texture::create(source->mWidth, source->mHeight);
		}

		texture->mSource = source;
		Cache::getInstance()->cacheSource(filePath, source);
		texture->textureSet();
		return texture;
	}
}