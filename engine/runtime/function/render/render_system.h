#pragma once

#include <array>
#include <memory>
#include <optional>

#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>

#include "runtime/engine/engine.h"
#include "runtime/function/render/render_entity.h"
#include "runtime/function/render/render_guid_allocator.h"
#include "runtime/function/render/render_swap_context.h"
#include "runtime/function/render/render_type.h"
#include "editor/include/editor_ui.h"
#include "runtime/function/render/rtr/scene/scene.h"
#include "runtime/function/render/rtr/math/frustum.h"
#include "runtime/function/render/rtr/lights/light.h"
#include "runtime/function/render/rtr/objects/mesh.h"
#include "runtime/function/render/rtr/material/material.h"
#include "runtime/function/render/rtr/geometries/boxGeometry.h"
#include "runtime/function/render/rtr/render/driverPrograms.h"

namespace MiniEngine
{
    class WindowSystem;
    class RenderScene;
    class RenderResource;
    class Camera;
    class Canvas;
    class Shader;
    class WindowUI;
    class Model;

    struct EngineContentViewport
    {
        float x{0.f};
        float y{0.f};
        float width{0.f};
        float height{0.f};
    };

    class BaseRenderEnviroment
    {
        public:
            ff::Light::Ptr light{ nullptr };
            glm::vec3 lightPos{ 5,3,-5 };

            bool isRenderFloor = false;
            glm::vec3 floorPos{ 0,-3,0 };
            ff::Mesh::Ptr floor{ nullptr };
            ff::Material::Ptr floorMaterial{ nullptr };
            ff::BoxGeometry::Ptr floorGeometry{ nullptr };
            float metallic = 1.0;
            float roughness = 0.9;

            bool isRenderSkyBox = false;
            //环境光照
            unsigned int skyBoxVAO = 0;
            unsigned int cubeMapFBO = 0;
            unsigned int cubeMapRBO = 0;
            unsigned int prefilterMap = 0;
            unsigned int brdfLUTTexture = 0;
            //天空盒
            ff::CubeTexture::Ptr skyBox = nullptr;
    };

    struct RenderSystemInitInfo
    {
        std::shared_ptr<WindowSystem> window_system;
    };

    class RenderSystem
    {
    public:
        RenderSystem() = default;
        ~RenderSystem();

        void initialize(RenderSystemInitInfo init_info);
        void tick(float delta_time);
        void clear();

        std::shared_ptr<Model> getRenderModel() const;
        std::shared_ptr<Camera> getRenderCamera() const;
        std::shared_ptr<PathTracing::PathTracer> getPathTracer() const;

        void initializeUIRenderBackend(WindowUI* window_ui);
        void updateEngineContentViewport(float offset_x, float offset_y, float width, float height);
        EngineContentViewport getEngineContentViewport() const;
        
        unsigned int getFrameBuffer() {return framebuffer;}
        unsigned int getTexColorBuffer() {return texColorBuffer;}
        unsigned int getTexDepthBuffer() {return texDepthBuffer;}

        void loadScene(char* filepath);
        void unloadScene();
        void setupCanvas(float hw, float hh);
        void startRendering();
        void stopRendering();

        void rtr_scene();
        void rtr_object();
        void rtr_light_model();
        void rtr_skybox();
        void get_shader_code(ff::ShaderType shaderType, string &vs, string &fs) noexcept;//生成shader并设置uniform
        void config_FBO(ff::ShaderType shaderType) noexcept;
        void rtr_process_floor(glm::vec3 pos, ff::MaterialType materialType = ff::MeshBasicMaterialType);
        void rtr_process_skybox();

        void projectObject(const ff::Object3D::Ptr& object) noexcept;

    private:
        void refreshFrameBuffer();
        void phone_render();
        void pcss_shadow_render();
        void ssr_render();
        void pbr_render();
        void pbr_ssr_render();
        void renderQuad();

        GLFWwindow *m_window;
        WindowUI *m_ui;
        EngineContentViewport m_viewport;
        std::thread m_tracing_process;
        std::shared_ptr<Model> m_render_model;
        std::shared_ptr<Canvas> m_render_canvas;
        std::shared_ptr<Shader> m_render_shader;
        std::shared_ptr<Shader> m_canvas_shader;
        std::shared_ptr<Camera> m_render_camera;
        std::shared_ptr<Camera> m_viewer_camera;
        std::shared_ptr<PathTracing::PathTracer> m_path_tracer;

        ff::Frustum::Ptr m_rtr_frustum{ nullptr };
        std::shared_ptr<Shader> m_rtr_light_shader;
        std::shared_ptr<Shader> m_rtr_skybox_shader;
        ff::DriverPrograms::Ptr m_rtr_shader_programs{ nullptr };
    
    public:
        ff::Scene::Ptr m_rtr_secene{ nullptr };

        //所有自定义的绘制都要渲染到该fbo上，最终才会显示到屏幕上
        unsigned int texColorBuffer, texDepthBuffer, framebuffer= 0;

        unsigned int flooreVAO, floorIndexCount = 0;

        unsigned int quadVAO = 0;
        unsigned int quadVBO;

        //深度缓冲
        unsigned int depthBufferFBO = 0;
        GLuint depthMap = 0;
        
        //Gbuffer
        unsigned int gBufferFBO = 0;
        GLuint ssColorMap = 0;
        GLuint ssDepthMap = 0;  //经过mvp变换的线性深度（离相机的距离，不是深度缓冲里归一化的深度）
        GLuint ssNormalMap = 0;
        GLuint ssVRM = 0;   //存储可见性、粗糙度和金属度
        GLuint ssWorldPosMap = 0;
        GLuint ssVelocityMap = 0;  //存储motion vector
        GLuint gBufferRboDepth = 0; //帧缓冲对象必须有深度附件，否则不会进行深度测试

        unsigned int preFramebuffer = 0;
        GLuint previousColor = 0;
        unsigned int curFramebuffer = 0;
        GLuint curColor = 0;
        GLuint curDepthBuffer = 0;
        
        BaseRenderEnviroment m_rtr_base_env;

        bool mDenoise = false;
        bool mTaa = false;

        bool updateFBO = false;  //修改窗口大小、切换材质、抗锯齿或者降噪时需要更新

        int frameCount = 0;  //当前是第几帧，从加载物体开始计算，重新加载物体或者updateFBO=true时清零
    };

    
}