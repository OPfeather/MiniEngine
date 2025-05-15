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

    void RenderSystem::rtr_object()
    {
        //绘制球形灯源

        //透视剪裁
        auto currentViewMatrix = m_render_camera->getPersProjMatrix() * m_render_camera->getViewMatrix();
        m_rtr_frustum->setFromProjectionMatrix(currentViewMatrix);
        
        m_rtr_secene->mOpaques.clear();
        m_rtr_secene->mTransparents.clear();
        projectObject(m_rtr_secene);

        //TODO:排序

        //渲染
        //pass1
        //TODO:在生成shader的函数里实现
        if (gBufferFBO == 0)
        {
            glGenFramebuffers(1, &gBufferFBO);
            // - Create depth texture
            glGenTextures(1, &depthMap);
            glBindTexture(GL_TEXTURE_2D, depthMap);

            glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT32F, 4096, 4096, 0, GL_DEPTH_COMPONENT, GL_FLOAT, NULL);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
            GLfloat borderColor[] = { 1.0, 1.0, 1.0, 1.0 };
            glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, borderColor);

            glBindFramebuffer(GL_FRAMEBUFFER, gBufferFBO);
            glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, depthMap, 0);
            glDrawBuffer(GL_NONE);
            glReadBuffer(GL_NONE);
            glBindFramebuffer(GL_FRAMEBUFFER, 0);
        }
        glViewport(0, 0, 4096, 4096);
        glBindFramebuffer(GL_FRAMEBUFFER, gBufferFBO);
        glClear(GL_DEPTH_BUFFER_BIT);

        std::shared_ptr<Shader>depth_shader = m_rtr_shader_map["depth"];
        std::shared_ptr<Shader>shadow_shader = m_rtr_shader_map["shadow"];

        glm::mat4 lightProjection, lightView;
        glm::mat4 lightSpaceMatrix;
        GLfloat near_plane = 1.0f, far_plane = 100.0f;
        lightProjection = glm::ortho(-50.0f, 50.0f, -50.0f, 50.0f, near_plane, far_plane);
        //lightProjection = glm::perspective(45.0f, (GLfloat)SHADOW_WIDTH / (GLfloat)SHADOW_HEIGHT, near_plane, far_plane); // Note that if you use a perspective projection matrix you'll have to change the light position as the current light position isn't enough to reflect the whole scene.
        lightView = glm::lookAt(lightPos, glm::vec3(0.0f), glm::vec3(0.0, 1.0, 0.0));
        lightSpaceMatrix = lightProjection * lightView;
        // - now render scene from light's point of view
        depth_shader->use();
        depth_shader->setMat4("lightSpaceMatrix", lightSpaceMatrix);
        for(auto obj : m_rtr_secene->mOpaques)
        {
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

            // m_render_shader->use();
            // glm::mat4 projection = m_render_camera->getPersProjMatrix();
            // glm::mat4 view = m_render_camera->getViewMatrix();
            // obj->updateWorldMatrix();
            // glm::mat4 model = obj->getWorldMatrix();
            //
            // m_render_shader->setMat4("projection", projection);
            // m_render_shader->setMat4("view", view);
            // m_render_shader->setMat4("model", model);
            // m_render_shader->setVec3("viewPos", m_render_camera->Position);
            // m_render_shader->setInt("diffuse_map", 0);
            // m_render_shader->setInt("specular_map", 1);
            // if (obj->getMaterial()->mDiffuseMap)
            // {
            //     glActiveTexture(GL_TEXTURE0);
            //     glBindTexture(GL_TEXTURE_2D, obj->getMaterial()->mDiffuseMap->mGlTexture);
            // }
            // if (obj->getMaterial()->mSpecularMap)
            // {
            //     glActiveTexture(GL_TEXTURE1);
            //     glBindTexture(GL_TEXTURE_2D, obj->getMaterial()->mSpecularMap->mGlTexture);
            // }

            // obj->getGeometry()->bindVAO();
            // auto index = obj->getGeometry()->getIndex();
            // auto position = obj->getGeometry()->getAttribute("position");
            // if (index)
            // {
            //     glDrawElements(GL_TRIANGLES, index->getCount(), ff::toGL(index->getDataType()), 0);
            // }
            // else
            // {
            //     glDrawArrays(GL_TRIANGLES, 0, position->getCount());
            // }
            //
            //glBindVertexArray(0);
            
        }

        //pass2
        //refreshFrameBuffer();
        // draw models in the scene
        glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
        glViewport(0, 0, m_viewport.width, m_viewport.height);
        glClearColor(0.05f, 0.05f, 0.05f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        shadow_shader->use();
        glm::mat4 projection = m_render_camera->getPersProjMatrix();
        glm::mat4 view = m_render_camera->getViewMatrix();
        shadow_shader->setMat4("projection", projection);
        shadow_shader->setMat4("view", view);
        // Set light uniforms
        shadow_shader->setVec3("lightPos", lightPos);
        shadow_shader->setVec3("viewPos", m_render_camera->Position);
        shadow_shader->setMat4("lightSpaceMatrix", lightSpaceMatrix);
        // Enable/Disable shadows by pressing 'SPACE'
        //glUniform1i(glGetUniformLocation(shader.Program, "shadows"), shadows);
        //shadow_shader->setInt("shadows", 1);
        shadow_shader->setInt("diffuseTexture", 0);
        shadow_shader->setInt("shadowMap", 1);

        for (auto obj : m_rtr_secene->mOpaques)
        {  
            if (obj->getMaterial()->mDiffuseMap)
            {
                glActiveTexture(GL_TEXTURE0);
                glBindTexture(GL_TEXTURE_2D, obj->getMaterial()->mDiffuseMap->mGlTexture);
            }
            glActiveTexture(GL_TEXTURE1);
            glBindTexture(GL_TEXTURE_2D, depthMap);
            
            obj->updateWorldMatrix();
            auto model = obj->getWorldMatrix();
            shadow_shader->setMat4("model", model);

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
            if (m_rtr_frustum->intersectObject(renderableObject)) {

                m_rtr_secene->mOpaques.push_back(renderableObject);
            }
        }

        auto children = object->getChildren();
        for (auto& child : children) {
            projectObject(child);
        }
    }

    void RenderSystem::rtr_shader_config(ff::MaterialType materialType) noexcept {
       std::shared_ptr<ConfigManager> config_manager = g_runtime_global_context.m_config_manager;
       ASSERT(config_manager);
       switch(materialType)
       {
       case ff::SsrMaterialType:

       
       default:
            std::shared_ptr<Shader>depth_shader = std::make_shared<Shader>((config_manager->getShaderFolder() / "shadow_mapping_depth.vs").generic_string().data(),
            (config_manager->getShaderFolder() / "shadow_mapping_depth.fs").generic_string().data());
            
            std::shared_ptr<Shader>shadow_shader = std::make_shared<Shader>((config_manager->getShaderFolder() / "shadow_mapping.vs").generic_string().data(),
                    (config_manager->getShaderFolder() / "shadow_mapping.fs").generic_string().data());
            
            m_rtr_shader_map["depth"] = depth_shader;
            m_rtr_shader_map["shadow"] = shadow_shader;
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
                m_rtr_base_env.floorGeometry = ff::BoxGeometry::create(10.0, 1.0, 10.0);
                m_rtr_base_env.floorMaterial = ff::Material::create();
                //TODO:根据材质加载数据(或者固定材质，但是渲染和灯光一样用单独的shader)
                m_rtr_base_env.floorMaterial->mDiffuseMap = ff::TextureLoader::load("E:/myProject/gameEngine/PiccoloRenderEngine/MiniEngine/engine/editor/demo/texture/concreteTexture.png");
                m_rtr_base_env.floor = ff::Mesh::create(m_rtr_base_env.floorGeometry, m_rtr_base_env.floorMaterial);
                m_rtr_base_env.floor->setPosition(pos.x, pos.y, pos.z);
                m_rtr_base_env.floorGeometry->createVAO();
                m_rtr_base_env.floorGeometry->bindVAO();
                m_rtr_base_env.floorGeometry->setupVertexAttributes();
                m_rtr_secene->addChild(m_rtr_base_env.floor);
                rtr_shader_config();
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
