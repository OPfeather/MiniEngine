#include "editor/include/editor_ui.h"

#include "editor/include/editor_global_context.h"
#include "editor/include/editor_input_manager.h"
#include "editor/include/editor_scene_manager.h"

#include "runtime/core/base/macro.h"
#include "runtime/core/meta/reflection/reflection.h"

#include "runtime/platform/path/path.h"

#include "runtime/resource/asset_manager/asset_manager.h"
#include "runtime/resource/config_manager/config_manager.h"

#include "runtime/engine/engine.h"

#include "runtime/function/framework/component/mesh/mesh_component.h"
#include "runtime/function/framework/component/transform/transform_component.h"
#include "runtime/function/framework/scene/scene.h"
#include "runtime/function/framework/world/world_manager.h"
#include "runtime/function/global/global_context.h"
#include "runtime/function/input/input_system.h"
#include "runtime/function/render/render_camera.h"
#include "runtime/function/render/render_system.h"
#include "runtime/function/render/window_system.h"
#include "runtime/function/render/rtr/loader/assimpLoader.h"
#include "runtime/function/render/rtr/loader/textureLoader.h"
#include "runtime/function/render/rtr/loader/cubeTextureLoader.h"
#include "runtime/function/render/rtr/geometries/boxGeometry.h"
#include "runtime/function/render/rtr/objects/mesh.h"

#include <imgui.h>
#include <imgui_internal.h>
#include <stb_image.h>
#include <nfd.h>

namespace MiniEngine
{
    std::vector<std::pair<std::string, bool>> g_editor_node_state_array;
    int                                       g_node_depth = -1;
    void                                      DrawVecControl(const std::string& label,
                                                             MiniEngine::Vector3&    values,
                                                             float              resetValue  = 0.0f,
                                                             float              columnWidth = 100.0f);
    void                                      DrawVecControl(const std::string& label,
                                                             MiniEngine::Quaternion& values,
                                                             float              resetValue  = 0.0f,
                                                             float              columnWidth = 100.0f);

    EditorUI::EditorUI()
    {
        m_camera = g_editor_global_context.m_render_system->getRenderCamera();
        m_rendering_init_info = g_editor_global_context.m_render_system->getPathTracer()->init_info;
        const auto& asset_folder            = g_runtime_global_context.m_config_manager->getAssetFolder();
        m_editor_ui_creator["TreeNodePush"] = [this](const std::string& name, void* value_ptr) -> void {
            static ImGuiTableFlags flags      = ImGuiTableFlags_Resizable | ImGuiTableFlags_NoSavedSettings;
            bool                   node_state = false;
            g_node_depth++;
            if (g_node_depth > 0)
            {
                if (g_editor_node_state_array[g_node_depth - 1].second)
                {
                    node_state = ImGui::TreeNodeEx(name.c_str(), ImGuiTreeNodeFlags_SpanFullWidth);
                }
                else
                {
                    g_editor_node_state_array.emplace_back(std::pair(name.c_str(), node_state));
                    return;
                }
            }
            else
            {
                node_state = ImGui::TreeNodeEx(name.c_str(), ImGuiTreeNodeFlags_SpanFullWidth);
            }
            g_editor_node_state_array.emplace_back(std::pair(name.c_str(), node_state));
        };
        m_editor_ui_creator["TreeNodePop"] = [this](const std::string& name, void* value_ptr) -> void {
            if (g_editor_node_state_array[g_node_depth].second)
            {
                ImGui::TreePop();
            }
            g_editor_node_state_array.pop_back();
            g_node_depth--;
        };
        m_editor_ui_creator["Transform"] = [this](const std::string& name, void* value_ptr) -> void {
            if (g_editor_node_state_array[g_node_depth].second)
            {
                Transform* trans_ptr = static_cast<Transform*>(value_ptr);

                Vector3 degrees_val;

                degrees_val.x = trans_ptr->m_rotation.getPitch(false).valueDegrees();
                degrees_val.y = trans_ptr->m_rotation.getRoll(false).valueDegrees();
                degrees_val.z = trans_ptr->m_rotation.getYaw(false).valueDegrees();

                DrawVecControl("Position", trans_ptr->m_position);
                DrawVecControl("Rotation", degrees_val);
                DrawVecControl("Scale", trans_ptr->m_scale);

                trans_ptr->m_rotation.w = Math::cos(Math::degreesToRadians(degrees_val.x / 2)) *
                                              Math::cos(Math::degreesToRadians(degrees_val.y / 2)) *
                                              Math::cos(Math::degreesToRadians(degrees_val.z / 2)) +
                                          Math::sin(Math::degreesToRadians(degrees_val.x / 2)) *
                                              Math::sin(Math::degreesToRadians(degrees_val.y / 2)) *
                                              Math::sin(Math::degreesToRadians(degrees_val.z / 2));
                trans_ptr->m_rotation.x = Math::sin(Math::degreesToRadians(degrees_val.x / 2)) *
                                              Math::cos(Math::degreesToRadians(degrees_val.y / 2)) *
                                              Math::cos(Math::degreesToRadians(degrees_val.z / 2)) -
                                          Math::cos(Math::degreesToRadians(degrees_val.x / 2)) *
                                              Math::sin(Math::degreesToRadians(degrees_val.y / 2)) *
                                              Math::sin(Math::degreesToRadians(degrees_val.z / 2));
                trans_ptr->m_rotation.y = Math::cos(Math::degreesToRadians(degrees_val.x / 2)) *
                                              Math::sin(Math::degreesToRadians(degrees_val.y / 2)) *
                                              Math::cos(Math::degreesToRadians(degrees_val.z / 2)) +
                                          Math::sin(Math::degreesToRadians(degrees_val.x / 2)) *
                                              Math::cos(Math::degreesToRadians(degrees_val.y / 2)) *
                                              Math::sin(Math::degreesToRadians(degrees_val.z / 2));
                trans_ptr->m_rotation.z = Math::cos(Math::degreesToRadians(degrees_val.x / 2)) *
                                              Math::cos(Math::degreesToRadians(degrees_val.y / 2)) *
                                              Math::sin(Math::degreesToRadians(degrees_val.z / 2)) -
                                          Math::sin(Math::degreesToRadians(degrees_val.x / 2)) *
                                              Math::sin(Math::degreesToRadians(degrees_val.y / 2)) *
                                              Math::cos(Math::degreesToRadians(degrees_val.z / 2));
                trans_ptr->m_rotation.normalise();

                g_editor_global_context.m_scene_manager->drawSelectedEntityAxis();
            }
        };
        m_editor_ui_creator["bool"] = [this](const std::string& name, void* value_ptr)  -> void {
            if(g_node_depth == -1)
            {
                std::string label = "##" + name;
                ImGui::Text("%s", name.c_str());
                ImGui::SameLine();
                ImGui::Checkbox(label.c_str(), static_cast<bool*>(value_ptr));
            }
            else
            {
                if(g_editor_node_state_array[g_node_depth].second)
                {
                    std::string full_label = "##" + getLeafUINodeParentLabel() + name;
                    ImGui::Text("%s", name.c_str());
                    ImGui::Checkbox(full_label.c_str(), static_cast<bool*>(value_ptr));
                }
            }
        };
        m_editor_ui_creator["int"] = [this](const std::string& name, void* value_ptr) -> void {
            if (g_node_depth == -1)
            {
                std::string label = "##" + name;
                ImGui::Text("%s", name.c_str());
                ImGui::SameLine();
                ImGui::InputInt(label.c_str(), static_cast<int*>(value_ptr));
            }
            else
            {
                if (g_editor_node_state_array[g_node_depth].second)
                {
                    std::string full_label = "##" + getLeafUINodeParentLabel() + name;
                    ImGui::Text("%s", (name + ":").c_str());
                    ImGui::InputInt(full_label.c_str(), static_cast<int*>(value_ptr));
                }
            }
        };
        m_editor_ui_creator["float"] = [this](const std::string& name, void* value_ptr) -> void {
            if (g_node_depth == -1)
            {
                std::string label = "##" + name;
                ImGui::Text("%s", name.c_str());
                ImGui::SameLine();
                ImGui::InputFloat(label.c_str(), static_cast<float*>(value_ptr));
            }
            else
            {
                if (g_editor_node_state_array[g_node_depth].second)
                {
                    std::string full_label = "##" + getLeafUINodeParentLabel() + name;
                    ImGui::Text("%s", (name + ":").c_str());
                    ImGui::InputFloat(full_label.c_str(), static_cast<float*>(value_ptr));
                }
            }
        };
        m_editor_ui_creator["Vector3"] = [this](const std::string& name, void* value_ptr) -> void {
            Vector3* vec_ptr = static_cast<Vector3*>(value_ptr);
            float    val[3]  = {vec_ptr->x, vec_ptr->y, vec_ptr->z};
            if (g_node_depth == -1)
            {
                std::string label = "##" + name;
                ImGui::Text("%s", name.c_str());
                ImGui::SameLine();
                ImGui::DragFloat3(label.c_str(), val);
            }
            else
            {
                if (g_editor_node_state_array[g_node_depth].second)
                {
                    std::string full_label = "##" + getLeafUINodeParentLabel() + name;
                    ImGui::Text("%s", (name + ":").c_str());
                    ImGui::DragFloat3(full_label.c_str(), val);
                }
            }
            vec_ptr->x = val[0];
            vec_ptr->y = val[1];
            vec_ptr->z = val[2];
        };
        m_editor_ui_creator["Quaternion"] = [this](const std::string& name, void* value_ptr) -> void {
            Quaternion* qua_ptr = static_cast<Quaternion*>(value_ptr);
            float       val[4]  = {qua_ptr->x, qua_ptr->y, qua_ptr->z, qua_ptr->w};
            if (g_node_depth == -1)
            {
                std::string label = "##" + name;
                ImGui::Text("%s", name.c_str());
                ImGui::SameLine();
                ImGui::DragFloat4(label.c_str(), val);
            }
            else
            {
                if (g_editor_node_state_array[g_node_depth].second)
                {
                    std::string full_label = "##" + getLeafUINodeParentLabel() + name;
                    ImGui::Text("%s", (name + ":").c_str());
                    ImGui::DragFloat4(full_label.c_str(), val);
                }
            }
            qua_ptr->x = val[0];
            qua_ptr->y = val[1];
            qua_ptr->z = val[2];
            qua_ptr->w = val[3];
        };
        m_editor_ui_creator["std::string"] = [this, &asset_folder](const std::string& name, void* value_ptr) -> void {
            if (g_node_depth == -1)
            {
                std::string label = "##" + name;
                ImGui::Text("%s", name.c_str());
                ImGui::SameLine();
                ImGui::Text("%s", (*static_cast<std::string*>(value_ptr)).c_str());
            }
            else
            {
                if (g_editor_node_state_array[g_node_depth].second)
                {
                    std::string full_label = "##" + getLeafUINodeParentLabel() + name;
                    ImGui::Text("%s", (name + ":").c_str());
                    std::string value_str = *static_cast<std::string*>(value_ptr);
                    if (value_str.find_first_of('/') != std::string::npos)
                    {
                        std::filesystem::path value_path(value_str);
                        if (value_path.is_absolute())
                        {
                            value_path = Path::getRelativePath(asset_folder, value_path);
                        }
                        value_str = value_path.generic_string();
                        if (value_str.size() >= 2 && value_str[0] == '.' && value_str[1] == '.')
                        {
                            value_str.clear();
                        }
                    }
                    ImGui::Text("%s", value_str.c_str());
                }
            }
        };
    }

