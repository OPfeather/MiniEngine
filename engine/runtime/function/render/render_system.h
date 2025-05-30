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

    enum LightType
    {
        DIRECTION_LIGHT,
        POINT_LIGHT,
        AREA_LIGHT,
        IBL
    };

    class BaseRenderEnviroment
    {
        public:
        //ff::Light::Ptr light{ nullptr };
        //unsigned int lightVAO, lightIndexCount = 0;
        bool isRenderFloor = false;
        glm::vec3 floorPos{ 0,0,0 };
        ff::Mesh::Ptr floor{ nullptr };
        ff::Material::Ptr floorMaterial{ nullptr };
        ff::BoxGeometry::Ptr floorGeometry{ nullptr };

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
        void get_shader_code(ff::ShaderType shaderType, string &vs, string &fs) noexcept;//生成shader并设置uniform
        void config_FBO(ff::ShaderType shaderType) noexcept;
        void rtr_process_floor(glm::vec3 pos, ff::MaterialType materialType = ff::MeshBasicMaterialType);

        void projectObject(const ff::Object3D::Ptr& object) noexcept;

    private:
        void refreshFrameBuffer();
        void phone_render();
        void pcss_shadow_render();
        void ssr_render();

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
        ff::DriverPrograms::Ptr m_rtr_shader_programs{ nullptr };
    
    public:
        ff::Scene::Ptr m_rtr_secene{ nullptr };
        LightType m_rtr_light_type{DIRECTION_LIGHT};

        unsigned int texColorBuffer, texDepthBuffer, framebuffer= 0;

        unsigned int lightVAO, lightIndexCount = 0;
        glm::vec3 lightPos= glm::vec3(2.5f, 6.0f, 4.0f);
        unsigned int flooreVAO, floorIndexCount = 0;

        unsigned int depthBufferFBO = 0;
        GLuint depthMap = 0;
        
        unsigned int gBufferFBO = 0;
        GLuint ssColorMap = 0;
        GLuint ssDepthMap = 0;  //经过mvp变换的线性深度（离相机的距离，不是深度缓冲里归一化的深度）
        GLuint ssNormalMap = 0;
        GLuint ssVisibilityMap = 0;
        GLuint ssWorldPosMap = 0;
        GLuint gBufferRboDepth = 0; //帧缓冲对象必须有深度附件，否则不会进行深度测试
        
        BaseRenderEnviroment m_rtr_base_env;
    };

    
}