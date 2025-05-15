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
		//消亡的时候，通过dispatcher向外发出本texture消亡的消息
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

		//设置纹理参数
		glTexParameteri(toGL(mTextureType), GL_TEXTURE_MIN_FILTER, toGL(mMinFilter));
		glTexParameteri(toGL(mTextureType), GL_TEXTURE_MAG_FILTER, toGL(mMagFilter));
		glTexParameteri(toGL(mTextureType), GL_TEXTURE_WRAP_S, toGL(mWrapS));
		glTexParameteri(toGL(mTextureType), GL_TEXTURE_WRAP_T, toGL(mWrapT));
		glTexParameteri(toGL(mTextureType), GL_TEXTURE_WRAP_R, toGL(mWrapR));

		if (mTextureType == TextureType::Texture2D) {
			//必须是贴图专用的texture而不是渲染目标，才可能有图片数据
			const unsigned char* data = (mUsage == TextureUsage::SamplerTexture) ? mSource->mData.data() : nullptr;

			//1 开辟内存空间
			//2 传输图片数据
			//第三个参数告诉OpenGL我们希望把纹理储存为何种格式。如果图像只有RGB值，就把纹理储存为RGB值。
			//第七第八个参数定义了源图的格式和数据类型。比如使用RGB值加载这个图像，并把它们储存为char(byte)数组
			glTexImage2D(GL_TEXTURE_2D, 0, toGL(mInternalFormat), mWidth, mHeight, 0, toGL(mFormat), toGL(mDataType), data);
			
			glGenerateMipmap(GL_TEXTURE_2D);
		}
		// else {
		// 	//为当前的cubeMap的texture做六次内存开辟以及数据更新
		// 	for (uint32_t i = 0; i < CubeTexture::CUBE_TEXTURE_COUNT; ++i) {
		// 		auto cubeTexture = std::static_pointer_cast<CubeTexture>(texture);
		// 		const byte* data = (texture->getUsage() == TextureUsage::SamplerTexture) ? cubeTexture->mSources[i]->mData.data() : nullptr;

		// 		//开辟内存及更新数据的顺序：右左上下前后
		// 		//要给哪一个面开辟内存更新数据，就输入哪一个面的target
		// 		glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0, toGL(texture->mInternalFormat), texture->mWidth, texture->mHeight, 0, toGL(texture->mFormat), toGL(texture->mDataType), data);
		// 	}
		// }

		glBindTexture(toGL(mTextureType), 0);

		return;
	}

}