    std::string EditorUI::getLeafUINodeParentLabel()
    {
        std::string parent_label;
        int         array_size = g_editor_node_state_array.size();
        for (int index = 0; index < array_size; index++)
        {
            parent_label += g_editor_node_state_array[index].first + "::";
        }
        return parent_label;
    }

    void EditorUI::showEditorUI()
    {
        showEditorMenu(&m_editor_menu_window_open);
        // showEditorWorldObjectsWindow(&m_asset_window_open);
        showEditorGameWindow(&m_game_engine_window_open);
        // showEditorFileContentWindow(&m_file_content_window_open);
        showEditorDetailWindow(&m_detail_window_open);
    }

    void EditorUI::showEditorMenu(bool* p_open)
    {
        ImGuiDockNodeFlags dock_flags   = ImGuiDockNodeFlags_DockSpace;
        ImGuiWindowFlags   window_flags = ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_NoTitleBar |
                                        ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize |
                                        ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoBackground |
                                        ImGuiConfigFlags_NoMouseCursorChange | ImGuiWindowFlags_NoBringToFrontOnFocus;

        const ImGuiViewport* main_viewport = ImGui::GetMainViewport();
        ImGui::SetNextWindowPos(main_viewport->WorkPos, ImGuiCond_Always);
        std::array<int, 2> window_size = g_editor_global_context.m_window_system->getWindowSize();
        ImGui::SetNextWindowSize(ImVec2((float)window_size[0], (float)window_size[1]), ImGuiCond_Always);

        ImGui::SetNextWindowViewport(main_viewport->ID);

        ImGui::Begin("Editor menu", p_open, window_flags);

        ImGuiID main_docking_id = ImGui::GetID("Main Docking");
        if (ImGui::DockBuilderGetNode(main_docking_id) == nullptr)
        {
            ImGui::DockBuilderAddNode(main_docking_id, dock_flags);
            ImGui::DockBuilderSetNodePos(main_docking_id,
                                         ImVec2(main_viewport->WorkPos.x, main_viewport->WorkPos.y + 18.0f));
            ImGui::DockBuilderSetNodeSize(main_docking_id,
                                          ImVec2((float)window_size[0], (float)window_size[1] - 18.0f));

            ImGuiID center = main_docking_id;
            ImGuiID left;
            ImGuiID right = ImGui::DockBuilderSplitNode(center, ImGuiDir_Right, 0.3f, nullptr, &left);

            ImGuiID left_other;
            ImGuiID left_file_content = ImGui::DockBuilderSplitNode(left, ImGuiDir_Down, 0.30f, nullptr, &left_other);

            ImGuiID left_game_engine;
            ImGuiID left_asset =
                ImGui::DockBuilderSplitNode(left_other, ImGuiDir_Left, 0.30f, nullptr, &left_game_engine);

            // ImGui::DockBuilderDockWindow("World Objects", left_asset);
            ImGui::DockBuilderDockWindow("Inspector", right);
            // ImGui::DockBuilderDockWindow("File Content", left_file_content);
            ImGui::DockBuilderDockWindow("Scene View", left_game_engine);

            ImGui::DockBuilderFinish(main_docking_id);
        }

        ImGui::DockSpace(main_docking_id);

        if (ImGui::BeginMenuBar())
        {
            if (ImGui::BeginMenu("File"))
            {
                if (ImGui::MenuItem("Open.."))
                {
                    nfdchar_t *outPath = NULL;
                    nfdresult_t result = NFD_OpenDialog( NULL, NULL, &outPath );
                        
                    if ( result == NFD_OKAY ) 
                    {
                        //g_runtime_global_context.m_render_system->loadScene(outPath);
                        auto model = ff::AssimpLoader::load(outPath);
                        auto& render_objs = g_runtime_global_context.m_render_system->m_rtr_secene->getChildren();
                        render_objs.clear();
                        g_runtime_global_context.m_render_system->m_rtr_secene->addChild(model->mObject);
                        
                        //TODO:加载材质信息
                        
                        g_runtime_global_context.m_render_system->rtr_process_floor(g_runtime_global_context.m_render_system->m_rtr_base_env.floorPos);
                        free(outPath);
                    }
                    else if ( result == NFD_CANCEL ) {}
                    else {
                        LOG_ERROR(NFD_GetError());
                    }

                    m_error_code = 0;
                    g_is_editor_mode = true;
                }
                if (ImGui::MenuItem("Close"))
                {
                    g_runtime_global_context.m_render_system->unloadScene();
                    m_error_code = 0;
                    g_is_editor_mode = true;
                }
                ImGui::Text("-----");
                // if (ImGui::MenuItem("Save Current Scene"))
                // {
                //     g_runtime_global_context.m_world_manager->saveCurrentScene();
                // }
                if (ImGui::MenuItem("Exit"))
                {
                    g_editor_global_context.m_engine_runtime->shutdownEngine();
                    exit(0);
                }
                ImGui::EndMenu();
            }
            if (ImGui::BeginMenu("Window"))
            {
                // ImGui::MenuItem("World Objects", nullptr, &m_asset_window_open);
                ImGui::MenuItem("Scene View", nullptr, &m_game_engine_window_open);
                // ImGui::MenuItem("File Content", nullptr, &m_file_content_window_open);
                ImGui::MenuItem("Inspector", nullptr, &m_detail_window_open);
                ImGui::EndMenu();
            }
            ImGui::EndMenuBar();
        }

        ImGui::End();
    }

