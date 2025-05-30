#include "texture.h"
#include "../tools/identity.h"
#include "../global/eventDispatcher.h"

namespace ff {

	Texture::Texture(
		const uint32_t& width,
		const uint32_t& height,
		const DataType& dataType,
		const TextureWrapping& wrapS,
		const TextureWrapping& wrapT,
		const TextureWrapping& wrapR,
		const TextureFilter& magFilter,
		const TextureFilter& minFilter,
		const TextureFormat& format
	) noexcept {
		mID = Identity::generateID();
		mWidth = width;
		mHeight = height;
		mDataType = dataType;
		mWrapS = wrapS;
		mWrapR = wrapR;
		mWrapT = wrapT;
		mMagFilter = magFilter;
		mMinFilter = minFilter;
		mFormat = format;
		mTextureType = TextureType::Texture2D;
	}

	Texture::~Texture() noexcept {
		//������ʱ��ͨ��dispatcher���ⷢ����texture��������Ϣ
		EventBase::Ptr e = EventBase::create("textureDispose");
		e->mTarget = this;
		EventDispatcher::getInstance()->dispatchEvent(e);

		if (mSource) {
			EventBase::Ptr e = EventBase::create("sourceRelease");
			e->mTarget = mSource.get();
			EventDispatcher::getInstance()->dispatchEvent(e);
		}
	}

	Texture::Ptr Texture::clone() noexcept {
		auto texture = Texture::create(mWidth, mHeight, mDataType, mWrapS, mWrapT, mWrapR, mMagFilter, mMinFilter, mFormat);
		texture->mSource = mSource;
		texture->mUsage = mUsage;
		texture->mTextureType = mTextureType;
		texture->mInternalFormat = mInternalFormat;

		return texture;
	}

	void Texture::textureSet() noexcept {

		if (!mGlTexture) {
			glGenTextures(1, &mGlTexture);
		}
		glBindTexture(toGL(mTextureType), mGlTexture);

		//�����������
		glTexParameteri(toGL(mTextureType), GL_TEXTURE_MIN_FILTER, toGL(mMinFilter));
		glTexParameteri(toGL(mTextureType), GL_TEXTURE_MAG_FILTER, toGL(mMagFilter));
		glTexParameteri(toGL(mTextureType), GL_TEXTURE_WRAP_S, toGL(mWrapS));
		glTexParameteri(toGL(mTextureType), GL_TEXTURE_WRAP_T, toGL(mWrapT));
		glTexParameteri(toGL(mTextureType), GL_TEXTURE_WRAP_R, toGL(mWrapR));

		if (mTextureType == TextureType::Texture2D) {
			//��������ͼר�õ�texture��������ȾĿ�꣬�ſ�����ͼƬ����
			const unsigned char* data = (mUsage == TextureUsage::SamplerTexture) ? mSource->mData.data() : nullptr;

			//1 �����ڴ�ռ�
			//2 ����ͼƬ����
			//��������������OpenGL����ϣ����������Ϊ���ָ�ʽ�����ͼ��ֻ��RGBֵ���Ͱ�������ΪRGBֵ��
			//���ߵڰ˸�����������Դͼ�ĸ�ʽ���������͡�����ʹ��RGBֵ�������ͼ�񣬲������Ǵ���Ϊchar(byte)����
			glTexImage2D(GL_TEXTURE_2D, 0, toGL(mInternalFormat), mWidth, mHeight, 0, toGL(mFormat), toGL(mDataType), data);
			
			glGenerateMipmap(GL_TEXTURE_2D);
		}
		// else {
		// 	//Ϊ��ǰ��cubeMap��texture�������ڴ濪���Լ����ݸ���
		// 	for (uint32_t i = 0; i < CubeTexture::CUBE_TEXTURE_COUNT; ++i) {
		// 		auto cubeTexture = std::static_pointer_cast<CubeTexture>(texture);
		// 		const byte* data = (texture->getUsage() == TextureUsage::SamplerTexture) ? cubeTexture->mSources[i]->mData.data() : nullptr;

		// 		//�����ڴ漰�������ݵ�˳����������ǰ��
		// 		//Ҫ����һ���濪���ڴ�������ݣ���������һ�����target
		// 		glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0, toGL(texture->mInternalFormat), texture->mWidth, texture->mHeight, 0, toGL(texture->mFormat), toGL(texture->mDataType), data);
		// 	}
		// }

		glBindTexture(toGL(mTextureType), 0);

		return;
	}

}