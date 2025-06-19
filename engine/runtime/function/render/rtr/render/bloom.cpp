#include "bloom.h"

namespace ff {
    // renderQuad() renders a 1x1 XY quad in NDC
    // -----------------------------------------
    unsigned int quadVAO = 0;
    unsigned int quadVBO;
    void renderQuad()
    {
        if (quadVAO == 0)
        {
            float quadVertices[] = {
                // positions        // texture Coords
                -1.0f,  1.0f, 0.0f, 0.0f, 1.0f,
                -1.0f, -1.0f, 0.0f, 0.0f, 0.0f,
                1.0f,  1.0f, 0.0f, 1.0f, 1.0f,
                1.0f, -1.0f, 0.0f, 1.0f, 0.0f,
            };
            // setup plane VAO
            glGenVertexArrays(1, &quadVAO);
            glGenBuffers(1, &quadVBO);
            glBindVertexArray(quadVAO);
            glBindBuffer(GL_ARRAY_BUFFER, quadVBO);
            glBufferData(GL_ARRAY_BUFFER, sizeof(quadVertices), &quadVertices, GL_STATIC_DRAW);
            glEnableVertexAttribArray(0);
            glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)0);
            glEnableVertexAttribArray(1);
            glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)(3 * sizeof(float)));
        }
        glBindVertexArray(quadVAO);
        glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
        glBindVertexArray(0);
    }

    bloomFBO::bloomFBO() : mInit(false) {}
    bloomFBO::~bloomFBO() {}

    bool bloomFBO::Init(unsigned int windowWidth, unsigned int windowHeight, unsigned int mipChainLength)
    {
        if (mInit) return true;

        glGenFramebuffers(1, &mFBO);
        glBindFramebuffer(GL_FRAMEBUFFER, mFBO);

        glm::vec2 mipSize((float)windowWidth, (float)windowHeight);
        glm::ivec2 mipIntSize((int)windowWidth, (int)windowHeight);
        // Safety check
        if (windowWidth > (unsigned int)INT_MAX || windowHeight > (unsigned int)INT_MAX) {
            std::cerr << "Window size conversion overflow - cannot build bloom FBO!" << std::endl;
            return false;
        }

        for (GLuint i = 0; i < mipChainLength; i++)
        {
            bloomMip mip;

            mipSize *= 0.5f;
            mipIntSize /= 2;
            mip.size = mipSize;
            mip.intSize = mipIntSize;

            glGenTextures(1, &mip.texture);
            glBindTexture(GL_TEXTURE_2D, mip.texture);
            // we are downscaling an HDR color buffer, so we need a float texture format
            glTexImage2D(GL_TEXTURE_2D, 0, GL_R11F_G11F_B10F,
                (int)mipSize.x, (int)mipSize.y,
                0, GL_RGB, GL_FLOAT, nullptr);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

            std::cout << "Created bloom mip " << mipIntSize.x << 'x' << mipIntSize.y << std::endl;
            mMipChain.emplace_back(mip);
        }

        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
            GL_TEXTURE_2D, mMipChain[0].texture, 0);

        // setup attachments
        unsigned int attachments[1] = { GL_COLOR_ATTACHMENT0 };
        glDrawBuffers(1, attachments);

        // check completion status
        int status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
        if (status != GL_FRAMEBUFFER_COMPLETE)
        {
            printf("gbuffer FBO error, status: 0x%x\n", status);
            glBindFramebuffer(GL_FRAMEBUFFER, 0);
            return false;
        }

        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        mInit = true;
        return true;
    }

    void bloomFBO::Destroy()
    {
        for (int i = 0; i < (int)mMipChain.size(); i++) {
            glDeleteTextures(1, &mMipChain[i].texture);
            mMipChain[i].texture = 0;
        }
        glDeleteFramebuffers(1, &mFBO);
        mFBO = 0;
        mInit = false;
        mMipChain.clear();
    }

    void bloomFBO::BindForWriting()
    {
        glBindFramebuffer(GL_FRAMEBUFFER, mFBO);
    }

    const std::vector<bloomMip>& bloomFBO::MipChain() const
    {
        return mMipChain;
    }


    BloomRenderer::BloomRenderer() : mInit(false), mUpdate(false) {}
    BloomRenderer::~BloomRenderer() {}

    bool BloomRenderer::Init(unsigned int windowWidth, unsigned int windowHeight, DriverProgram::Ptr upsampleShader, DriverProgram::Ptr downsampleShader)
    {
        if (mInit) return true;
        mSrcViewportSize = glm::ivec2(windowWidth, windowHeight);
        mSrcViewportSizeFloat = glm::vec2((float)windowWidth, (float)windowHeight);

        // Framebuffer
        const unsigned int num_bloom_mips = 6; // TODO: Play around with this value
        bool status = mFBO.Init(windowWidth, windowHeight, num_bloom_mips);
        if (!status) {
            std::cerr << "Failed to initialize bloom FBO - cannot create bloom renderer!\n";
            return false;
        }

        // Shaders
        mDownsampleShader = downsampleShader;
        mUpsampleShader = upsampleShader;

        // Downsample
        mDownsampleShader->use();
        mDownsampleShader->setInt("uSrcTexture", 0);
        glUseProgram(0);

        // Upsample
        mUpsampleShader->use();
        mUpsampleShader->setInt("uSrcTexture", 0);
        glUseProgram(0);

        return true;
    }

    bool BloomRenderer::UpdateBloom(unsigned int windowWidth, unsigned int windowHeight)
    {
        if(mUpdate)
        {
            Destroy();
            mSrcViewportSize = glm::ivec2(windowWidth, windowHeight);
            mSrcViewportSizeFloat = glm::vec2((float)windowWidth, (float)windowHeight);

            // Framebuffer
            const unsigned int num_bloom_mips = 6; // TODO: Play around with this value
            bool status = mFBO.Init(windowWidth, windowHeight, num_bloom_mips);
            if (!status) {
                std::cerr << "Failed to initialize bloom FBO - cannot create bloom renderer!\n";
                return false;
            }
            mUpdate = false;
        }
        return true;
    }

    void BloomRenderer::Destroy()
    {
        mFBO.Destroy();
    }

    void BloomRenderer::RenderDownsamples(unsigned int srcTexture)
    {
        const std::vector<bloomMip>& mipChain = mFBO.MipChain();

        mDownsampleShader->use();
        mDownsampleShader->setFloat("uScreenWidth", mSrcViewportSizeFloat.x);
        mDownsampleShader->setFloat("uScreenHeight", mSrcViewportSizeFloat.y);
        if (mKarisAverageOnDownsample) {
            mDownsampleShader->setInt("uMipLevel", 0);
        }

        // Bind srcTexture (HDR color buffer) as initial texture input
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, srcTexture);

        // Progressively downsample through the mip chain
        for (int i = 0; i < (int)mipChain.size(); i++)
        {
            const bloomMip& mip = mipChain[i];
            glViewport(0, 0, mip.size.x, mip.size.y);
            glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                GL_TEXTURE_2D, mip.texture, 0);

            // Render screen-filled quad of resolution of current mip
            renderQuad();

            // Set current mip resolution as srcResolution for next iteration
            mDownsampleShader->setFloat("uScreenWidth", mip.size.x);
            mDownsampleShader->setFloat("uScreenHeight", mip.size.y);
            // Set current mip as texture input for next iteration
            glBindTexture(GL_TEXTURE_2D, mip.texture);
            // Disable Karis average for consequent downsamples
            if (i == 0) { mDownsampleShader->setInt("uMipLevel", 1); }
        }

        glUseProgram(0);
    }

    void BloomRenderer::RenderUpsamples(float filterRadius)
    {
        const std::vector<bloomMip>& mipChain = mFBO.MipChain();

        mUpsampleShader->use();
        //mUpsampleShader->setFloat("uFilterRadius", filterRadius);

        // Enable additive blending
        glEnable(GL_BLEND);
        glBlendFunc(GL_ONE, GL_ONE);
        glBlendEquation(GL_FUNC_ADD);

        for (int i = (int)mipChain.size() - 1; i > 0; i--)
        {
            const bloomMip& mip = mipChain[i];
            const bloomMip& nextMip = mipChain[i - 1];

            mUpsampleShader->setFloat("uFilterRadius", 1 / mip.size.x);
            mUpsampleShader->setFloat("uAspectRatio", mip.size.x / mip.size.y);

            // Bind viewport and texture from where to read
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, mip.texture);

            // Set framebuffer render target (we write to this texture)
            glViewport(0, 0, nextMip.size.x, nextMip.size.y);
            glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                GL_TEXTURE_2D, nextMip.texture, 0);

            // Render screen-filled quad of resolution of current mip
            renderQuad();
        }

        // Disable additive blending
        //glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
        glDisable(GL_BLEND);

        glUseProgram(0);
    }

    void BloomRenderer::RenderBloomTexture(unsigned int srcTexture, float filterRadius)
    {
        mFBO.BindForWriting();

        this->RenderDownsamples(srcTexture);
        this->RenderUpsamples(filterRadius);

        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        // Restore viewport
        glViewport(0, 0, mSrcViewportSize.x, mSrcViewportSize.y);
    }

    GLuint BloomRenderer::BloomTexture()
    {
        return mFBO.MipChain()[0].texture;
    }

    GLuint BloomRenderer::BloomMip_i(int index)
    {
        const std::vector<bloomMip>& mipChain = mFBO.MipChain();
        int size = (int)mipChain.size();
        return mipChain[(index > size - 1) ? size - 1 : (index < 0) ? 0 : index].texture;
    }

}
