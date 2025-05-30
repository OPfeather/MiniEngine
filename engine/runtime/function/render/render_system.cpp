#include "runtime/core/base/macro.h"

#include "runtime/resource/asset_manager/asset_manager.h"
#include "runtime/resource/config_manager/config_manager.h"

#include "runtime/function/global/global_context.h"
#include "runtime/function/render/window_system.h"
#include "runtime/function/render/render_camera.h"
#include "runtime/function/render/render_system.h"
#include "runtime/function/render/render_scene.h"
#include "runtime/function/render/render_shader.h"
#include "runtime/function/render/render_model.h"
#include "runtime/function/render/render_canvas.h"
#include "runtime/function/render/render_swap_context.h"
#include "runtime/function/render/render_resource.h"
#include "runtime/function/render/pathtracing/path_tracer.h"
#include "runtime/function/render/rtr/loader/textureLoader.h"

namespace MiniEngine
{
    RenderSystem::~RenderSystem()
    {
        clear();
    }

    void RenderSystem::initialize(RenderSystemInitInfo init_info)
    {
        std::shared_ptr<ConfigManager> config_manager = g_runtime_global_context.m_config_manager;
        ASSERT(config_manager);
        std::shared_ptr<AssetManager> asset_manager = g_runtime_global_context.m_asset_manager;
        ASSERT(asset_manager);

        // configure global opengl state
        glEnable(GL_CULL_FACE);
        glEnable(GL_DEPTH_TEST);
        glEnable(GL_MULTISAMPLE);
        glEnable(GL_FRAMEBUFFER_SRGB);
        glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

        // setup window & viewport
        m_window = init_info.window_system->getWindow();
        std::array<int, 2> window_size = init_info.window_system->getWindowSize();

        // load rendering resource
        GlobalRenderingRes global_rendering_res;
        const std::string &global_rendering_res_url = config_manager->getGlobalRenderingResUrl();
        asset_manager->loadAsset(global_rendering_res_url, global_rendering_res);

        // setup render camera
        const CameraConfig &camera_config = global_rendering_res.m_camera_config;
        const CameraPose &camera_pose = camera_config.m_pose;

        m_render_camera = std::make_shared<Camera>(camera_pose.m_position.to_glm(),
                                                   glm::vec3(0.f, 1.f, 0.f), 
                                                   camera_pose.m_rotation.z, camera_pose.m_rotation.y,
                                                   camera_config.m_aspect.x / camera_config.m_aspect.y, 
                                                   camera_config.m_z_near, camera_config.m_z_far);
        m_viewer_camera = std::make_shared<Camera>(glm::vec3(0.0f, 1.0f, 0.0f));

        // create and setup shader
        m_render_shader = std::make_shared<Shader>((config_manager->getShaderFolder() / "lit.vert").generic_string().data(),
                                                   (config_manager->getShaderFolder() / "lit.frag").generic_string().data());
        m_canvas_shader = std::make_shared<Shader>((config_manager->getShaderFolder() / "unlit.vert").generic_string().data(),
                                                   (config_manager->getShaderFolder() / "unlit.frag").generic_string().data());

        // init path tracer
        m_path_tracer = std::make_shared<PathTracing::PathTracer>();

        m_rtr_secene = ff::Scene::create();
        m_rtr_frustum = ff::Frustum::create();
        m_rtr_shader_programs = ff::DriverPrograms::create();
    }