    void EditorUI::showEditorWorldObjectsWindow(bool* p_open)
    {
        ImGuiWindowFlags window_flags = ImGuiWindowFlags_None;

        const ImGuiViewport* main_viewport = ImGui::GetMainViewport();

        if (!*p_open)
            return;

        if (!ImGui::Begin("World Objects", p_open, window_flags))
        {
            ImGui::End();
            return;
        }

        std::shared_ptr<Scene> current_active_Scene =
            g_runtime_global_context.m_world_manager->getCurrentActiveScene().lock();
        if (current_active_Scene == nullptr)
            return;

        const SceneObjectsMap& all_gobjects = current_active_Scene->getAllGObjects();
        for (auto& id_object_pair : all_gobjects)
        {
            const GObjectID          object_id = id_object_pair.first;
            std::shared_ptr<GObject> object    = id_object_pair.second;
            const std::string        name      = object->getName();
            if (name.size() > 0)
            {
                if (ImGui::Selectable(name.c_str(),
                                      g_editor_global_context.m_scene_manager->getSelectedObjectID() == object_id))
                {
                    if (g_editor_global_context.m_scene_manager->getSelectedObjectID() != object_id)
                    {
                        g_editor_global_context.m_scene_manager->onGObjectSelected(object_id);
                    }
                    else
                    {
                        g_editor_global_context.m_scene_manager->onGObjectSelected(k_invalid_gobject_id);
                    }
                    break;
                }
            }
        }
        ImGui::End();
    }

    void EditorUI::createClassUI(Reflection::ReflectionInstance& instance)
    {
        Reflection::ReflectionInstance* reflection_instance;
        int count = instance.m_meta.getBaseClassReflectionInstanceList(reflection_instance, instance.m_instance);
        for (int index = 0; index < count; index++)
        {
            createClassUI(reflection_instance[index]);
        }
        createLeafNodeUI(instance);

        if (count > 0)
            delete[] reflection_instance;
    }

    void EditorUI::createLeafNodeUI(Reflection::ReflectionInstance& instance)
    {
        Reflection::FieldAccessor* fields;
        int                        fields_count = instance.m_meta.getFieldsList(fields);

        for (size_t index = 0; index < fields_count; index++)
        {
            auto field = fields[index];
            if (field.isArrayType())
            {
                Reflection::ArrayAccessor array_accessor;
                if (Reflection::TypeMeta::newArrayAccessorFromName(field.getFieldTypeName(), array_accessor))
                {
                    void* field_instance = field.get(instance.m_instance);
                    int   array_count    = array_accessor.getSize(field_instance);
                    m_editor_ui_creator["TreeNodePush"](
                        std::string(field.getFieldName()) + "[" + std::to_string(array_count) + "]", nullptr);
                    auto item_type_meta_item =
                        Reflection::TypeMeta::newMetaFromName(array_accessor.getElementTypeName());
                    auto item_ui_creator_iterator = m_editor_ui_creator.find(item_type_meta_item.getTypeName());
                    for (int index = 0; index < array_count; index++)
                    {
                        if (item_ui_creator_iterator == m_editor_ui_creator.end())
                        {
                            m_editor_ui_creator["TreeNodePush"]("[" + std::to_string(index) + "]", nullptr);
                            auto object_instance = Reflection::ReflectionInstance(
                                MiniEngine::Reflection::TypeMeta::newMetaFromName(item_type_meta_item.getTypeName().c_str()),
                                array_accessor.get(index, field_instance));
                            createClassUI(object_instance);
                            m_editor_ui_creator["TreeNodePop"]("[" + std::to_string(index) + "]", nullptr);
                        }
                        else
                        {
                            if (item_ui_creator_iterator == m_editor_ui_creator.end())
                            {
                                continue;
                            }
                            m_editor_ui_creator[item_type_meta_item.getTypeName()](
                                "[" + std::to_string(index) + "]", array_accessor.get(index, field_instance));
                        }
                    }
                    m_editor_ui_creator["TreeNodePop"](field.getFieldName(), nullptr);
                }
            }
            auto ui_creator_iterator = m_editor_ui_creator.find(field.getFieldTypeName());
            if (ui_creator_iterator == m_editor_ui_creator.end())
            {
                Reflection::TypeMeta field_meta =
                    Reflection::TypeMeta::newMetaFromName(field.getFieldTypeName());
                if (field.getTypeMeta(field_meta))
                {
                    auto child_instance =
                        Reflection::ReflectionInstance(field_meta, field.get(instance.m_instance));
                    m_editor_ui_creator["TreeNodePush"](field_meta.getTypeName(), nullptr);
                    createClassUI(child_instance);
                    m_editor_ui_creator["TreeNodePop"](field_meta.getTypeName(), nullptr);
                }
                else
                {
                    if (ui_creator_iterator == m_editor_ui_creator.end())
                    {
                        continue;
                    }
                    m_editor_ui_creator[field.getFieldTypeName()](field.getFieldName(),
                                                                         field.get(instance.m_instance));
                }
            }
            else
            {
                m_editor_ui_creator[field.getFieldTypeName()](field.getFieldName(),
                                                                     field.get(instance.m_instance));
            }
        }
        delete[] fields;
    }

