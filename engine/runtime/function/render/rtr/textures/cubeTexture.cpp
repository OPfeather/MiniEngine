#include "cubeTexture.h"
#include "../global/eventDispatcher.h"

namespace ff {

	CubeTexture::CubeTexture(
		const uint32_t& width,
		const uint32_t& height,
		const DataType& dataType,
		const TextureWrapping& wrapS,
		const TextureWrapping& wrapT,
		const TextureWrapping& wrapR,
		const TextureFilter& magFilter,
		const TextureFilter& minFilter,
		const TextureFormat& format
	) noexcept: Texture(width, height, dataType, wrapS, wrapT, wrapR, magFilter, minFilter, format) {
		mTextureType = TextureType::TextureCubeMap;
	}

	CubeTexture::~CubeTexture() noexcept {
		for (uint32_t i = 0; i < CUBE_TEXTURE_COUNT; ++i) {
			auto source = mSources[i];
			if (source) {
				EventBase::Ptr e = EventBase::create("sourceRelease");
				e->mTarget = source.get();
				EventDispatcher::getInstance()->dispatchEvent(e);
			}
		}
	}

	void CubeTexture::setCubeTexture(){
		if(mGlTexture == 0)
		{
			glGenTextures(1, &mGlTexture);
			glBindTexture(GL_TEXTURE_CUBE_MAP, mGlTexture);

			int width, height, nrChannels;
			for (unsigned int i = 0; i < CUBE_TEXTURE_COUNT; i++)
			{
				const unsigned char* data = mSources[i]->mData.data();
				glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0, toGL(mInternalFormat), mWidth, mHeight, 0, toGL(mFormat), toGL(mDataType), data);
			}
			glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
			glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
			glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
			glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
			glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
			glGenerateMipmap(GL_TEXTURE_CUBE_MAP);
		}
	}
}