    void renderSphere(unsigned int &sphereVAO, unsigned int &indexCount)
    {
        if (sphereVAO == 0)
        {
            glGenVertexArrays(1, &sphereVAO);

            unsigned int vbo, ebo;
            glGenBuffers(1, &vbo);
            glGenBuffers(1, &ebo);

            std::vector<glm::vec3> positions;
            std::vector<glm::vec2> uv;
            std::vector<glm::vec3> normals;
            std::vector<unsigned int> indices;

            const unsigned int X_SEGMENTS = 64;
            const unsigned int Y_SEGMENTS = 64;
            const float PI = 3.14159265359f;
            for (unsigned int x = 0; x <= X_SEGMENTS; ++x)
            {
                for (unsigned int y = 0; y <= Y_SEGMENTS; ++y)
                {
                    float xSegment = (float)x / (float)X_SEGMENTS;
                    float ySegment = (float)y / (float)Y_SEGMENTS;
                    float xPos = std::cos(xSegment * 2.0f * PI) * std::sin(ySegment * PI);
                    float yPos = std::cos(ySegment * PI);
                    float zPos = std::sin(xSegment * 2.0f * PI) * std::sin(ySegment * PI);

                    positions.push_back(glm::vec3(xPos, yPos, zPos));
                    uv.push_back(glm::vec2(xSegment, ySegment));
                    normals.push_back(glm::vec3(xPos, yPos, zPos));
                }
            }

            bool oddRow = false;
            for (unsigned int y = 0; y < Y_SEGMENTS; ++y)
            {
                if (!oddRow) // even rows: y == 0, y == 2; and so on
                {
                    for (unsigned int x = 0; x <= X_SEGMENTS; ++x)
                    {
                        indices.push_back(y * (X_SEGMENTS + 1) + x);
                        indices.push_back((y + 1) * (X_SEGMENTS + 1) + x);
                    }
                }
                else
                {
                    for (int x = X_SEGMENTS; x >= 0; --x)
                    {
                        indices.push_back((y + 1) * (X_SEGMENTS + 1) + x);
                        indices.push_back(y * (X_SEGMENTS + 1) + x);
                    }
                }
                oddRow = !oddRow;
            }
            indexCount = static_cast<GLsizei>(indices.size());

            std::vector<float> data;
            for (unsigned int i = 0; i < positions.size(); ++i)
            {
                data.push_back(positions[i].x);
                data.push_back(positions[i].y);
                data.push_back(positions[i].z);
                if (normals.size() > 0)
                {
                    data.push_back(normals[i].x);
                    data.push_back(normals[i].y);
                    data.push_back(normals[i].z);
                }
                if (uv.size() > 0)
                {
                    data.push_back(uv[i].x);
                    data.push_back(uv[i].y);
                }
            }
            glBindVertexArray(sphereVAO);
            glBindBuffer(GL_ARRAY_BUFFER, vbo);
            glBufferData(GL_ARRAY_BUFFER, data.size() * sizeof(float), &data[0], GL_STATIC_DRAW);
            glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
            glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(unsigned int), &indices[0], GL_STATIC_DRAW);
            unsigned int stride = (3 + 2 + 3) * sizeof(float);
            glEnableVertexAttribArray(0);
            glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, stride, (void*)0);
            glEnableVertexAttribArray(1);
            glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, stride, (void*)(3 * sizeof(float)));
            glEnableVertexAttribArray(2);
            glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, stride, (void*)(6 * sizeof(float)));
        }

        glBindVertexArray(sphereVAO);
        glDrawElements(GL_TRIANGLE_STRIP, indexCount, GL_UNSIGNED_INT, 0);
    }

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

    void RenderSystem::rtr_light_model()
    {
        //TODO:根据光源类型绘制灯光模型
        std::shared_ptr<ConfigManager> config_manager = g_runtime_global_context.m_config_manager;
        ASSERT(config_manager);
        //绘制球形灯源
        m_rtr_light_shader = std::make_shared<Shader>((config_manager->getShaderFolder() / "light.vert").generic_string().data(),
                                                   (config_manager->getShaderFolder() / "light.frag").generic_string().data());
        
        m_rtr_light_shader->use();
        glm::mat4 projection = m_render_camera->getPersProjMatrix();
        glm::mat4 view = m_render_camera->getViewMatrix();
        glm::mat4 model = glm::mat4(1.0f);
        model = glm::translate(model, lightPos);
        m_rtr_light_shader->setMat4("projection", projection);
        m_rtr_light_shader->setMat4("view", view);
        m_rtr_light_shader->setMat4("model", model);
        renderSphere(lightVAO, lightIndexCount);
    }

    void RenderSystem::phone_render()
    {
        m_render_shader->use();
        glm::mat4 projection = m_render_camera->getPersProjMatrix();
        glm::mat4 view = m_render_camera->getViewMatrix();
        m_render_shader->setMat4("projection", projection);
        m_render_shader->setMat4("view", view);

        m_render_shader->setVec3("viewPos", m_render_camera->Position);
        m_render_shader->setInt("diffuse_map", 0);
        m_render_shader->setInt("specular_map", 1);
        for (auto obj : m_rtr_secene->mOpaques)
        {
            obj->updateWorldMatrix();
            glm::mat4 model = obj->getWorldMatrix();
            m_render_shader->setMat4("model", model);
            if (obj->getMaterial()->mDiffuseMap)
            {
                glActiveTexture(GL_TEXTURE0);
                glBindTexture(GL_TEXTURE_2D, obj->getMaterial()->mDiffuseMap->mGlTexture);
            }
            if (obj->getMaterial()->mSpecularMap)
            {
                glActiveTexture(GL_TEXTURE1);
                glBindTexture(GL_TEXTURE_2D, obj->getMaterial()->mSpecularMap->mGlTexture);
            }

            obj->getGeometry()->bindVAO();
            auto index = obj->getGeometry()->getIndex();
            auto position = obj->getGeometry()->getAttribute("position");
            if (index)
            {
                glDrawElements(GL_TRIANGLES, index->getCount(), ff::toGL(index->getDataType()), 0);
            }
            else
            {
                glDrawArrays(GL_TRIANGLES, 0, position->getCount());
            }
            glBindVertexArray(0);
        }
        
    }

    void RenderSystem::pcss_shadow_render()
    {
        std::string depth_shader_vs;
        std::string depth_shader_fs;
        std::string pcss_shader_vs;
        std::string pcss_shader_fs;

        get_shader_code(ff::DepthShader, depth_shader_vs, depth_shader_fs);
        get_shader_code(ff::PcssShader, pcss_shader_vs, pcss_shader_fs);
        config_FBO(ff::DepthShader);
        ff::DriverProgram::Ptr depth_shader = nullptr;
        ff::DriverProgram::Ptr pcss_shader = nullptr;

        glm::mat4 lightProjection, lightView;
        glm::mat4 lightSpaceMatrix;
        GLfloat near_plane = 1.0f, far_plane = 100.0f;
        lightProjection = glm::ortho(-50.0f, 50.0f, -50.0f, 50.0f, near_plane, far_plane);
        //lightProjection = glm::perspective(45.0f, (GLfloat)SHADOW_WIDTH / (GLfloat)SHADOW_HEIGHT, near_plane, far_plane); // Note that if you use a perspective projection matrix you'll have to change the light position as the current light position isn't enough to reflect the whole scene.
        lightView = glm::lookAt(lightPos, glm::vec3(0.0f), glm::vec3(0.0, 1.0, 0.0));
        lightSpaceMatrix = lightProjection * lightView;
        // - now render scene from light's point of view
       
        for (auto obj : m_rtr_secene->mOpaques)
        {
            ff::DriverProgram::Parameters::Ptr para = m_rtr_shader_programs->getParameters(
                obj->getMaterial(), obj, depth_shader_vs, depth_shader_fs);
            HashType cacheKey = m_rtr_shader_programs->getProgramCacheKey(para);
            depth_shader = m_rtr_shader_programs->acquireProgram(para, cacheKey);
            
            depth_shader->use();
            depth_shader->setMat4("lightSpaceMatrix", lightSpaceMatrix);
            obj->updateWorldMatrix();
            glm::mat4 model = obj->getWorldMatrix();
            depth_shader->setMat4("model", model);

            obj->getGeometry()->bindVAO();
            auto index = obj->getGeometry()->getIndex();
            auto position = obj->getGeometry()->getAttribute("position");
            if (index)
            {
                glDrawElements(GL_TRIANGLES, index->getCount(), ff::toGL(index->getDataType()), 0);
            }
            else
            {
                glDrawArrays(GL_TRIANGLES, 0, position->getCount());
            }
            glBindVertexArray(0);
        }

        //pass2
        //refreshFrameBuffer();
        // draw models in the scene
        glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
        glViewport(0, 0, m_viewport.width, m_viewport.height);
        glClearColor(0.05f, 0.05f, 0.05f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        for (auto obj : m_rtr_secene->mOpaques)
        {
            ff::DriverProgram::Parameters::Ptr para = m_rtr_shader_programs->getParameters(
                obj->getMaterial(), obj, pcss_shader_vs, pcss_shader_fs);
            HashType cacheKey = m_rtr_shader_programs->getProgramCacheKey(para);
            pcss_shader = m_rtr_shader_programs->acquireProgram(para, cacheKey);
            
            pcss_shader->use();
            glm::mat4 projection = m_render_camera->getPersProjMatrix();
            glm::mat4 view = m_render_camera->getViewMatrix();
            pcss_shader->setMat4("projection", projection);
            pcss_shader->setMat4("view", view);
            // Set light uniforms
            pcss_shader->setVec3("lightPos", lightPos);
            pcss_shader->setVec3("viewPos", m_render_camera->Position);
            pcss_shader->setMat4("lightSpaceMatrix", lightSpaceMatrix);

            pcss_shader->setInt("diffuseTexture", 0);
            pcss_shader->setInt("shadowMap", 1);
            glm::vec3 uLightIntensity = glm::vec3(1.0f, 1.0f, 1.0f);
            pcss_shader->setVec3("uLightIntensity", uLightIntensity);
            
            if (obj->getMaterial()->mDiffuseMap)
            {
                glActiveTexture(GL_TEXTURE0);
                glBindTexture(GL_TEXTURE_2D, obj->getMaterial()->mDiffuseMap->mGlTexture);
            }
            else
            {
                glm::vec3 Kd(obj->getMaterial()->mKd[0], obj->getMaterial()->mKd[1], obj->getMaterial()->mKd[2]);
                pcss_shader->setVec3("Kd", Kd);
            }
            glActiveTexture(GL_TEXTURE1);
            glBindTexture(GL_TEXTURE_2D, depthMap);

            obj->updateWorldMatrix();
            auto model = obj->getWorldMatrix();
            pcss_shader->setMat4("model", model);

            obj->getGeometry()->bindVAO();
            auto index = obj->getGeometry()->getIndex();
            auto position = obj->getGeometry()->getAttribute("position");
            if (index)
            {
                glDrawElements(GL_TRIANGLES, index->getCount(), ff::toGL(index->getDataType()), 0);
            }
            else
            {
                glDrawArrays(GL_TRIANGLES, 0, position->getCount());
            }

            glBindVertexArray(0);

        }
    }

    // 判断两个向量是否近似共线
    bool isNearlyColinear(const glm::vec3& v1, const glm::vec3& v2, float epsilon = 1e-4f) {
        float dot = glm::dot(glm::normalize(v1), glm::normalize(v2));
        return glm::abs(glm::abs(dot) - 1.0f) < epsilon;
    }

    // 安全的 lookAt 实现
    glm::mat4 lookAtSafe(const glm::vec3& eye, const glm::vec3& center, glm::vec3 up) {
        glm::vec3 forward = glm::normalize(center - eye);

        // 如果 forward 和 up 向量几乎共线，选择一个替代的 up 向量
        if (isNearlyColinear(forward, up)) {
            // 选一个和 forward 不共线的新 up
            if (!isNearlyColinear(forward, glm::vec3(0, 0, 1))) {
                up = glm::vec3(0, 0, 1);
            }
            else {
                up = glm::vec3(1, 0, 0); // fallback
            }
        }

        return glm::lookAt(eye, center, up);
    }

    void RenderSystem::ssr_render()
    {
        std::string depth_shader_vs;
        std::string depth_shader_fs;
        std::string gBuffer_shader_vs;
        std::string gBuffer_shader_fs;
        std::string ssr_shader_vs;
        std::string ssr_shader_fs;
        ff::DriverProgram::Ptr depth_shader = nullptr;
        ff::DriverProgram::Ptr gBuffer_shader = nullptr;
        ff::DriverProgram::Ptr ssr_shader = nullptr;

        //psaa1:渲染深度
        get_shader_code(ff::DepthShader, depth_shader_vs, depth_shader_fs);
        config_FBO(ff::DepthShader);
        
        glm::mat4 lightProjection, lightView;
        glm::mat4 lightSpaceMatrix;
        GLfloat near_plane = 1.0f, far_plane = 100.0f;
        lightProjection = glm::ortho(-50.0f, 50.0f, -50.0f, 50.0f, near_plane, far_plane);
        //lightProjection = glm::perspective(45.0f, (GLfloat)SHADOW_WIDTH / (GLfloat)SHADOW_HEIGHT, near_plane, far_plane); // Note that if you use a perspective projection matrix you'll have to change the light position as the current light position isn't enough to reflect the whole scene.
        //lightView = glm::lookAt(lightPos, glm::vec3(0.0f), glm::vec3(0.0, 1.0, 0.0));
        lightView = lookAtSafe(lightPos, glm::vec3(0.0f), glm::vec3(0.0, 1.0, 0.0));
        lightSpaceMatrix = lightProjection * lightView;
        // - now render scene from light's point of view

        for (auto obj : m_rtr_secene->mOpaques)
        {
            ff::DriverProgram::Parameters::Ptr para = m_rtr_shader_programs->getParameters(
                obj->getMaterial(), obj, depth_shader_vs, depth_shader_fs);
            HashType cacheKey = m_rtr_shader_programs->getProgramCacheKey(para);
            depth_shader = m_rtr_shader_programs->acquireProgram(para, cacheKey);

            depth_shader->use();
            depth_shader->setMat4("lightSpaceMatrix", lightSpaceMatrix);
            obj->updateWorldMatrix();
            glm::mat4 model = obj->getWorldMatrix();
            depth_shader->setMat4("model", model);

            obj->getGeometry()->bindVAO();
            auto index = obj->getGeometry()->getIndex();
            auto position = obj->getGeometry()->getAttribute("position");
            if (index)
            {
                glDrawElements(GL_TRIANGLES, index->getCount(), ff::toGL(index->getDataType()), 0);
            }
            else
            {
                glDrawArrays(GL_TRIANGLES, 0, position->getCount());
            }
            glBindVertexArray(0);
        }

        //pass2：渲染gBuffer
        get_shader_code(ff::SsrGbufferShader, gBuffer_shader_vs, gBuffer_shader_fs);
        config_FBO(ff::SsrGbufferShader);
        
        for (auto obj : m_rtr_secene->mOpaques)
        {
            ff::DriverProgram::Parameters::Ptr para = m_rtr_shader_programs->getParameters(
                obj->getMaterial(), obj, gBuffer_shader_vs, gBuffer_shader_fs);
            HashType cacheKey = m_rtr_shader_programs->getProgramCacheKey(para);
            gBuffer_shader = m_rtr_shader_programs->acquireProgram(para, cacheKey);

            gBuffer_shader->use();
            glm::mat4 projection = m_render_camera->getPersProjMatrix();
            glm::mat4 view = m_render_camera->getViewMatrix();
            gBuffer_shader->setMat4("uProjectionMatrix", projection);
            gBuffer_shader->setMat4("uViewMatrix", view);
            obj->updateWorldMatrix();
            auto model = obj->getWorldMatrix();
            gBuffer_shader->setMat4("uModelMatrix", model);
            // Set light uniforms
            gBuffer_shader->setMat4("uLightVP", lightSpaceMatrix);

            if (obj->getMaterial()->mDiffuseMap)
            {
                gBuffer_shader->setInt("udiffuseMap", 0);
                glActiveTexture(GL_TEXTURE0);
                glBindTexture(GL_TEXTURE_2D, obj->getMaterial()->mDiffuseMap->mGlTexture);
            }
            else
            {
                glm::vec3 Kd(obj->getMaterial()->mKd[0], obj->getMaterial()->mKd[1], obj->getMaterial()->mKd[2]);
                gBuffer_shader->setVec3("uKd", Kd);
            }

            if (obj->getMaterial()->mNormalMap)
            {
                gBuffer_shader->setInt("uNormalTexture", 1);
                glActiveTexture(GL_TEXTURE1);
                glBindTexture(GL_TEXTURE_2D, obj->getMaterial()->mNormalMap->mGlTexture);
            }

            gBuffer_shader->setInt("uShadowMap", 2);
            glActiveTexture(GL_TEXTURE2);
            glBindTexture(GL_TEXTURE_2D, depthMap);

            obj->getGeometry()->bindVAO();
            auto index = obj->getGeometry()->getIndex();
            auto position = obj->getGeometry()->getAttribute("position");
            if (index)
            {
                glDrawElements(GL_TRIANGLES, index->getCount(), ff::toGL(index->getDataType()), 0);
            }
            else
            {
                glDrawArrays(GL_TRIANGLES, 0, position->getCount());
            }
            glBindVertexArray(0);

        }

        //pass3:屏幕空间光追
        //refreshFrameBuffer();
        // draw models in the scene
        glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
        glViewport(0, 0, m_viewport.width, m_viewport.height);
        glClearColor(0.05f, 0.05f, 0.05f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        get_shader_code(ff::SsrShader, ssr_shader_vs, ssr_shader_fs);

        for (auto obj : m_rtr_secene->mOpaques)
        {
            ff::DriverProgram::Parameters::Ptr para = m_rtr_shader_programs->getParameters(
                obj->getMaterial(), obj, ssr_shader_vs, ssr_shader_fs);
            HashType cacheKey = m_rtr_shader_programs->getProgramCacheKey(para);
            ssr_shader = m_rtr_shader_programs->acquireProgram(para, cacheKey);

            ssr_shader->use();
            glm::mat4 projection = m_render_camera->getPersProjMatrix();
            glm::mat4 view = m_render_camera->getViewMatrix();
            ssr_shader->setMat4("uProjectionMatrix", projection);
            ssr_shader->setMat4("uViewMatrix", view);
            obj->updateWorldMatrix();
            auto model = obj->getWorldMatrix();
            ssr_shader->setMat4("uModelMatrix", model);
            // Set light uniforms
            ssr_shader->setVec3("uLightDir", lightPos);
            ssr_shader->setVec3("uCameraPos", m_render_camera->Position);
            glm::vec3 lightRadiance(1.0, 1.0, 1.0);
            ssr_shader->setVec3("uLightRadiance", lightRadiance);

            ssr_shader->setInt("uGDiffuse", 0);
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, ssColorMap);
            ssr_shader->setInt("uGDepth", 1);
            glActiveTexture(GL_TEXTURE1);
            glBindTexture(GL_TEXTURE_2D, ssDepthMap);
            ssr_shader->setInt("uGNormalWorld", 2);
            glActiveTexture(GL_TEXTURE2);
            glBindTexture(GL_TEXTURE_2D, ssNormalMap);
            ssr_shader->setInt("uGShadow", 3);
            glActiveTexture(GL_TEXTURE3);
            glBindTexture(GL_TEXTURE_2D, ssVisibilityMap);
            ssr_shader->setInt("uGPosWorld", 4);     
            glActiveTexture(GL_TEXTURE4);
            glBindTexture(GL_TEXTURE_2D, ssWorldPosMap);

            obj->getGeometry()->bindVAO();
            auto index = obj->getGeometry()->getIndex();
            auto position = obj->getGeometry()->getAttribute("position");
            if (index)
            {
                glDrawElements(GL_TRIANGLES, index->getCount(), ff::toGL(index->getDataType()), 0);
            }
            else
            {
                glDrawArrays(GL_TRIANGLES, 0, position->getCount());
            }

            glBindVertexArray(0);

        }
    }

    void RenderSystem::rtr_object()
    {
        //绘制球形灯源

        //透视剪裁
        auto currentViewMatrix = m_render_camera->getPersProjMatrix() * m_render_camera->getViewMatrix();
        m_rtr_frustum->setFromProjectionMatrix(currentViewMatrix);
        
        m_rtr_secene->mOpaques.clear();
        m_rtr_secene->mTransparents.clear();
        //深度贴图需要全场景渲染，这里暂不进行剪裁优化
        projectObject(m_rtr_secene);

        //TODO:排序

        switch (m_rtr_secene->mSceneMaterialType)
        {
        case ff::MeshPcssMaterialType:
            pcss_shadow_render();
            break;
        case ff::SsrMaterialType:
            glEnable(GL_DEPTH_TEST);
            ssr_render();
            break;
        default:
            phone_render();
        } 
    }
    
    void RenderSystem::rtr_scene()
    {
        //渲染光源等场景

        //渲染物体
    }

    void RenderSystem::tick(float delta_time)
    {
        // refresh render target frame buffer
        refreshFrameBuffer();
        

        // draw models in the scene
        glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
        glViewport(0, 0, m_viewport.width, m_viewport.height);
        glClearColor(0.05f, 0.05f, 0.05f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        if (!g_is_editor_mode)
        {
            m_canvas_shader->use();
            m_canvas_shader->setInt("result", 0);
            float image_aspect = (float)m_path_tracer->init_info->Resolution.x / (float)m_path_tracer->init_info->Resolution.y;
            glm::mat4 projection = m_viewer_camera->getOrthoProjMatrix(image_aspect);
            glm::mat4 view = m_viewer_camera->getViewMatrix();
            glm::mat4 model = glm::mat4(1.0f);
            m_canvas_shader->setMat4("projection", projection);
            m_canvas_shader->setMat4("view", view);
            m_canvas_shader->setMat4("model", model);
            m_render_canvas->Draw(m_canvas_shader);
        }
        else if (m_render_model)
        {
            m_render_shader->use();
            glm::mat4 projection = m_render_camera->getPersProjMatrix();
            glm::mat4 view = m_render_camera->getViewMatrix();
            glm::mat4 model = glm::mat4(1.0f);
            m_render_shader->setMat4("projection", projection);
            m_render_shader->setMat4("view", view);
            m_render_shader->setMat4("model", model);
            m_render_shader->setVec3("viewPos", m_render_camera->Position);
            m_render_model->Draw(m_render_shader);
            
        }
        else if (m_rtr_secene->getChildren().size() > 0)
        {
            rtr_object();
        }
        rtr_light_model();

        // draw editor ui
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glViewport(0, 0, m_viewport.width, m_viewport.height);
        glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
        glClear(GL_COLOR_BUFFER_BIT);

        if (m_ui)
        {
            ImGui_ImplOpenGL3_NewFrame();//创建新shader绘制到屏幕上
            ImGui_ImplGlfw_NewFrame();
            ImGui::NewFrame();

            m_ui->preRender();
            ImGui::Render();

            ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        }

        // swap buffers
        glfwSwapBuffers(m_window);
    }

    void RenderSystem::loadScene(char *filepath)
    {
        clear();
        m_render_model = std::make_shared<Model>(filepath);
    }

    void RenderSystem::unloadScene()
    {
        clear();
    }

    void RenderSystem::setupCanvas(float hw, float hh)
    {
        if (m_render_canvas)
            m_render_canvas.reset();
        m_render_canvas = std::make_shared<Canvas>(hw, hh);
    }

    void RenderSystem::refreshFrameBuffer()
    {
        if (framebuffer)
        {
            glDeleteFramebuffers(1, &framebuffer);
            glDeleteTextures(1, &texColorBuffer);
            glDeleteRenderbuffers(1, &texDepthBuffer);
        }

        glGenFramebuffers(1, &framebuffer);
        glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);

        glGenTextures(1, &texColorBuffer);
        glBindTexture(GL_TEXTURE_2D, texColorBuffer);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, m_viewport.width, m_viewport.height, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glBindTexture(GL_TEXTURE_2D, 0);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, texColorBuffer, 0);

        glGenRenderbuffers(1, &texDepthBuffer);
        glBindRenderbuffer(GL_RENDERBUFFER, texDepthBuffer);
        glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, m_viewport.width, m_viewport.height);
        glBindRenderbuffer(GL_RENDERBUFFER, 0);
        glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, texDepthBuffer);

        // if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
        //     LOG_WARN("framebuffer is not complete");

        glBindFramebuffer(GL_FRAMEBUFFER, 0);
    }

    void RenderSystem::clear()
    {
        if (m_render_model)
        {
            m_render_model.reset();
        }
    }

    void RenderSystem::startRendering()
    {
        m_path_tracer->should_stop_tracing = false;
        m_tracing_process = std::thread(&PathTracing::PathTracer::startTracing,m_path_tracer,m_render_model,m_render_camera);
        m_tracing_process.detach();
    };

    void RenderSystem::stopRendering()
    {
        m_path_tracer->should_stop_tracing = true;
        m_path_tracer->state = 0;
    };

    std::shared_ptr<Model> RenderSystem::getRenderModel() const
    {
        return m_render_model;
    }

    std::shared_ptr<Camera> RenderSystem::getRenderCamera() const
    {
        return m_render_camera;
    }

    std::shared_ptr<PathTracing::PathTracer> RenderSystem::getPathTracer() const
    {
        return m_path_tracer;
    }

    void RenderSystem::updateEngineContentViewport(float offset_x, float offset_y, float width, float height)
    {
        m_viewport.x = offset_x;
        m_viewport.y = offset_y;
        m_viewport.width = width;
        m_viewport.height = height;

        m_render_camera->setAspect(width / height);
        m_viewer_camera->setAspect(width / height);
    }

    EngineContentViewport RenderSystem::getEngineContentViewport() const
    {
        return {m_viewport.x, m_viewport.y, m_viewport.width, m_viewport.height};
    }

    void RenderSystem::initializeUIRenderBackend(WindowUI *window_ui)
    {
        m_ui = window_ui;

        ImGui_ImplGlfw_InitForOpenGL(m_window, true);
        ImGui_ImplOpenGL3_Init("#version 330");
    }

    void RenderSystem::projectObject(const ff::Object3D::Ptr& object) noexcept {
        //当前需要被解析的物体，如果是不可见物体，那么连同其子节点一起都变为不可见状态
        if (!object->mVisible) return;

        //如果是可渲染物体
        if (object->mIsRenderableObject) {

            auto renderableObject = std::static_pointer_cast<ff::RenderableObject>(object);

            //首先对object进行一次视景体剪裁测试
            //if (m_rtr_frustum->intersectObject(renderableObject)) {

                m_rtr_secene->mOpaques.push_back(renderableObject);
            //}
        }

        auto children = object->getChildren();
        for (auto& child : children) {
            projectObject(child);
        }
    }

    void RenderSystem::get_shader_code(ff::ShaderType shaderType, string& vs, string& fs) noexcept {
       std::shared_ptr<ConfigManager> config_manager = g_runtime_global_context.m_config_manager;
       ASSERT(config_manager);
       std::shared_ptr<Shader> shader = nullptr;
       string vertexPath;
       string fragmentPath;
       switch(shaderType)
       {
       case ff::DepthShader:
           vertexPath = (config_manager->getShaderFolder() / "shadow_mapping_depth.vs").generic_string();
           fragmentPath = (config_manager->getShaderFolder() / "shadow_mapping_depth.fs").generic_string();
           break;

       case ff::ShadowShader:
           vertexPath = (config_manager->getShaderFolder() / "shadow_mapping.vs").generic_string();
           fragmentPath = (config_manager->getShaderFolder() / "shadow_mapping.fs").generic_string();
           break;

       case ff::PcssShader:
           vertexPath = (config_manager->getShaderFolder() / "pcss_shadow.vs").generic_string();
           fragmentPath = (config_manager->getShaderFolder() / "pcss_shadow.fs").generic_string();
           break;

       case ff::SsrGbufferShader:
           vertexPath = (config_manager->getShaderFolder() / "ssr_gbuffer.vs").generic_string();
           fragmentPath = (config_manager->getShaderFolder() / "ssr_gbuffer.fs").generic_string();
           break;

       case ff::SsrShader:
           vertexPath = (config_manager->getShaderFolder() / "ssr.vs").generic_string();
           fragmentPath = (config_manager->getShaderFolder() / "ssr.fs").generic_string();
           break;
       
       default:
           vertexPath = (config_manager->getShaderFolder() / "lit.vs").generic_string();
           fragmentPath = (config_manager->getShaderFolder() / "lit.fs").generic_string();
            break;
       }

       // 1. retrieve the vertex/fragment source code from filePath
       std::ifstream vShaderFile;
       std::ifstream fShaderFile;
       // ensure ifstream objects can throw exceptions:
       vShaderFile.exceptions(std::ifstream::failbit | std::ifstream::badbit);
       fShaderFile.exceptions(std::ifstream::failbit | std::ifstream::badbit);
       try
       {
           // open files
           vShaderFile.open(vertexPath);
           fShaderFile.open(fragmentPath);
           std::stringstream vShaderStream, fShaderStream;
           // read file's buffer contents into streams
           vShaderStream << vShaderFile.rdbuf();
           fShaderStream << fShaderFile.rdbuf();
           // close file handlers
           vShaderFile.close();
           fShaderFile.close();
           // convert stream into string
           vs = vShaderStream.str();
           fs = fShaderStream.str();
       }
       catch (std::ifstream::failure& e)
       {
           std::cout << "ERROR::SHADER::FILE_NOT_SUCCESFULLY_READ: " << e.what() << std::endl;
       }
    }

    void RenderSystem::config_FBO(ff::ShaderType shaderType) noexcept {
        switch(shaderType)
        {
            case ff::DepthShader:
                if(depthBufferFBO == 0)
                {
                    glGenFramebuffers(1, &depthBufferFBO);
                    // - Create depth texture
                    glGenTextures(1, &depthMap);
                    glBindTexture(GL_TEXTURE_2D, depthMap);
        
                    glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT32F, m_viewport.width*8, m_viewport.height*8, 0, GL_DEPTH_COMPONENT, GL_FLOAT, NULL);
                    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
                    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
                    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
                    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
                    GLfloat borderColor[] = { 1.0, 1.0, 1.0, 1.0 };
                    glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, borderColor);
        
                    glBindFramebuffer(GL_FRAMEBUFFER, depthBufferFBO);
                    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, depthMap, 0);
                    glDrawBuffer(GL_NONE);
                    glReadBuffer(GL_NONE);
                    glBindFramebuffer(GL_FRAMEBUFFER, 0);
                }
                glViewport(0, 0, m_viewport.width * 8, m_viewport.height * 8);
                glBindFramebuffer(GL_FRAMEBUFFER, depthBufferFBO);
                glClear(GL_DEPTH_BUFFER_BIT);
            break;
            case ff::SsrGbufferShader:
                if (gBufferFBO == 0)
                {
                    glGenFramebuffers(1, &gBufferFBO);
                    glBindFramebuffer(GL_FRAMEBUFFER, gBufferFBO);

                    // color + specular color buffer
                    glGenTextures(1, &ssColorMap);
                    glBindTexture(GL_TEXTURE_2D, ssColorMap);
                    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, m_viewport.width, m_viewport.height, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
                    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
                    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
                    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
                    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
                    GLfloat borderColor[] = { 1.0, 1.0, 1.0, 1.0 };
                    glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, borderColor);
                    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, ssColorMap, 0);

                    glGenTextures(1, &ssDepthMap);
                    glBindTexture(GL_TEXTURE_2D, ssDepthMap);
                    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, m_viewport.width, m_viewport.height, 0, GL_RGBA, GL_FLOAT, NULL);
                    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
                    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
                    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
                    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
                    GLfloat depthBorderColor[] = { 1000.0, 1000.0, 1000.0, 1000.0 };
                    glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, depthBorderColor);
                    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1, GL_TEXTURE_2D, ssDepthMap, 0);

                    // normal color buffer
                    glGenTextures(1, &ssNormalMap);
                    glBindTexture(GL_TEXTURE_2D, ssNormalMap);
                    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB16F, m_viewport.width, m_viewport.height, 0, GL_RGB, GL_FLOAT, NULL);
                    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
                    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
                    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
                    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
                    glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, borderColor);
                    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT2, GL_TEXTURE_2D, ssNormalMap, 0);

                    glGenTextures(1, &ssVisibilityMap);
                    glBindTexture(GL_TEXTURE_2D, ssVisibilityMap);
                    glTexImage2D(GL_TEXTURE_2D, 0, GL_R16F, m_viewport.width, m_viewport.height, 0, GL_RED, GL_FLOAT, NULL);
                    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
                    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
                    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
                    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
                    glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, borderColor);
                    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT3, GL_TEXTURE_2D, ssVisibilityMap, 0);
                    
                    glGenTextures(1, &ssWorldPosMap);
                    glBindTexture(GL_TEXTURE_2D, ssWorldPosMap);
                    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB32F, m_viewport.width, m_viewport.height, 0, GL_RGB, GL_FLOAT, NULL);
                    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
                    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
                    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
                    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
                    glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, borderColor);
                    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT4, GL_TEXTURE_2D, ssWorldPosMap, 0);

                    unsigned int attachments[5] = { GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1, GL_COLOR_ATTACHMENT2, GL_COLOR_ATTACHMENT3, GL_COLOR_ATTACHMENT4 };
                    glDrawBuffers(5, attachments);

                    //必须添加深度缓冲，否则不会进行深度测试
                    glGenRenderbuffers(1, &gBufferRboDepth);
                    glBindRenderbuffer(GL_RENDERBUFFER, gBufferRboDepth);
                    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT, m_viewport.width, m_viewport.height);
                    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, gBufferRboDepth);
                    
                    // tell OpenGL which color attachments we'll use (of this framebuffer) for rendering 
                    
                    // finally check if framebuffer is complete
                    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
                        std::cout << "Framebuffer not complete!" << std::endl;
                        GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
                        std::cout << "FBO Status: 0x" << std::hex << status << std::endl;
                    }
                        
                }
                glViewport(0, 0, m_viewport.width, m_viewport.height);
                glBindFramebuffer(GL_FRAMEBUFFER, gBufferFBO);
                
                glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
                break;
        }
        
        
    }

    void RenderSystem::rtr_process_floor(glm::vec3 pos, ff::MaterialType materialType) {
        if (m_rtr_base_env.isRenderFloor)
        {
            if (m_rtr_base_env.floor)
            {
                m_rtr_base_env.floor->setPosition(pos.x, pos.y, pos.z);
            }
            else
            {
                m_rtr_base_env.floorGeometry = ff::BoxGeometry::create(50.0, 1.0, 50.0);
                m_rtr_base_env.floorMaterial = ff::Material::create();
                //TODO:根据材质加载数据(或者固定材质，但是渲染和灯光一样用单独的shader)
                m_rtr_base_env.floorMaterial->mDiffuseMap = ff::TextureLoader::load("E:/myProject/gameEngine/PiccoloRenderEngine/MiniEngine/engine/editor/demo/texture/concreteTexture.png");
                m_rtr_base_env.floor = ff::Mesh::create(m_rtr_base_env.floorGeometry, m_rtr_base_env.floorMaterial);
                m_rtr_base_env.floor->setPosition(pos.x, pos.y, pos.z);
                m_rtr_base_env.floorGeometry->createVAO();
                m_rtr_base_env.floorGeometry->bindVAO();
                m_rtr_base_env.floorGeometry->setupVertexAttributes();
                m_rtr_secene->addChild(m_rtr_base_env.floor);
            }

        }
        else
        {
            auto& render_objs = m_rtr_secene->getChildren();
            ff::Object3D::Ptr floor = m_rtr_base_env.floor;
            render_objs.erase(
                std::remove(render_objs.begin(), render_objs.end(), floor),
                render_objs.end());
            m_rtr_base_env.floor = nullptr;
        }
    }

} // namespace MiniEngine