    void EditorUI::showEditorDetailWindow(bool* p_open)
    {
        ImGuiWindowFlags window_flags = ImGuiWindowFlags_None;

        const ImGuiViewport* main_viewport = ImGui::GetMainViewport();

        if (!*p_open)
            return;

        if (!ImGui::Begin("Inspector", p_open, window_flags))
        {
            ImGui::End();
            return;
        }

        // Inspector
        ImGuiIO& io = ImGui::GetIO();

        //if (ImGui::TreeNode("Camera"))
        //{

        //    ImGui::Text("Position");
        //    ImGui::DragFloat("X", &m_camera->Position.x, 0.01f, -INFINITY, INFINITY, "%.2f", ImGuiSliderFlags_AlwaysClamp);
        //    ImGui::DragFloat("Y", &m_camera->Position.y, 0.01f, -INFINITY, INFINITY, "%.2f", ImGuiSliderFlags_AlwaysClamp);
        //    ImGui::DragFloat("Z", &m_camera->Position.z, 0.01f, -INFINITY, INFINITY, "%.2f", ImGuiSliderFlags_AlwaysClamp);
        //    
        //    ImGui::Text("Rotation");
        //    if (ImGui::DragFloat("Yaw", &m_camera->Yaw, 0.01f, -INFINITY, INFINITY, "%.2f", ImGuiSliderFlags_AlwaysClamp))
        //        m_camera->updateCameraVectors();
        //    if (ImGui::DragFloat("Pitch", &m_camera->Pitch, 0.01f, -INFINITY, INFINITY, "%.2f", ImGuiSliderFlags_AlwaysClamp))
        //        m_camera->updateCameraVectors();

        //    ImGui::Text("Field of View");
        //    ImGui::DragFloat("Degree", &m_camera->Zoom, 0.01f, 10.f, 135.f, "%.2f", ImGuiSliderFlags_AlwaysClamp);
        //    
        //    ImGui::Text("Clip Plane");
        //    ImGui::DragFloat("Near", &m_camera->Near, 0.001f, 0.001f, INFINITY, "%.2f", ImGuiSliderFlags_AlwaysClamp);
        //    ImGui::DragFloat("Far", &m_camera->Far, 1.f, 0.001f, INFINITY, "%.2f", ImGuiSliderFlags_AlwaysClamp);
        //    
        //    ImGui::Text("Depth of Field");
        //    ImGui::DragFloat("Aperture", &m_camera->Aperture, 0.001f, 0.f, 1.f, "%.2f", ImGuiSliderFlags_AlwaysClamp);
        //    ImGui::Combo("Focus Mode", &m_camera->FocusMode, "Auto\0Manual\0");
        //    if (m_camera->FocusMode)
        //        ImGui::DragFloat("Distance", &m_camera->FocusDistance, 0.01f, 0.001, INFINITY, "%.2f", ImGuiSliderFlags_AlwaysClamp);

        //    ImGui::TreePop();
        //    ImGui::Spacing();
        //}

        if (ImGui::TreeNode("Scene Config"))
        {
            bool res = false;
            //floor
            glm::vec3& floorPos = g_runtime_global_context.m_render_system->m_rtr_base_env.floorPos;
            float &metallic = g_runtime_global_context.m_render_system->m_rtr_base_env.metallic;
            float &roughness = g_runtime_global_context.m_render_system->m_rtr_base_env.roughness;
            if (ImGui::Checkbox("floor", &g_runtime_global_context.m_render_system->m_rtr_base_env.isRenderFloor))
            {
                g_runtime_global_context.m_render_system->rtr_process_floor(floorPos);
            }
            ImGui::Text("Position");
            res |= ImGui::DragFloat("F_X", &floorPos.x, 0.01f, -INFINITY, INFINITY, "%.2f", ImGuiSliderFlags_AlwaysClamp);
            res |= ImGui::DragFloat("F_Y", &floorPos.y, 0.01f, -INFINITY, INFINITY, "%.2f", ImGuiSliderFlags_AlwaysClamp);
            res |= ImGui::DragFloat("F_Z", &floorPos.z, 0.01f, -INFINITY, INFINITY, "%.2f", ImGuiSliderFlags_AlwaysClamp);
            if (res)
            {
                g_runtime_global_context.m_render_system->rtr_process_floor(floorPos);
            }
            ImGui::DragFloat("Metallic", &metallic, 0.001f, 0, 1, "%.2f", ImGuiSliderFlags_AlwaysClamp);
            ImGui::DragFloat("Roughness", &roughness, 0.001f, 0, 1, "%.2f", ImGuiSliderFlags_AlwaysClamp);
                
            //light
            ImGui::Text("Light");
            const char* items[] = { "Direction Light", "Point Light", "Area Light"};
            static int currentItem = 0;
            static int previousItem = -1;  // 保存上一次选择的值
            glm::vec3& lightPos = g_runtime_global_context.m_render_system->m_rtr_base_env.lightPos;
            glm::vec3& lightRot = g_runtime_global_context.m_render_system->m_rtr_base_env.light->rotation;
            float& intensity = g_runtime_global_context.m_render_system->m_rtr_base_env.light->mIntensity;
            if (ImGui::Combo("Type", &currentItem, items, IM_ARRAYSIZE(items)))
            {
                if (currentItem != previousItem)
                {
                    previousItem = currentItem;

                    // 根据选项执行不同的逻辑
                    switch (currentItem)
                    {
                    case 0:
                        g_runtime_global_context.m_render_system->m_rtr_base_env.light->mType = ff::DIRECTION_LIGHT;
                        break;
                    case 1:
                        g_runtime_global_context.m_render_system->m_rtr_base_env.light->mType = ff::POINT_LIGHT;
                        break;
                    case 2:
                        g_runtime_global_context.m_render_system->m_rtr_base_env.light->mType = ff::AREA_LIGHT;
                        g_runtime_global_context.m_render_system->m_rtr_base_env.light->loadMinvTexture();
                        g_runtime_global_context.m_render_system->m_rtr_base_env.light->loadFGTexture();
                        break;
                    }
                }
            }
            ImGui::Text("Position");
            res = false;
            res |= ImGui::DragFloat("L_X", &lightPos.x, 0.01f, -INFINITY, INFINITY, "%.2f", ImGuiSliderFlags_AlwaysClamp);
            res |= ImGui::DragFloat("L_Y", &lightPos.y, 0.01f, -INFINITY, INFINITY, "%.2f", ImGuiSliderFlags_AlwaysClamp);
            res |= ImGui::DragFloat("L_Z", &lightPos.z, 0.01f, -INFINITY, INFINITY, "%.2f", ImGuiSliderFlags_AlwaysClamp);
            if ( res )
            {
                g_runtime_global_context.m_render_system->m_rtr_base_env.light->updatePosition(lightPos);
                g_runtime_global_context.m_render_system->m_rtr_base_env.light->updateViewMatrix();
            }
            ImGui::Text("Rotation");
            
            //ImGui::DragFloat("Pitch", &lightRot.x, 0.01f, -INFINITY, INFINITY, "%.2f", ImGuiSliderFlags_AlwaysClamp);
            ImGui::DragFloat("Yaw", &lightRot.y, 0.02f, -180, 180, "%.2f", ImGuiSliderFlags_AlwaysClamp);
            ImGui::DragFloat("Roll", &lightRot.z, 0.02f, -180, 180, "%.2f", ImGuiSliderFlags_AlwaysClamp);

            ImGui::DragFloat("mIntensity", &intensity, 0.01f, 1, 100, "%.2f", ImGuiSliderFlags_AlwaysClamp);

            if (ImGui::Checkbox("SkyBox", &g_runtime_global_context.m_render_system->m_rtr_base_env.isRenderSkyBox))
            {
                if (g_runtime_global_context.m_render_system->m_rtr_base_env.isRenderSkyBox)
                {
                    
                    nfdchar_t* outPath = nullptr;
                    nfdresult_t result = NFD_PickFolder(nullptr, &outPath);
                    std::string folderPath;
                    if (result == NFD_OKAY)
                    {
                        folderPath = outPath; // 获取路径
                        free(outPath); // 释放内存
                        std::vector<std::string> cubePaths = {
                        folderPath + "/right.jpg",
                        folderPath + "/left.jpg",
                        folderPath + "/top.jpg",
                        folderPath + "/bottom.jpg",
                        folderPath + "/front.jpg",
                        folderPath + "/back.jpg",
                        };
                        g_runtime_global_context.m_render_system->m_rtr_base_env.skyBox = ff::CubeTextureLoader::load(cubePaths);
                        g_runtime_global_context.m_render_system->m_rtr_base_env.skyBox->setCubeTexture();
                        g_runtime_global_context.m_render_system->rtr_process_skybox();
                    }
                    else
                    {
                        g_runtime_global_context.m_render_system->m_rtr_base_env.isRenderSkyBox = false;
                    }

                }
            }
            

            ImGui::TreePop();
            ImGui::Spacing();
        }

        if (ImGui::TreeNode("Real-time Rendering"))
        {
            const char* items[] = {"PBR", "Phong"};
            static int currentItem = 0;
            static int previousItem = -1;  // 保存上一次选择的值
            if (ImGui::Combo("Options", &currentItem, items, IM_ARRAYSIZE(items)))
            {
                if (currentItem != previousItem)
                {
                    previousItem = currentItem;

                    // 根据选项执行不同的逻辑
                    switch (currentItem)
                    {
                    case 0:
                        g_runtime_global_context.m_render_system->m_rtr_secene->mSceneMaterialType = ff::SsrMaterialType;
                        g_runtime_global_context.m_render_system->updateFBO = true;
                        break;
                    case 1:
                        g_runtime_global_context.m_render_system->m_rtr_secene->mSceneMaterialType = ff::MeshPhongMaterialType;
                        g_runtime_global_context.m_render_system->updateFBO = true;
                        break;
                    }
                }
            }
            float& metallic = g_runtime_global_context.m_render_system->m_rtr_secene->metallic;
            float& roughness = g_runtime_global_context.m_render_system->m_rtr_secene->roughness;
            ImGui::DragFloat("Metallic", &metallic, 0.001f, 0, 1, "%.2f", ImGuiSliderFlags_AlwaysClamp);
            ImGui::DragFloat("Roughness", &roughness, 0.001f, 0, 1, "%.2f", ImGuiSliderFlags_AlwaysClamp);
            
            if (ImGui::Checkbox("Denoise", &g_runtime_global_context.m_render_system->mDenoise))
            {
                g_runtime_global_context.m_render_system->updateFBO = true;
            }
            if (ImGui::Checkbox("TAA", &g_runtime_global_context.m_render_system->mTaa))
            {
                g_runtime_global_context.m_render_system->updateFBO = true;
            }
            if (ImGui::Checkbox("SSAO", &g_runtime_global_context.m_render_system->mSsao))
            {
                g_runtime_global_context.m_render_system->updateFBO = true;
            }

            ImGui::TreePop();
            ImGui::Spacing();
        }

        if (ImGui::TreeNode("Offline Rendering"))
        {
            ImGui::Text("Resolution");
            ImGui::DragInt("Width", &m_rendering_init_info->Resolution.x, 1.f, 1.f, 4096.f, "%d", ImGuiSliderFlags_AlwaysClamp);
            ImGui::DragInt("Height", &m_rendering_init_info->Resolution.y, 1.f, 1.f, 4096.f, "%d", ImGuiSliderFlags_AlwaysClamp);

            ImGui::Text("Ray Tracing");
            ImGui::DragInt("Sample Count", &m_rendering_init_info->SampleCount, 1.f, 1.f, 1048576.f, "%d", ImGuiSliderFlags_AlwaysClamp);
            ImGui::DragInt("Bounce Limit", &m_rendering_init_info->BounceLimit, 1.f, 1.f, 1024.f, "%d", ImGuiSliderFlags_AlwaysClamp);
            ImGui::Checkbox("Impotance Samling", &m_rendering_init_info->ImportSample);
            ImGui::Checkbox("BVH", &m_rendering_init_info->BVH);
            ImGui::Checkbox("Multi-Thread", &m_rendering_init_info->MultiThread);
            ImGui::Checkbox("Denoise", &m_rendering_init_info->Denoise);

            ImGui::Text("Output");
            ImGui::Checkbox("Render to Disk", &m_rendering_init_info->Output);
            static char buf[128] = "";
            if (ImGui::InputText("..", buf, 128))
            {
                strcpy(m_rendering_init_info->SavePath, buf);
            }
            ImGui::SameLine();
            if (ImGui::Button("Browse"))
            {
                nfdchar_t* outPath = NULL;
                nfdresult_t result = NFD_SaveDialog(NULL, NULL, &outPath);

                if (result == NFD_OKAY)
                {
                    strcpy(buf, outPath);
                    strcpy(m_rendering_init_info->SavePath, buf);
                    free(outPath);
                }
                else if (result == NFD_CANCEL) {}
                else {
                    LOG_ERROR(NFD_GetError());
                }
            }

            ImGui::TreePop();
            ImGui::Spacing();
        }

        ImGui::End();
    }

