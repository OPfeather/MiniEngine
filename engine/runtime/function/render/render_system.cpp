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
        //glEnable(GL_CULL_FACE);
        glEnable(GL_DEPTH_TEST);
        //glEnable(GL_MULTISAMPLE);
        //glEnable(GL_FRAMEBUFFER_SRGB);
        glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
        glEnable(GL_TEXTURE_CUBE_MAP_SEAMLESS);

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
        m_rtr_base_env.light = ff::Light::create(m_rtr_base_env.lightPos);
    }

    void RenderSystem::renderQuad()
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
        std::shared_ptr<ConfigManager> config_manager = g_runtime_global_context.m_config_manager;
        ASSERT(config_manager);

        if (m_rtr_light_shader == nullptr)
        {
            m_rtr_light_shader = std::make_shared<Shader>((config_manager->getShaderFolder() / "light.vert").generic_string().data(),
                (config_manager->getShaderFolder() / "light.frag").generic_string().data());
        }
        m_rtr_light_shader->use();
        glm::mat4 projection = m_render_camera->getPersProjMatrix();
        glm::mat4 view = m_render_camera->getViewMatrix();
        glm::mat4 model = glm::mat4(1.0f);
        glm::vec3& lightRot = m_rtr_base_env.light->rotation;
        glm::quat quatX = glm::angleAxis(glm::radians(lightRot.x), glm::vec3(1.0f, 0.0f, 0.0f));
        glm::quat quatY = glm::angleAxis(glm::radians(lightRot.y), glm::vec3(0.0f, 1.0f, 0.0f));
        glm::quat quatZ = glm::angleAxis(glm::radians(lightRot.z), glm::vec3(0.0f, 0.0f, 1.0f));
        glm::quat finalQuat = quatX * quatY * quatZ; // 顺序敏感
        glm::mat4 rotationMatrix = glm::mat4_cast(finalQuat);
        glm::mat4 translateMatrix = glm::translate(glm::mat4(1.0f), m_rtr_base_env.lightPos);
        model = translateMatrix * rotationMatrix;
        m_rtr_light_shader->setMat4("projection", projection);
        m_rtr_light_shader->setMat4("view", view);
        m_rtr_light_shader->setMat4("model", model);
        m_rtr_base_env.light->light_shape_render();
     
    }

    void RenderSystem::rtr_skybox()
    {
        std::shared_ptr<ConfigManager> config_manager = g_runtime_global_context.m_config_manager;
        ASSERT(config_manager);
        if (m_rtr_base_env.isRenderSkyBox)
        {
            if (m_rtr_skybox_shader == nullptr)
            {
                m_rtr_skybox_shader = std::make_shared<Shader>((config_manager->getShaderFolder() / "background.vs").generic_string().data(),
                    (config_manager->getShaderFolder() / "background.fs").generic_string().data());
            }
            glDepthFunc(GL_LEQUAL);// change depth function so depth test passes when values are equal to depth buffer's content
            m_rtr_skybox_shader->use();
            glm::mat4 projection = m_render_camera->getPersProjMatrix();
            glm::mat4 view = glm::mat4(glm::mat3(m_render_camera->getViewMatrix()));
            m_rtr_skybox_shader->setMat4("projection", projection);
            m_rtr_skybox_shader->setMat4("view", view);
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_CUBE_MAP, m_rtr_base_env.skyBox->mGlTexture);
            m_rtr_skybox_shader->setInt("environmentMap", 0);
            glBindVertexArray(m_rtr_base_env.skyBoxVAO); 
            glDrawArrays(GL_TRIANGLES, 0, 36);
            glBindVertexArray(0);
            glDepthFunc(GL_LESS);// set depth function back to default
        }
    }

    void RenderSystem::phone_render()
    {
        //m_render_shader->use();
        //glm::mat4 projection = m_render_camera->getPersProjMatrix();
        //glm::mat4 view = m_render_camera->getViewMatrix();
        //m_render_shader->setMat4("projection", projection);
        //m_render_shader->setMat4("view", view);

        //m_render_shader->setVec3("viewPos", m_render_camera->Position);
        //m_render_shader->setInt("diffuse_map", 0);
        //m_render_shader->setInt("specular_map", 1);
        //for (auto obj : m_rtr_secene->mOpaques)
        //{
        //    obj->updateWorldMatrix();
        //    glm::mat4 model = obj->getWorldMatrix();
        //    m_render_shader->setMat4("model", model);
        //    if (obj->getMaterial()->mDiffuseMap)
        //    {
        //        glActiveTexture(GL_TEXTURE0);
        //        glBindTexture(GL_TEXTURE_2D, obj->getMaterial()->mDiffuseMap->mGlTexture);
        //    }
        //    if (obj->getMaterial()->mSpecularMap)
        //    {
        //        glActiveTexture(GL_TEXTURE1);
        //        glBindTexture(GL_TEXTURE_2D, obj->getMaterial()->mSpecularMap->mGlTexture);
        //    }

        //    obj->getGeometry()->bindVAO();
        //    auto index = obj->getGeometry()->getIndex();
        //    auto position = obj->getGeometry()->getAttribute("position");
        //    if (index)
        //    {
        //        glDrawElements(GL_TRIANGLES, index->getCount(), ff::toGL(index->getDataType()), 0);
        //    }
        //    else
        //    {
        //        glDrawArrays(GL_TRIANGLES, 0, position->getCount());
        //    }
        //    glBindVertexArray(0);
        //}

        std::string depth_shader_vs;
        std::string depth_shader_fs;
        std::string phong_shader_vs;
        std::string phong_shader_fs;
        ff::DriverProgram::Ptr depth_shader = nullptr;
        ff::DriverProgram::Ptr phong_shader = nullptr;
        ff::DriverProgram::Parameters::Ptr para = nullptr;
        HashType cacheKey = 0;

        //psaa1:渲染深度
        get_shader_code(ff::DepthShader, depth_shader_vs, depth_shader_fs);
        config_FBO(ff::DepthShader);

        m_rtr_base_env.light->updateViewMatrix();
        glm::mat4 lightSpaceMatrix = m_rtr_base_env.light->getProjectionMatrix() * m_rtr_base_env.light->getViewMatrix();
        // - now render scene from light's point of view

        for (auto obj : m_rtr_secene->mOpaques)
        {
            para = m_rtr_shader_programs->getParameters(
                obj->getMaterial(), obj, m_rtr_base_env.light->mType, depth_shader_vs, depth_shader_fs);
            cacheKey = m_rtr_shader_programs->getProgramCacheKey(para);
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

        //pass2：渲染物体
        // draw models in the scene
        glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
        glViewport(0, 0, m_viewport.width, m_viewport.height);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        get_shader_code(ff::PhongShader, phong_shader_vs, phong_shader_fs);

        for (auto obj : m_rtr_secene->mOpaques)
        {
            para = m_rtr_shader_programs->getParameters(
                obj->getMaterial(), obj, m_rtr_base_env.light->mType, phong_shader_vs, phong_shader_fs,
                mDenoise, mTaa, m_rtr_base_env.isRenderSkyBox);
            cacheKey = m_rtr_shader_programs->getProgramCacheKey(para);
            phong_shader = m_rtr_shader_programs->acquireProgram(para, cacheKey);

            phong_shader->use();
            glm::mat4 projection = m_render_camera->getPersProjMatrix();
            glm::mat4 view = m_render_camera->getViewMatrix();
            phong_shader->setMat4("uProjectionMatrix", projection);
            phong_shader->setMat4("uViewMatrix", view);
            obj->updateWorldMatrix();
            glm::mat4 model = obj->getWorldMatrix();
            phong_shader->setMat4("uModelMatrix", model);
            // Set light uniforms
            phong_shader->setMat4("uLightVP", lightSpaceMatrix);
            phong_shader->setVec3("uCameraPos", m_render_camera->Position);

            if (obj->getMaterial()->mDiffuseMap)
            {
                phong_shader->setInt("uDiffuseMap", 0);
                glActiveTexture(GL_TEXTURE0);
                glBindTexture(GL_TEXTURE_2D, obj->getMaterial()->mDiffuseMap->mGlTexture);
            }
            else
            {
                glm::vec3 Kd(obj->getMaterial()->mKd[0], obj->getMaterial()->mKd[1], obj->getMaterial()->mKd[2]);
                phong_shader->setVec3("uKd", Kd);
            }

            if (obj->getMaterial()->mSpecularMap)
            {
                phong_shader->setInt("uSpecularMap", 1);
                glActiveTexture(GL_TEXTURE1);
                glBindTexture(GL_TEXTURE_2D, obj->getMaterial()->mSpecularMap->mGlTexture);
            }
            else
            {
                glm::vec3 Ks(obj->getMaterial()->mKs[0], obj->getMaterial()->mKs[1], obj->getMaterial()->mKs[2]);
                phong_shader->setVec3("uKs", Ks);
            }

            phong_shader->setVec3("uLightPos", m_rtr_base_env.lightPos);

            phong_shader->setInt("uShadowMap", 2);
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
    }

    void RenderSystem::pbr_ssr_render()
    {
        std::string depth_shader_vs;
        std::string depth_shader_fs;
        std::string gBuffer_shader_vs;
        std::string gBuffer_shader_fs;
        std::string pbr_ssr_shader_vs;
        std::string pbr_ssr_shader_fs;
        std::string post_process_shader_vs;
        std::string post_process_shader_fs;
        ff::DriverProgram::Ptr depth_shader = nullptr;
        ff::DriverProgram::Ptr gBuffer_shader = nullptr;
        ff::DriverProgram::Ptr pbr_ssr_shader = nullptr;
        ff::DriverProgram::Ptr post_process_shader = nullptr;
        ff::DriverProgram::Parameters::Ptr para = nullptr;
        HashType cacheKey = 0;

        //psaa1:渲染深度
        get_shader_code(ff::DepthShader, depth_shader_vs, depth_shader_fs);
        config_FBO(ff::DepthShader);

        m_rtr_base_env.light->updateViewMatrix();
        glm::mat4 lightSpaceMatrix = m_rtr_base_env.light->getProjectionMatrix() * m_rtr_base_env.light->getViewMatrix();
        // - now render scene from light's point of view

        for (auto obj : m_rtr_secene->mOpaques)
        {
            para = m_rtr_shader_programs->getParameters(
                obj->getMaterial(), obj, m_rtr_base_env.light->mType, depth_shader_vs, depth_shader_fs);
            cacheKey = m_rtr_shader_programs->getProgramCacheKey(para);
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
        glEnable(GL_STENCIL_TEST);
        glStencilFunc(GL_ALWAYS, 1, 0xFF);
        glStencilOp(GL_KEEP, GL_KEEP, GL_REPLACE);
        glStencilMask(0xFF);

        for (auto obj : m_rtr_secene->mOpaques)
        {
            para = m_rtr_shader_programs->getParameters(
                obj->getMaterial(), obj, m_rtr_base_env.light->mType, gBuffer_shader_vs, gBuffer_shader_fs,
                mDenoise, mTaa, m_rtr_base_env.isRenderSkyBox);
            cacheKey = m_rtr_shader_programs->getProgramCacheKey(para);
            gBuffer_shader = m_rtr_shader_programs->acquireProgram(para, cacheKey);

            gBuffer_shader->use();
            glm::mat4 projection = m_render_camera->getPersProjMatrix();
            glm::mat4 view = m_render_camera->getViewMatrix();
            gBuffer_shader->setMat4("uProjectionMatrix", projection);
            gBuffer_shader->setMat4("uViewMatrix", view);
            obj->updateWorldMatrix();
            glm::mat4 model = obj->getWorldMatrix();
            gBuffer_shader->setMat4("uModelMatrix", model);
            // Set light uniforms
            gBuffer_shader->setMat4("uLightVP", lightSpaceMatrix);

            if (mTaa || mDenoise)
            {
                glm::mat4 preProjection = m_render_camera->getPrePersProjMatrix();
                glm::mat4 preView = m_render_camera->getPreViewMatrix();
                gBuffer_shader->setMat4("uPreProjectionMatrix", preProjection);
                gBuffer_shader->setMat4("uPreViewMatrix", preView);
                glm::mat4 preModel = obj->getPreWorldMatrix();
                gBuffer_shader->setMat4("uPreModelMatrix", preModel);
                gBuffer_shader->setInt("uFrameCount", frameCount);
                gBuffer_shader->setFloat("uScreenWidth", m_viewport.width);
                gBuffer_shader->setFloat("uScreenHeight", m_viewport.height);

                // 保存当前VP矩阵供下一帧使用
                m_render_camera->updatePrePersProjMatrix();
                m_render_camera->updatePreViewMatrix();
                obj->updatePreWorldMatrix();
            }

            if (obj->getMaterial()->mIsFloortMaterial)
            {
                gBuffer_shader->setFloat("uMetallic", m_rtr_base_env.metallic);
                gBuffer_shader->setFloat("uRoughness", m_rtr_base_env.roughness);
            }
            else
            {
                gBuffer_shader->setFloat("uMetallic", m_rtr_secene->metallic);
                gBuffer_shader->setFloat("uRoughness", m_rtr_secene->roughness);
            }

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

            if (m_rtr_base_env.light->mType == ff::AREA_LIGHT) {
                gBuffer_shader->setVec3("uLightPos", m_rtr_base_env.lightPos);
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
        config_FBO(ff::SsrShader);
        glEnable(GL_STENCIL_TEST);
        glStencilFunc(GL_EQUAL, 1, 0xFF);
        //模板测试在片段着色器前进行，这里将gbuffer的模板复制过来，进行模板测试，避免渲染空白区域光照
        glBindFramebuffer(GL_READ_FRAMEBUFFER, gBufferFBO);
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, curFramebuffer);
        glBlitFramebuffer(0, 0, m_viewport.width, m_viewport.height,
            0, 0, m_viewport.width, m_viewport.height,
            GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT, GL_NEAREST);

        get_shader_code(ff::SsrShader, pbr_ssr_shader_vs, pbr_ssr_shader_fs);

        para = m_rtr_shader_programs->getParameters(
            nullptr, nullptr, m_rtr_base_env.light->mType, pbr_ssr_shader_vs, pbr_ssr_shader_fs,
            mDenoise, mTaa, m_rtr_base_env.isRenderSkyBox);
        cacheKey = m_rtr_shader_programs->getProgramCacheKey(para);
        pbr_ssr_shader = m_rtr_shader_programs->acquireProgram(para, cacheKey);

        pbr_ssr_shader->use();
        glm::mat4 projection = m_render_camera->getPersProjMatrix();
        glm::mat4 view = m_render_camera->getViewMatrix();
        pbr_ssr_shader->setMat4("uProjectionMatrix", projection);
        pbr_ssr_shader->setMat4("uViewMatrix", view);
        pbr_ssr_shader->setVec3("uCameraPos", m_render_camera->Position);
        if (mDenoise)
        {
            std::random_device rd;
            std::mt19937 gen(rd());
            std::uniform_int_distribution<> dist(0, 1023);

            int random1 = dist(gen);

            pbr_ssr_shader->setInt("uRandom", random1);
        }
        
        // Set light uniforms
        if (m_rtr_base_env.light->mType == ff::DIRECTION_LIGHT) {
            pbr_ssr_shader->setVec3("uLightDir", m_rtr_base_env.lightPos);
        }
        else if(m_rtr_base_env.light->mType == ff::POINT_LIGHT)
        {
            pbr_ssr_shader->setVec3("uLightPos", m_rtr_base_env.lightPos);
        }
        else if (m_rtr_base_env.light->mType == ff::AREA_LIGHT)
        {
            glm::mat4 model = glm::mat4(1.0f);
            glm::vec3& lightRot = m_rtr_base_env.light->rotation;
            glm::quat quatX = glm::angleAxis(glm::radians(lightRot.x), glm::vec3(1.0f, 0.0f, 0.0f));
            glm::quat quatY = glm::angleAxis(glm::radians(lightRot.y), glm::vec3(0.0f, 1.0f, 0.0f));
            glm::quat quatZ = glm::angleAxis(glm::radians(lightRot.z), glm::vec3(0.0f, 0.0f, 1.0f));
            glm::quat finalQuat = quatX * quatY * quatZ; // 顺序敏感
            glm::mat4 rotationMatrix = glm::mat4_cast(finalQuat);
            glm::mat4 translateMatrix = glm::translate(glm::mat4(1.0f), m_rtr_base_env.lightPos);
            model = translateMatrix * rotationMatrix;
            glm::vec3 lightPointPos[4];
            lightPointPos[0] = model * glm::vec4(m_rtr_base_env.light->edgePos[0].position, 1.0);
            lightPointPos[1] = model * glm::vec4(m_rtr_base_env.light->edgePos[1].position, 1.0);
            lightPointPos[2] = model * glm::vec4(m_rtr_base_env.light->edgePos[2].position, 1.0);
            lightPointPos[3] = model * glm::vec4(m_rtr_base_env.light->edgePos[3].position, 1.0);
            pbr_ssr_shader->setVec3("points[0]", lightPointPos[0]);
            pbr_ssr_shader->setVec3("points[1]", lightPointPos[1]);
            pbr_ssr_shader->setVec3("points[2]", lightPointPos[3]);  //按顺时针顺序传入
            pbr_ssr_shader->setVec3("points[3]", lightPointPos[2]);

            pbr_ssr_shader->setInt("LTC1", 7);
            glActiveTexture(GL_TEXTURE7);
            glBindTexture(GL_TEXTURE_2D, m_rtr_base_env.light->M_INV);
            pbr_ssr_shader->setInt("LTC2", 8);
            glActiveTexture(GL_TEXTURE8);
            glBindTexture(GL_TEXTURE_2D, m_rtr_base_env.light->FG);
        }
        glm::vec3 lightRadiance = m_rtr_base_env.light->mColor * m_rtr_base_env.light->mIntensity;
        pbr_ssr_shader->setVec3("uLightRadiance", lightRadiance);

        pbr_ssr_shader->setInt("uGDiffuse", 0);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, ssColorMap);
        pbr_ssr_shader->setInt("uGDepth", 1);
        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, ssDepthMap);
        pbr_ssr_shader->setInt("uGNormalWorld", 2);
        glActiveTexture(GL_TEXTURE2);
        glBindTexture(GL_TEXTURE_2D, ssNormalMap);
        pbr_ssr_shader->setInt("uGVRM", 3);
        glActiveTexture(GL_TEXTURE3);
        glBindTexture(GL_TEXTURE_2D, ssVRM);
        pbr_ssr_shader->setInt("uGPosWorld", 4);
        glActiveTexture(GL_TEXTURE4);
        glBindTexture(GL_TEXTURE_2D, ssWorldPosMap);

        pbr_ssr_shader->setInt("uBRDFLut", 5);
        glActiveTexture(GL_TEXTURE5);
        glBindTexture(GL_TEXTURE_2D, m_rtr_secene->mBRDFLut->mGlTexture);
        pbr_ssr_shader->setInt("uEavgLut", 6);
        glActiveTexture(GL_TEXTURE6);
        glBindTexture(GL_TEXTURE_2D, m_rtr_secene->mEavgLut->mGlTexture);

        if (m_rtr_base_env.isRenderSkyBox)
        {
            pbr_ssr_shader->setInt("uPrefilterMap", 9);
            glActiveTexture(GL_TEXTURE9);
            glBindTexture(GL_TEXTURE_CUBE_MAP, m_rtr_base_env.prefilterMap);
            pbr_ssr_shader->setInt("uIBLBrdfLUT", 10);
            glActiveTexture(GL_TEXTURE10);
            glBindTexture(GL_TEXTURE_2D, m_rtr_base_env.brdfLUTTexture);
        }

        renderQuad();

        
        // draw models in the scene
        glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
        glViewport(0, 0, m_viewport.width, m_viewport.height);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        glDisable(GL_STENCIL_TEST);
        glDisable(GL_DEPTH_TEST);

        get_shader_code(ff::PostProcessShader, post_process_shader_vs, post_process_shader_fs);

        para = m_rtr_shader_programs->getParameters(
            nullptr, nullptr, m_rtr_base_env.light->mType, post_process_shader_vs, post_process_shader_fs, mDenoise, mTaa);
        cacheKey = m_rtr_shader_programs->getProgramCacheKey(para);
        post_process_shader = m_rtr_shader_programs->acquireProgram(para, cacheKey);

        post_process_shader->use();

        post_process_shader->setInt("uCurrentColor", 0);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, curColor);

        if (mTaa || mDenoise)
        {
            post_process_shader->setFloat("uScreenWidth", m_viewport.width);
            post_process_shader->setFloat("uScreenHeight", m_viewport.height);
            post_process_shader->setInt("uFrameCount", frameCount);

            post_process_shader->setInt("uPreviousColor", 1);
            glActiveTexture(GL_TEXTURE1);
            glBindTexture(GL_TEXTURE_2D, previousColor);

            post_process_shader->setInt("uVelocityMap", 2);
            glActiveTexture(GL_TEXTURE2);
            glBindTexture(GL_TEXTURE_2D, ssVelocityMap);

            if (mTaa)
            {
                post_process_shader->setInt("uCurrentDepth", 3);
                glActiveTexture(GL_TEXTURE3);
                glBindTexture(GL_TEXTURE_2D, ssDepthMap);
            }
            if (mDenoise)
            {
                post_process_shader->setInt("uGNormalWorld", 4);
                glActiveTexture(GL_TEXTURE4);
                glBindTexture(GL_TEXTURE_2D, ssNormalMap);
                post_process_shader->setInt("uGPosWorld", 5);
                glActiveTexture(GL_TEXTURE5);
                glBindTexture(GL_TEXTURE_2D, ssWorldPosMap);
            }
            
        }
        renderQuad();

        if (mTaa || mDenoise)
        {
            frameCount++;
            glBindFramebuffer(GL_READ_FRAMEBUFFER, framebuffer);  // 来源
            glReadBuffer(GL_COLOR_ATTACHMENT0);

            glBindFramebuffer(GL_DRAW_FRAMEBUFFER, preFramebuffer);  // 目标
            glDrawBuffer(GL_COLOR_ATTACHMENT0);

            glBlitFramebuffer(
                0, 0, m_viewport.width, m_viewport.height,  // src rect
                0, 0, m_viewport.width, m_viewport.height,  // dst rect
                GL_COLOR_BUFFER_BIT,
                GL_NEAREST            // 过滤方式（或 GL_LINEAR）
            );
        }
        glBindFramebuffer(GL_READ_FRAMEBUFFER, gBufferFBO);
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, framebuffer);
        glBlitFramebuffer(0, 0, m_viewport.width, m_viewport.height,
            0, 0, m_viewport.width, m_viewport.height,
            GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT, GL_NEAREST);
        glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
        glEnable(GL_DEPTH_TEST);
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
        case ff::SsrMaterialType:
            pbr_ssr_render();
            break;
        case ff::MeshPhongMaterialType:
            phone_render();
            break;
        default:
            phone_render();
        } 
    }

    void RenderSystem::tick(float delta_time)
    {
        // refresh render target frame buffer
        refreshFrameBuffer();
        
        glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
        glViewport(0, 0, m_viewport.width, m_viewport.height);
        //glClearColor(0.05f, 0.05f, 0.05f, 1.0f);
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
        rtr_skybox();

        // draw editor ui
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glViewport(0, 0, m_viewport.width, m_viewport.height);
        glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

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

        //GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
        //if (status != GL_FRAMEBUFFER_COMPLETE)
        //{
        //    std::cerr << "Framebuffer incomplete: ";
        //    switch (status) {
        //    case GL_FRAMEBUFFER_UNDEFINED: std::cerr << "UNDEFINED"; break;
        //    case GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT: std::cerr << "INCOMPLETE_ATTACHMENT"; break;
        //    case GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT: std::cerr << "MISSING_ATTACHMENT"; break;
        //    case GL_FRAMEBUFFER_INCOMPLETE_DRAW_BUFFER: std::cerr << "DRAW_BUFFER"; break;
        //    case GL_FRAMEBUFFER_INCOMPLETE_READ_BUFFER: std::cerr << "READ_BUFFER"; break;
        //    case GL_FRAMEBUFFER_UNSUPPORTED: std::cerr << "UNSUPPORTED"; break;
        //    default: std::cerr << "UNKNOWN"; break;
        //    }
        //    std::cerr << std::endl;
        //}

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
        if (std::fabs(m_viewport.width - width) > 1e-5f || std::fabs(m_viewport.height - height) > 1e-5f)
        {
            updateFBO = true;
        }
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

       case ff::PhongShader:
           vertexPath = (config_manager->getShaderFolder() / "phong.vs").generic_string();
           fragmentPath = (config_manager->getShaderFolder() / "phong.fs").generic_string();
           break;

       case ff::SsrGbufferShader:
           vertexPath = (config_manager->getShaderFolder() / "ssr_gbuffer.vs").generic_string();
           fragmentPath = (config_manager->getShaderFolder() / "ssr_gbuffer.fs").generic_string();
           break;

       case ff::SsrShader:
           //vertexPath = (config_manager->getShaderFolder() / "ssr.vs").generic_string();
           //fragmentPath = (config_manager->getShaderFolder() / "ssr.fs").generic_string();
           vertexPath = (config_manager->getShaderFolder() / "brdf_ssr.vs").generic_string();
           fragmentPath = (config_manager->getShaderFolder() / "brdf_ssr.fs").generic_string();
           break;

        case ff::PostProcessShader:
            vertexPath = (config_manager->getShaderFolder() / "post_process.vs").generic_string();
            fragmentPath = (config_manager->getShaderFolder() / "post_process.fs").generic_string();
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
        if (updateFBO)
        {
            if (depthBufferFBO != 0)
            {
                glDeleteFramebuffers(1, &depthBufferFBO);
                glDeleteTextures(1, &depthMap);
                depthBufferFBO = 0;
                depthMap = 0;
            }
            if (gBufferFBO != 0)
            {
                glDeleteFramebuffers(1, &gBufferFBO);
                glDeleteTextures(1, &ssColorMap);
                glDeleteTextures(1, &ssDepthMap);
                glDeleteTextures(1, &ssNormalMap);
                glDeleteTextures(1, &ssVRM);
                glDeleteTextures(1, &ssWorldPosMap);
                glDeleteTextures(1, &ssVelocityMap);
                glDeleteRenderbuffers(1, &gBufferRboDepth);
                gBufferFBO = 0;
                ssColorMap = 0;
                ssDepthMap = 0;
                ssNormalMap = 0;
                ssVRM = 0;
                ssWorldPosMap = 0;
                ssVelocityMap = 0;
                gBufferRboDepth = 0;
                
            }
            if (preFramebuffer != 0)
            {
                glDeleteFramebuffers(1, &preFramebuffer);
                glDeleteTextures(1, &previousColor);
                preFramebuffer = 0;
                previousColor = 0;
            }
            if (curFramebuffer != 0)
            {
                glDeleteFramebuffers(1, &curFramebuffer);
                glDeleteTextures(1, &curColor);
                glDeleteRenderbuffers(1, &curDepthBuffer);
                curFramebuffer = 0;
                curColor = 0;
                curDepthBuffer = 0;
            }
            frameCount = 0;
            updateFBO = false;
        }
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
                if (mTaa || mDenoise)
                {
                    if (preFramebuffer == 0)
                    {
                        // 创建帧缓冲
                        glGenFramebuffers(1, &preFramebuffer);
                        glBindFramebuffer(GL_FRAMEBUFFER, preFramebuffer);

                        // 颜色纹理
                        glGenTextures(1, &previousColor);
                        glBindTexture(GL_TEXTURE_2D, previousColor);
                        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, m_viewport.width, m_viewport.height, 0, GL_RGBA, GL_FLOAT, NULL);
                        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
                        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
                        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, previousColor, 0);

                        // 检查帧缓冲完整性
                        if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
                            std::cerr << "Previous Framebuffer not complete!" << std::endl;
                        }

                        glBindFramebuffer(GL_FRAMEBUFFER, 0);
                    }
                }
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

                    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, ssColorMap, 0);

                    glGenTextures(1, &ssDepthMap);
                    glBindTexture(GL_TEXTURE_2D, ssDepthMap);
                    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, m_viewport.width, m_viewport.height, 0, GL_RGBA, GL_FLOAT, NULL);
                    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
                    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
                    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1, GL_TEXTURE_2D, ssDepthMap, 0);

                    // normal color buffer
                    glGenTextures(1, &ssNormalMap);
                    glBindTexture(GL_TEXTURE_2D, ssNormalMap);
                    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB16F, m_viewport.width, m_viewport.height, 0, GL_RGB, GL_FLOAT, NULL);
                    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
                    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
                    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT2, GL_TEXTURE_2D, ssNormalMap, 0);

                    glGenTextures(1, &ssVRM);
                    glBindTexture(GL_TEXTURE_2D, ssVRM);
                    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB16F, m_viewport.width, m_viewport.height, 0, GL_RGB, GL_FLOAT, NULL);
                    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
                    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
                    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT3, GL_TEXTURE_2D, ssVRM, 0);
                    
                    glGenTextures(1, &ssWorldPosMap);
                    glBindTexture(GL_TEXTURE_2D, ssWorldPosMap);
                    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB32F, m_viewport.width, m_viewport.height, 0, GL_RGB, GL_FLOAT, NULL);
                    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
                    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
                    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT4, GL_TEXTURE_2D, ssWorldPosMap, 0);

                    if (mTaa || mDenoise)
                    {
                        glGenTextures(1, &ssVelocityMap);
                        glBindTexture(GL_TEXTURE_2D, ssVelocityMap);
                        glTexImage2D(GL_TEXTURE_2D, 0, GL_RG16F, m_viewport.width, m_viewport.height, 0, GL_RG, GL_FLOAT, NULL);
                        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
                        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
                        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT5, GL_TEXTURE_2D, ssVelocityMap, 0);
                    }

                    unsigned int attachments[6] = { GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1, GL_COLOR_ATTACHMENT2, GL_COLOR_ATTACHMENT3, GL_COLOR_ATTACHMENT4, GL_COLOR_ATTACHMENT5 };
                    glDrawBuffers(6, attachments);

                    //必须添加深度缓冲，否则不会进行深度测试
                    glGenRenderbuffers(1, &gBufferRboDepth);
                    glBindRenderbuffer(GL_RENDERBUFFER, gBufferRboDepth);
                    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, m_viewport.width, m_viewport.height);
                    glBindRenderbuffer(GL_RENDERBUFFER, 0);
                    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, gBufferRboDepth);
                    
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
                //glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
                glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
                break;
            case ff::SsrShader:
                if (curFramebuffer == 0)
                {
                    // 创建帧缓冲
                    glGenFramebuffers(1, &curFramebuffer);
                    glBindFramebuffer(GL_FRAMEBUFFER, curFramebuffer);

                    // 颜色纹理
                    glGenTextures(1, &curColor);
                    glBindTexture(GL_TEXTURE_2D, curColor);
                    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, m_viewport.width, m_viewport.height, 0, GL_RGBA, GL_FLOAT, NULL);
                    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
                    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
                    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, curColor, 0);

                    //用于进行模板测试
                    glGenRenderbuffers(1, &curDepthBuffer);
                    glBindRenderbuffer(GL_RENDERBUFFER, curDepthBuffer);
                    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, m_viewport.width, m_viewport.height);
                    glBindRenderbuffer(GL_RENDERBUFFER, 0);
                    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, curDepthBuffer);

                    // 检查帧缓冲完整性
                    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
                        std::cerr << "Current Framebuffer not complete!" << std::endl;
                    }

                    glBindFramebuffer(GL_FRAMEBUFFER, 0);
                }
                glViewport(0, 0, m_viewport.width, m_viewport.height);
                glBindFramebuffer(GL_FRAMEBUFFER, curFramebuffer);
                //glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
                glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
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
                m_rtr_base_env.floorMaterial->mIsFloortMaterial = true;
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

    void RenderSystem::rtr_process_skybox() {
        if (m_rtr_base_env.isRenderSkyBox)
        {
            if (m_rtr_base_env.skyBoxVAO == 0)
            {
                float skyboxVertices[] = {
                    // back face
                    -1.0f, -1.0f, -1.0f,  0.0f,  0.0f, -1.0f, 0.0f, 0.0f, // bottom-left
                     1.0f,  1.0f, -1.0f,  0.0f,  0.0f, -1.0f, 1.0f, 1.0f, // top-right
                     1.0f, -1.0f, -1.0f,  0.0f,  0.0f, -1.0f, 1.0f, 0.0f, // bottom-right         
                     1.0f,  1.0f, -1.0f,  0.0f,  0.0f, -1.0f, 1.0f, 1.0f, // top-right
                    -1.0f, -1.0f, -1.0f,  0.0f,  0.0f, -1.0f, 0.0f, 0.0f, // bottom-left
                    -1.0f,  1.0f, -1.0f,  0.0f,  0.0f, -1.0f, 0.0f, 1.0f, // top-left
                    // front face
                    -1.0f, -1.0f,  1.0f,  0.0f,  0.0f,  1.0f, 0.0f, 0.0f, // bottom-left
                     1.0f, -1.0f,  1.0f,  0.0f,  0.0f,  1.0f, 1.0f, 0.0f, // bottom-right
                     1.0f,  1.0f,  1.0f,  0.0f,  0.0f,  1.0f, 1.0f, 1.0f, // top-right
                     1.0f,  1.0f,  1.0f,  0.0f,  0.0f,  1.0f, 1.0f, 1.0f, // top-right
                    -1.0f,  1.0f,  1.0f,  0.0f,  0.0f,  1.0f, 0.0f, 1.0f, // top-left
                    -1.0f, -1.0f,  1.0f,  0.0f,  0.0f,  1.0f, 0.0f, 0.0f, // bottom-left
                    // left face
                    -1.0f,  1.0f,  1.0f, -1.0f,  0.0f,  0.0f, 1.0f, 0.0f, // top-right
                    -1.0f,  1.0f, -1.0f, -1.0f,  0.0f,  0.0f, 1.0f, 1.0f, // top-left
                    -1.0f, -1.0f, -1.0f, -1.0f,  0.0f,  0.0f, 0.0f, 1.0f, // bottom-left
                    -1.0f, -1.0f, -1.0f, -1.0f,  0.0f,  0.0f, 0.0f, 1.0f, // bottom-left
                    -1.0f, -1.0f,  1.0f, -1.0f,  0.0f,  0.0f, 0.0f, 0.0f, // bottom-right
                    -1.0f,  1.0f,  1.0f, -1.0f,  0.0f,  0.0f, 1.0f, 0.0f, // top-right
                    // right face
                     1.0f,  1.0f,  1.0f,  1.0f,  0.0f,  0.0f, 1.0f, 0.0f, // top-left
                     1.0f, -1.0f, -1.0f,  1.0f,  0.0f,  0.0f, 0.0f, 1.0f, // bottom-right
                     1.0f,  1.0f, -1.0f,  1.0f,  0.0f,  0.0f, 1.0f, 1.0f, // top-right         
                     1.0f, -1.0f, -1.0f,  1.0f,  0.0f,  0.0f, 0.0f, 1.0f, // bottom-right
                     1.0f,  1.0f,  1.0f,  1.0f,  0.0f,  0.0f, 1.0f, 0.0f, // top-left
                     1.0f, -1.0f,  1.0f,  1.0f,  0.0f,  0.0f, 0.0f, 0.0f, // bottom-left     
                     // bottom face
                     -1.0f, -1.0f, -1.0f,  0.0f, -1.0f,  0.0f, 0.0f, 1.0f, // top-right
                      1.0f, -1.0f, -1.0f,  0.0f, -1.0f,  0.0f, 1.0f, 1.0f, // top-left
                      1.0f, -1.0f,  1.0f,  0.0f, -1.0f,  0.0f, 1.0f, 0.0f, // bottom-left
                      1.0f, -1.0f,  1.0f,  0.0f, -1.0f,  0.0f, 1.0f, 0.0f, // bottom-left
                     -1.0f, -1.0f,  1.0f,  0.0f, -1.0f,  0.0f, 0.0f, 0.0f, // bottom-right
                     -1.0f, -1.0f, -1.0f,  0.0f, -1.0f,  0.0f, 0.0f, 1.0f, // top-right
                     // top face
                     -1.0f,  1.0f, -1.0f,  0.0f,  1.0f,  0.0f, 0.0f, 1.0f, // top-left
                      1.0f,  1.0f , 1.0f,  0.0f,  1.0f,  0.0f, 1.0f, 0.0f, // bottom-right
                      1.0f,  1.0f, -1.0f,  0.0f,  1.0f,  0.0f, 1.0f, 1.0f, // top-right     
                      1.0f,  1.0f,  1.0f,  0.0f,  1.0f,  0.0f, 1.0f, 0.0f, // bottom-right
                     -1.0f,  1.0f, -1.0f,  0.0f,  1.0f,  0.0f, 0.0f, 1.0f, // top-left
                     -1.0f,  1.0f,  1.0f,  0.0f,  1.0f,  0.0f, 0.0f, 0.0f  // bottom-left  
                };

                unsigned int skyBoxVBO;
                // AREA LIGHT
                glGenVertexArrays(1, &m_rtr_base_env.skyBoxVAO);
                glBindVertexArray(m_rtr_base_env.skyBoxVAO);

                glGenBuffers(1, &skyBoxVBO);
                glBindBuffer(GL_ARRAY_BUFFER, skyBoxVBO);
                glBufferData(GL_ARRAY_BUFFER, sizeof(skyboxVertices), &skyboxVertices, GL_STATIC_DRAW);

                glEnableVertexAttribArray(0);
                glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)0);
                glEnableVertexAttribArray(1);
                glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(3 * sizeof(float)));
                glEnableVertexAttribArray(2);
                glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(6 * sizeof(float)));
                glBindBuffer(GL_ARRAY_BUFFER, 0);
                glBindVertexArray(0);
            }

            if (m_rtr_base_env.cubeMapFBO == 0)
            {
                glGenFramebuffers(1, &m_rtr_base_env.cubeMapFBO);
                glGenRenderbuffers(1, &m_rtr_base_env.cubeMapRBO);

                glBindFramebuffer(GL_FRAMEBUFFER, m_rtr_base_env.cubeMapFBO);
                glBindRenderbuffer(GL_RENDERBUFFER, m_rtr_base_env.cubeMapRBO);
                glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, 512, 512);
                glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, m_rtr_base_env.cubeMapRBO);
            }
            if (m_rtr_base_env.brdfLUTTexture == 0)
            {
                glGenTextures(1, &m_rtr_base_env.brdfLUTTexture);

                // pre-allocate enough memory for the LUT texture.
                glBindTexture(GL_TEXTURE_2D, m_rtr_base_env.brdfLUTTexture);
                glTexImage2D(GL_TEXTURE_2D, 0, GL_RG16F, 512, 512, 0, GL_RG, GL_FLOAT, 0);
                // be sure to set wrapping mode to GL_CLAMP_TO_EDGE
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

                // then re-configure capture framebuffer object and render screen-space quad with BRDF shader.
                glBindFramebuffer(GL_FRAMEBUFFER, m_rtr_base_env.cubeMapFBO);
                glBindRenderbuffer(GL_RENDERBUFFER, m_rtr_base_env.cubeMapRBO);
                glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, 512, 512);
                glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, m_rtr_base_env.brdfLUTTexture, 0);

                glViewport(0, 0, 512, 512);
                std::shared_ptr<ConfigManager> config_manager = g_runtime_global_context.m_config_manager;
                std::shared_ptr<Shader> brdfShader = std::make_shared<Shader>((config_manager->getShaderFolder() / "brdf_prt.vs").generic_string().data(),
                    (config_manager->getShaderFolder() / "brdf_prt.fs").generic_string().data());
                brdfShader->use();
                glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
                renderQuad();

                glBindFramebuffer(GL_FRAMEBUFFER, 0);
            }
            if (m_rtr_base_env.prefilterMap != 0)
            {
                glDeleteTextures(1, &m_rtr_base_env.prefilterMap);
            }
            glGenTextures(1, &m_rtr_base_env.prefilterMap);
            glBindTexture(GL_TEXTURE_CUBE_MAP, m_rtr_base_env.prefilterMap);
            for (unsigned int i = 0; i < 6; ++i)
            {
                glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0, GL_RGB16F, 128, 128, 0, GL_RGB, GL_FLOAT, nullptr);
            }
            glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR); // be sure to set minification filter to mip_linear 
            glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            // generate mipmaps for the cubemap so OpenGL automatically allocates the required memory.
            glGenerateMipmap(GL_TEXTURE_CUBE_MAP);

            std::shared_ptr<ConfigManager> config_manager = g_runtime_global_context.m_config_manager;
            std::shared_ptr<Shader> prefilterShader = std::make_shared<Shader>((config_manager->getShaderFolder() / "ibl_light_prt.vs").generic_string().data(),
                (config_manager->getShaderFolder() / "ibl_light_prt.fs").generic_string().data());

            glm::mat4 captureViews[] =
            {
                glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(1.0f,  0.0f,  0.0f), glm::vec3(0.0f, -1.0f,  0.0f)),
                glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(-1.0f,  0.0f,  0.0f), glm::vec3(0.0f, -1.0f,  0.0f)),
                glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f,  1.0f,  0.0f), glm::vec3(0.0f,  0.0f,  1.0f)),
                glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, -1.0f,  0.0f), glm::vec3(0.0f,  0.0f, -1.0f)),
                glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f,  0.0f,  1.0f), glm::vec3(0.0f, -1.0f,  0.0f)),
                glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f,  0.0f, -1.0f), glm::vec3(0.0f, -1.0f,  0.0f))
            };
            glm::mat4 captureProjection = glm::perspective(glm::radians(90.0f), 1.0f, 0.1f, 10.0f);

            prefilterShader->use();
            prefilterShader->setInt("uEnvironmentMap", 0);
            prefilterShader->setMat4("uProjectionMatrix", captureProjection);
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_CUBE_MAP, m_rtr_base_env.skyBox->mGlTexture);

            glBindFramebuffer(GL_FRAMEBUFFER, m_rtr_base_env.cubeMapFBO);
            unsigned int maxMipLevels = 5;
            for (unsigned int mip = 0; mip < maxMipLevels; ++mip)
            {
                // reisze framebuffer according to mip-level size.
                unsigned int mipWidth = static_cast<unsigned int>(128 * std::pow(0.5, mip));
                unsigned int mipHeight = static_cast<unsigned int>(128 * std::pow(0.5, mip));
                glBindRenderbuffer(GL_RENDERBUFFER, m_rtr_base_env.cubeMapRBO);
                glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, mipWidth, mipHeight);
                glViewport(0, 0, mipWidth, mipHeight);

                float roughness = (float)mip / (float)(maxMipLevels - 1);
                prefilterShader->setFloat("uRoughness", roughness);
                for (unsigned int i = 0; i < 6; ++i)
                {
                    prefilterShader->setMat4("uViewMatrix", captureViews[i]);
                    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, m_rtr_base_env.prefilterMap, mip);
                    glDepthFunc(GL_LEQUAL);
                    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
                    glBindVertexArray(m_rtr_base_env.skyBoxVAO);
                    glDrawArrays(GL_TRIANGLES, 0, 36);
                    glBindVertexArray(0);
                    glDepthFunc(GL_LESS);
                }
            }
            glBindFramebuffer(GL_FRAMEBUFFER, 0);
            
        }
    }

} // namespace MiniEngine