    void EditorUI::showEditorFileContentWindow(bool* p_open)
    {
        ImGuiWindowFlags window_flags = ImGuiWindowFlags_None;

        const ImGuiViewport* main_viewport = ImGui::GetMainViewport();

        if (!*p_open)
            return;

        if (!ImGui::Begin("File Content", p_open, window_flags))
        {
            ImGui::End();
            return;
        }

        static ImGuiTableFlags flags = ImGuiTableFlags_BordersV | ImGuiTableFlags_BordersOuterH |
                                       ImGuiTableFlags_Resizable | ImGuiTableFlags_RowBg |
                                       ImGuiTableFlags_NoBordersInBody;

        if (ImGui::BeginTable("File Content", 2, flags))
        {
            ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_NoHide);
            ImGui::TableSetupColumn("Type", ImGuiTableColumnFlags_WidthFixed);
            ImGui::TableHeadersRow();

            // auto current_time = std::chrono::steady_clock::now();
            // if (current_time - m_last_file_tree_update > std::chrono::seconds(1))
            // {
            //     m_editor_file_service.buildEngineFileTree();
            //     m_last_file_tree_update = current_time;
            // }
            // m_last_file_tree_update = current_time;

            // EditorFileNode* editor_root_node = m_editor_file_service.getEditorRootNode();
            // buildEditorFileAssetsUITree(editor_root_node);
            ImGui::EndTable();
        }

        ImGui::End();
    }

    void EditorUI::showEditorGameWindow(bool* p_open)
    {
        ImGuiIO&         io           = ImGui::GetIO();
        ImGuiWindowFlags window_flags = ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_MenuBar;

        const ImGuiViewport* main_viewport = ImGui::GetMainViewport();

        if (!*p_open)
            return;

        if (!ImGui::Begin("Scene View", p_open, window_flags))
        {
            ImGui::End();
            return;
        }

        static bool trans_button_ckecked  = false;
        static bool rotate_button_ckecked = false;
        static bool scale_button_ckecked  = false;

        switch (g_editor_global_context.m_scene_manager->getEditorAxisMode())
        {
            case EditorAxisMode::TranslateMode:
                trans_button_ckecked  = true;
                rotate_button_ckecked = false;
                scale_button_ckecked  = false;
                break;
            case EditorAxisMode::RotateMode:
                trans_button_ckecked  = false;
                rotate_button_ckecked = true;
                scale_button_ckecked  = false;
                break;
            case EditorAxisMode::ScaleMode:
                trans_button_ckecked  = false;
                rotate_button_ckecked = false;
                scale_button_ckecked  = true;
                break;
            default:
                break;
        }

        if (ImGui::BeginMenuBar())
        {
            ImGui::Indent(10.f);

            if (!g_is_editor_mode)
            {
                if (g_editor_global_context.m_render_system->getPathTracer()->state && 
                    !g_editor_global_context.m_render_system->getPathTracer()->getMainLightNumber())
                    m_error_code =2;

                if (m_error_code)
                {
                    switch (m_error_code)
                    {
                    case 1:
                        ImGui::TextColored(ImVec4(1.f, 0.5f, 0.5f, 1.0f), "Error: No scenes are loaded!");
                        break;
                    case 2:
                        ImGui::TextColored(ImVec4(1.f, 0.5f, 0.5f, 1.0f), "Error: No light in the scene!");
                        break;
                    case 3:
                        ImGui::TextColored(ImVec4(1.f, 0.5f, 0.5f, 1.0f), "Error: Rendering process cancelled!");
                        break;
                    default:
                        break;
                    }
                }
                else
                {
                    switch (g_editor_global_context.m_render_system->getPathTracer()->state)
                    {
                    case 0:
                        ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "Collecting scene data...");
                        break;
                    case 1:
                        ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "Building BVH...");
                        break;
                    case 2:
                        ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "Rendering (%.2f%%)...", 
                                                  g_editor_global_context.m_render_system->getPathTracer()->progress);
                        break;
                    case 3:
                        ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "Denoising...");
                        break;
                    case 4:
                        ImGui::TextColored(ImVec4(0.5f, 1.f, 0.5f, 1.0f), "Rendering is completed in %.2fs!", 
                                                  g_editor_global_context.m_render_system->getPathTracer()->render_time);
                        break;
                    default:
                        break;
                    }
                }
            }
            else
            {
                ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f),
                                "Current editor camera move speed: [%f]",
                                g_editor_global_context.m_input_manager->getCameraSpeed());
            }

            float indent_val = 0.0f;

#if defined(__GNUC__) && defined(__MACH__)
            float indent_scale = 1.0f;
#else // Not tested on Linux
            float x_scale, y_scale;
            glfwGetWindowContentScale(g_editor_global_context.m_window_system->getWindow(), &x_scale, &y_scale);
            float indent_scale = fmaxf(1.0f, fmaxf(x_scale, y_scale));
#endif
            indent_val = g_editor_global_context.m_input_manager->getEngineWindowSize().x - 100.0f * indent_scale;

            ImGui::Indent(indent_val);
            if (g_is_editor_mode)
            {
                ImGui::PushID("Render");
                if (ImGui::Button("Render"))
                {
                    g_is_editor_mode = false;

                    g_editor_global_context.m_render_system->setupCanvas((float)m_rendering_init_info->Resolution.x/(float)m_rendering_init_info->Resolution.y,1.f);

                    if (!g_editor_global_context.m_render_system->getRenderModel())
                    {
                        m_error_code = 1;
                    }
                    else
                    {
                        m_error_code = 0;
                        g_editor_global_context.m_render_system->getPathTracer()->initializeRenderer();
                        g_editor_global_context.m_render_system->startRendering();
                    }

                }
                ImGui::PopID();
            }
            else
            {
                if (g_editor_global_context.m_render_system->getPathTracer()->state == 2)
                {
                    if (ImGui::Button(" Stop "))
                    {
                        m_error_code = 3;
                        g_editor_global_context.m_render_system->stopRendering();
                    }
                }
                else if (m_error_code ||
                         g_editor_global_context.m_render_system->getPathTracer()->state == 4)
                {
                    if (ImGui::Button(" Back "))
                    {
                        g_is_editor_mode = true;
                    }
                }
            }

            ImGui::Unindent();
            ImGui::EndMenuBar();
        }

        // GetWindowPos() ----->  X--------------------------------------------O
        //                        |                                            |
        //                        |                                            |
        // menu_bar_rect.Min -->  X--------------------------------------------O
        //                        |    It is the menu bar window.              |
        //                        |                                            |
        //                        O--------------------------------------------X  <-- menu_bar_rect.Max
        //                        |                                            |
        //                        |     It is the render target window.        |
        //                        |                                            |
        //                        O--------------------------------------------O

        Vector2 render_target_window_pos = { 0.0f, 0.0f };
        Vector2 render_target_window_size = { 0.0f, 0.0f };

        auto menu_bar_rect = ImGui::GetCurrentWindow()->MenuBarRect();

        render_target_window_pos.x = ImGui::GetWindowPos().x;
        render_target_window_pos.y = menu_bar_rect.Max.y;
        render_target_window_size.x = ImGui::GetWindowSize().x;
        render_target_window_size.y = (ImGui::GetWindowSize().y + ImGui::GetWindowPos().y) - menu_bar_rect.Max.y; // coord of right bottom point of full window minus coord of right bottom point of menu bar window.

        // if (new_window_pos != m_engine_window_pos || new_window_size != m_engine_window_size)
        {
#if defined(__MACH__)
            // The dpi_scale is not reactive to DPI changes or monitor switching, it might be a bug from ImGui.
            // Return value from ImGui::GetMainViewport()->DpiScal is always the same as first frame.
            // glfwGetMonitorContentScale and glfwSetWindowContentScaleCallback are more adaptive.
            float dpi_scale = main_viewport->DpiScale;
            g_runtime_global_context.m_render_system->updateEngineContentViewport(render_target_window_pos.x * dpi_scale,
                render_target_window_pos.y * dpi_scale,
                render_target_window_size.x * dpi_scale,
                render_target_window_size.y * dpi_scale);
#else
            g_runtime_global_context.m_render_system->updateEngineContentViewport(
                render_target_window_pos.x, render_target_window_pos.y, render_target_window_size.x, render_target_window_size.y);
#endif
            g_editor_global_context.m_input_manager->setEngineWindowPos(render_target_window_pos);
            g_editor_global_context.m_input_manager->setEngineWindowSize(render_target_window_size);
        }

        EngineContentViewport render_target_viewport;
        render_target_viewport = g_runtime_global_context.m_render_system->getEngineContentViewport();

        // std::cout<<" "<<render_target_window_pos.x
        // <<" "<<render_target_window_pos.y
        // <<" "<<render_target_window_size.x
        // <<" "<<render_target_window_size.y
        // <<std::endl;

        uint64_t texture_id = g_runtime_global_context.m_render_system->getTexColorBuffer();
        ImGui::GetWindowDrawList()->AddImage((void *)texture_id,
                                             ImVec2(render_target_window_pos.x, render_target_window_pos.y),
                                             ImVec2(render_target_window_size.x + render_target_window_pos.x, render_target_window_size.y + render_target_window_pos.y),
                                             ImVec2(0, 1),
                                             ImVec2(1, 0));

        ImGui::End();
    }

    void EditorUI::drawAxisToggleButton(const char* string_id, bool check_state, int axis_mode)
    {
        if (check_state)
        {
            ImGui::PushID(string_id);
            ImVec4 check_button_color = ImVec4(93.0f / 255.0f, 10.0f / 255.0f, 66.0f / 255.0f, 1.00f);
            ImGui::PushStyleColor(ImGuiCol_Button,
                                  ImVec4(check_button_color.x, check_button_color.y, check_button_color.z, 0.40f));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, check_button_color);
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, check_button_color);
            ImGui::Button(string_id);
            ImGui::PopStyleColor(3);
            ImGui::PopID();
        }
        else
        {
            if (ImGui::Button(string_id))
            {
                check_state = true;
                g_editor_global_context.m_scene_manager->setEditorAxisMode((EditorAxisMode)axis_mode);
                g_editor_global_context.m_scene_manager->drawSelectedEntityAxis();
            }
        }
    }

    void EditorUI::buildEditorFileAssetsUITree(EditorFileNode* node)
    {
        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        const bool is_folder = (node->m_child_nodes.size() > 0);
        if (is_folder)
        {
            bool open = ImGui::TreeNodeEx(node->m_file_name.c_str(), ImGuiTreeNodeFlags_SpanFullWidth);
            ImGui::TableNextColumn();
            ImGui::SetNextItemWidth(100.0f);
            ImGui::TextUnformatted(node->m_file_type.c_str());
            if (open)
            {
                for (int child_n = 0; child_n < node->m_child_nodes.size(); child_n++)
                    buildEditorFileAssetsUITree(node->m_child_nodes[child_n].get());
                ImGui::TreePop();
            }
        }
        else
        {
            ImGui::TreeNodeEx(node->m_file_name.c_str(),
                              ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen |
                                  ImGuiTreeNodeFlags_SpanFullWidth);
            if (ImGui::IsItemClicked() && !ImGui::IsItemToggledOpen())
            {
                onFileContentItemClicked(node);
            }
            ImGui::TableNextColumn();
            ImGui::SetNextItemWidth(100.0f);
            ImGui::TextUnformatted(node->m_file_type.c_str());
        }
    }

    void EditorUI::onFileContentItemClicked(EditorFileNode* node)
    {
        if (node->m_file_type != "object")
            return;

        std::shared_ptr<Scene> Scene = g_runtime_global_context.m_world_manager->getCurrentActiveScene().lock();
        if (Scene == nullptr)
            return;

        const unsigned int new_object_index = ++m_new_object_index_map[node->m_file_name];

        ObjectInstanceRes new_object_instance_res;
        new_object_instance_res.m_name =
            "New_" + Path::getFilePureName(node->m_file_name) + "_" + std::to_string(new_object_index);
        new_object_instance_res.m_definition =
            g_runtime_global_context.m_asset_manager->getFullPath(node->m_file_path).generic_string();

        size_t new_gobject_id = Scene->createObject(new_object_instance_res);
        if (new_gobject_id != k_invalid_gobject_id)
        {
            g_editor_global_context.m_scene_manager->onGObjectSelected(new_gobject_id);
        }
    }

    inline void windowContentScaleUpdate(float scale)
    {
#if defined(__GNUC__) && defined(__MACH__)
        float font_scale               = fmaxf(1.0f, scale);
        ImGui::GetIO().FontGlobalScale = 1.0f / font_scale;
#endif
        // TOOD: Reload fonts if DPI scale is larger than previous font loading DPI scale
    }

    inline void windowContentScaleCallback(GLFWwindow* window, float x_scale, float y_scale)
    {
        windowContentScaleUpdate(fmaxf(x_scale, y_scale));
    }

    void EditorUI::initialize(WindowUIInitInfo init_info)
    {
        std::shared_ptr<ConfigManager> config_manager = g_runtime_global_context.m_config_manager;
        ASSERT(config_manager);

        // create imgui context
        IMGUI_CHECKVERSION();
        ImGui::CreateContext();

        // set ui content scale
        float x_scale, y_scale;
        glfwGetWindowContentScale(init_info.window_system->getWindow(), &x_scale, &y_scale);
        float content_scale = fmaxf(1.0f, fmaxf(x_scale, y_scale));
        windowContentScaleUpdate(content_scale);
        glfwSetWindowContentScaleCallback(init_info.window_system->getWindow(), windowContentScaleCallback);

        ImGuiIO& io = ImGui::GetIO();
        io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
        io.ConfigDockingAlwaysTabBar         = true;
        io.ConfigWindowsMoveFromTitleBarOnly = true;
        io.Fonts->AddFontFromFileTTF(
            config_manager->getEditorFontPath().generic_string().data(), content_scale * 16, nullptr, nullptr);
        io.Fonts->Build();

        ImGuiStyle& style     = ImGui::GetStyle();
        style.WindowPadding   = ImVec2(1.0, 0);
        style.FramePadding    = ImVec2(14.0, 2.0f);
        style.ChildBorderSize = 0.0f;
        style.FrameRounding   = 5.0f;
        style.FrameBorderSize = 1.5f;

        // set imgui color style
        setUIColorStyle();

        // initialize imgui render backend
        init_info.render_system->initializeUIRenderBackend(this);
    }

    void EditorUI::setUIColorStyle()
    {
        ImGuiStyle* style  = &ImGui::GetStyle();
        ImVec4*     colors = style->Colors;

        colors[ImGuiCol_Text]                  = ImVec4(0.4745f, 0.4745f, 0.4745f, 1.00f);
        colors[ImGuiCol_TextDisabled]          = ImVec4(0.50f, 0.50f, 0.50f, 1.00f);
        colors[ImGuiCol_WindowBg]              = ImVec4(0.0078f, 0.0078f, 0.0078f, 1.00f);
        colors[ImGuiCol_ChildBg]               = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
        colors[ImGuiCol_PopupBg]               = ImVec4(0.08f, 0.08f, 0.08f, 0.94f);
        colors[ImGuiCol_Border]                = ImVec4(0.0f, 0.0f, 0.0f, 1.0f);
        colors[ImGuiCol_BorderShadow]          = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
        colors[ImGuiCol_FrameBg]               = ImVec4(0.047f, 0.047f, 0.047f, 0.5411f);
        colors[ImGuiCol_FrameBgHovered]        = ImVec4(0.196f, 0.196f, 0.196f, 0.40f);
        colors[ImGuiCol_FrameBgActive]         = ImVec4(0.294f, 0.294f, 0.294f, 0.67f);
        colors[ImGuiCol_TitleBg]               = ImVec4(0.0039f, 0.0039f, 0.0039f, 1.00f);
        colors[ImGuiCol_TitleBgActive]         = ImVec4(0.0039f, 0.0039f, 0.0039f, 1.00f);
        colors[ImGuiCol_TitleBgCollapsed]      = ImVec4(0.00f, 0.00f, 0.00f, 0.50f);
        colors[ImGuiCol_MenuBarBg]             = ImVec4(0.0f, 0.0f, 0.0f, 1.00f);
        colors[ImGuiCol_ScrollbarBg]           = ImVec4(0.02f, 0.02f, 0.02f, 0.53f);
        colors[ImGuiCol_ScrollbarGrab]         = ImVec4(0.31f, 0.31f, 0.31f, 1.00f);
        colors[ImGuiCol_ScrollbarGrabHovered]  = ImVec4(0.41f, 0.41f, 0.41f, 1.00f);
        colors[ImGuiCol_ScrollbarGrabActive]   = ImVec4(0.51f, 0.51f, 0.51f, 1.00f);
        colors[ImGuiCol_CheckMark]             = ImVec4(93.0f / 255.0f, 10.0f / 255.0f, 66.0f / 255.0f, 1.00f);
        colors[ImGuiCol_SliderGrab]            = colors[ImGuiCol_CheckMark];
        colors[ImGuiCol_SliderGrabActive]      = ImVec4(0.3647f, 0.0392f, 0.2588f, 0.50f);
        colors[ImGuiCol_Button]                = ImVec4(0.0117f, 0.0117f, 0.0117f, 1.00f);
        colors[ImGuiCol_ButtonHovered]         = ImVec4(0.0235f, 0.0235f, 0.0235f, 1.00f);
        colors[ImGuiCol_ButtonActive]          = ImVec4(0.0353f, 0.0196f, 0.0235f, 1.00f);
        colors[ImGuiCol_Header]                = ImVec4(0.1137f, 0.0235f, 0.0745f, 0.588f);
        colors[ImGuiCol_HeaderHovered]         = ImVec4(5.0f / 255.0f, 5.0f / 255.0f, 5.0f / 255.0f, 1.00f);
        colors[ImGuiCol_HeaderActive]          = ImVec4(0.0f, 0.0f, 0.0f, 1.00f);
        colors[ImGuiCol_Separator]             = ImVec4(0.0f, 0.0f, 0.0f, 0.50f);
        colors[ImGuiCol_SeparatorHovered]      = ImVec4(45.0f / 255.0f, 7.0f / 255.0f, 26.0f / 255.0f, 1.00f);
        colors[ImGuiCol_SeparatorActive]       = ImVec4(45.0f / 255.0f, 7.0f / 255.0f, 26.0f / 255.0f, 1.00f);
        colors[ImGuiCol_ResizeGrip]            = ImVec4(0.26f, 0.59f, 0.98f, 0.20f);
        colors[ImGuiCol_ResizeGripHovered]     = ImVec4(0.26f, 0.59f, 0.98f, 0.67f);
        colors[ImGuiCol_ResizeGripActive]      = ImVec4(0.26f, 0.59f, 0.98f, 0.95f);
        colors[ImGuiCol_Tab]                   = ImVec4(6.0f / 255.0f, 6.0f / 255.0f, 8.0f / 255.0f, 1.00f);
        colors[ImGuiCol_TabHovered]            = ImVec4(45.0f / 255.0f, 7.0f / 255.0f, 26.0f / 255.0f, 150.0f / 255.0f);
        colors[ImGuiCol_TabActive]             = ImVec4(47.0f / 255.0f, 6.0f / 255.0f, 29.0f / 255.0f, 1.0f);
        colors[ImGuiCol_TabUnfocused]          = ImVec4(45.0f / 255.0f, 7.0f / 255.0f, 26.0f / 255.0f, 25.0f / 255.0f);
        colors[ImGuiCol_TabUnfocusedActive]    = ImVec4(6.0f / 255.0f, 6.0f / 255.0f, 8.0f / 255.0f, 200.0f / 255.0f);
        colors[ImGuiCol_DockingPreview]        = ImVec4(47.0f / 255.0f, 6.0f / 255.0f, 29.0f / 255.0f, 0.7f);
        colors[ImGuiCol_DockingEmptyBg]        = ImVec4(0.20f, 0.20f, 0.20f, 0.00f);
        colors[ImGuiCol_PlotLines]             = ImVec4(0.61f, 0.61f, 0.61f, 1.00f);
        colors[ImGuiCol_PlotLinesHovered]      = ImVec4(1.00f, 0.43f, 0.35f, 1.00f);
        colors[ImGuiCol_PlotHistogram]         = ImVec4(0.90f, 0.70f, 0.00f, 1.00f);
        colors[ImGuiCol_PlotHistogramHovered]  = ImVec4(1.00f, 0.60f, 0.00f, 1.00f);
        colors[ImGuiCol_TableHeaderBg]         = ImVec4(0.0f, 0.0f, 0.0f, 1.00f);
        colors[ImGuiCol_TableBorderStrong]     = ImVec4(2.0f / 255.0f, 2.0f / 255.0f, 2.0f / 255.0f, 1.0f);
        colors[ImGuiCol_TableBorderLight]      = ImVec4(0.23f, 0.23f, 0.25f, 1.00f);
        colors[ImGuiCol_TableRowBg]            = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
        colors[ImGuiCol_TableRowBgAlt]         = ImVec4(1.00f, 1.00f, 1.00f, 0.06f);
        colors[ImGuiCol_TextSelectedBg]        = ImVec4(0.26f, 0.59f, 0.98f, 0.35f);
        colors[ImGuiCol_DragDropTarget]        = ImVec4(1.00f, 1.00f, 0.00f, 0.90f);
        colors[ImGuiCol_NavHighlight]          = ImVec4(0.26f, 0.59f, 0.98f, 1.00f);
        colors[ImGuiCol_NavWindowingHighlight] = ImVec4(1.00f, 1.00f, 1.00f, 0.70f);
        colors[ImGuiCol_NavWindowingDimBg]     = ImVec4(0.80f, 0.80f, 0.80f, 0.20f);
        colors[ImGuiCol_ModalWindowDimBg]      = ImVec4(0.80f, 0.80f, 0.80f, 0.35f);
    }

    void EditorUI::preRender() { showEditorUI(); }

    void DrawVecControl(const std::string& label, MiniEngine::Vector3& values, float resetValue, float columnWidth)
    {
        ImGui::PushID(label.c_str());

        ImGui::Columns(2);
        ImGui::SetColumnWidth(0, columnWidth);
        ImGui::Text("%s", label.c_str());
        ImGui::NextColumn();

        ImGui::PushMultiItemsWidths(3, ImGui::CalcItemWidth());
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2 {0, 0});

        float  lineHeight = GImGui->Font->FontSize + GImGui->Style.FramePadding.y * 2.0f;
        ImVec2 buttonSize = {lineHeight + 3.0f, lineHeight};

        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4 {0.8f, 0.1f, 0.15f, 1.0f});
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4 {0.9f, 0.2f, 0.2f, 1.0f});
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4 {0.8f, 0.1f, 0.15f, 1.0f});
        if (ImGui::Button("X", buttonSize))
            values.x = resetValue;
        ImGui::PopStyleColor(3);

        ImGui::SameLine();
        ImGui::DragFloat("##X", &values.x, 0.1f, 0.0f, 0.0f, "%.2f");
        ImGui::PopItemWidth();
        ImGui::SameLine();

        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4 {0.2f, 0.45f, 0.2f, 1.0f});
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4 {0.3f, 0.55f, 0.3f, 1.0f});
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4 {0.2f, 0.45f, 0.2f, 1.0f});
        if (ImGui::Button("Y", buttonSize))
            values.y = resetValue;
        ImGui::PopStyleColor(3);

        ImGui::SameLine();
        ImGui::DragFloat("##Y", &values.y, 0.1f, 0.0f, 0.0f, "%.2f");
        ImGui::PopItemWidth();
        ImGui::SameLine();

        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4 {0.1f, 0.25f, 0.8f, 1.0f});
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4 {0.2f, 0.35f, 0.9f, 1.0f});
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4 {0.1f, 0.25f, 0.8f, 1.0f});
        if (ImGui::Button("Z", buttonSize))
            values.z = resetValue;
        ImGui::PopStyleColor(3);

        ImGui::SameLine();
        ImGui::DragFloat("##Z", &values.z, 0.1f, 0.0f, 0.0f, "%.2f");
        ImGui::PopItemWidth();

        ImGui::PopStyleVar();

        ImGui::Columns(1);
        ImGui::PopID();
    }

    void DrawVecControl(const std::string& label, MiniEngine::Quaternion& values, float resetValue, float columnWidth)
    {
        ImGui::PushID(label.c_str());

        ImGui::Columns(2);
        ImGui::SetColumnWidth(0, columnWidth);
        ImGui::Text("%s", label.c_str());
        ImGui::NextColumn();

        ImGui::PushMultiItemsWidths(4, ImGui::CalcItemWidth());
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2 {0, 0});

        float  lineHeight = GImGui->Font->FontSize + GImGui->Style.FramePadding.y * 2.0f;
        ImVec2 buttonSize = {lineHeight + 3.0f, lineHeight};

        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4 {0.8f, 0.1f, 0.15f, 1.0f});
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4 {0.9f, 0.2f, 0.2f, 1.0f});
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4 {0.8f, 0.1f, 0.15f, 1.0f});
        if (ImGui::Button("X", buttonSize))
            values.x = resetValue;
        ImGui::PopStyleColor(3);

        ImGui::SameLine();
        ImGui::DragFloat("##X", &values.x, 0.1f, 0.0f, 0.0f, "%.2f");
        ImGui::PopItemWidth();
        ImGui::SameLine();

        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4 {0.2f, 0.45f, 0.2f, 1.0f});
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4 {0.3f, 0.55f, 0.3f, 1.0f});
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4 {0.2f, 0.45f, 0.2f, 1.0f});
        if (ImGui::Button("Y", buttonSize))
            values.y = resetValue;
        ImGui::PopStyleColor(3);

        ImGui::SameLine();
        ImGui::DragFloat("##Y", &values.y, 0.1f, 0.0f, 0.0f, "%.2f");
        ImGui::PopItemWidth();
        ImGui::SameLine();

        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4 {0.1f, 0.25f, 0.8f, 1.0f});
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4 {0.2f, 0.35f, 0.9f, 1.0f});
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4 {0.1f, 0.25f, 0.8f, 1.0f});
        if (ImGui::Button("Z", buttonSize))
            values.z = resetValue;
        ImGui::PopStyleColor(3);

        ImGui::SameLine();
        ImGui::DragFloat("##Z", &values.z, 0.1f, 0.0f, 0.0f, "%.2f");
        ImGui::PopItemWidth();
        ImGui::SameLine();

        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4 {0.5f, 0.25f, 0.5f, 1.0f});
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4 {0.6f, 0.35f, 0.6f, 1.0f});
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4 {0.5f, 0.25f, 0.5f, 1.0f});
        if (ImGui::Button("W", buttonSize))
            values.w = resetValue;
        ImGui::PopStyleColor(3);

        ImGui::SameLine();
        ImGui::DragFloat("##W", &values.w, 0.1f, 0.0f, 0.0f, "%.2f");
        ImGui::PopItemWidth();

        ImGui::PopStyleVar();

        ImGui::Columns(1);
        ImGui::PopID();
    }
} // namespace MiniEngine
