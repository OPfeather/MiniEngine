#include "editor/include/editor_input_manager.h"

#include "editor/include/editor.h"
#include "editor/include/editor_global_context.h"
#include "editor/include/editor_scene_manager.h"

#include "runtime/engine/engine.h"
#include "runtime/function/framework/scene/scene.h"
#include "runtime/function/framework/world/world_manager.h"
#include "runtime/function/global/global_context.h"
#include "runtime/function/input/input_system.h"

#include "runtime/function/render/render_camera.h"
#include "runtime/function/render/render_system.h"
#include "runtime/function/render/window_system.h"

namespace MiniEngine
{
    void EditorInputManager::initialize() 
    { 
        m_camera = g_runtime_global_context.m_render_system->getRenderCamera();
        registerInput(); 
    }

    void EditorInputManager::tick(float delta_time) 
    { 
        processEditorCommand(delta_time); 
    }

    void EditorInputManager::registerInput()
    {
        g_editor_global_context.m_window_system->registerOnResetFunc(std::bind(&EditorInputManager::onReset, this));
        g_editor_global_context.m_window_system->registerOnCursorPosFunc(
            std::bind(&EditorInputManager::onCursorPos, this, std::placeholders::_1, std::placeholders::_2));
        g_editor_global_context.m_window_system->registerOnCursorEnterFunc(
            std::bind(&EditorInputManager::onCursorEnter, this, std::placeholders::_1));
        g_editor_global_context.m_window_system->registerOnScrollFunc(
            std::bind(&EditorInputManager::onScroll, this, std::placeholders::_1, std::placeholders::_2));
        g_editor_global_context.m_window_system->registerOnMouseButtonFunc(
            std::bind(&EditorInputManager::onMouseButtonClicked, this, std::placeholders::_1, std::placeholders::_2));
        g_editor_global_context.m_window_system->registerOnWindowCloseFunc(
            std::bind(&EditorInputManager::onWindowClosed, this));
        g_editor_global_context.m_window_system->registerOnKeyFunc(std::bind(&EditorInputManager::onKey,
                                                                             this,
                                                                             std::placeholders::_1,
                                                                             std::placeholders::_2,
                                                                             std::placeholders::_3,
                                                                             std::placeholders::_4));
    }

    void EditorInputManager::updateCursorOnAxis(Vector2 cursor_uv)
    {
        if (g_editor_global_context.m_scene_manager->getEditorCamera())
        {
            Vector2 window_size(m_engine_window_size.x, m_engine_window_size.y);
            m_cursor_on_axis = g_editor_global_context.m_scene_manager->updateCursorOnAxis(cursor_uv, window_size);
        }
    }

    void EditorInputManager::processEditorCommand(float delta_time)
    {
        if ((unsigned int)EditorCommand::camera_foward & m_editor_command)
        {
            m_camera->processKeyboard(FORWARD, delta_time);
        }
        if ((unsigned int)EditorCommand::camera_back & m_editor_command)
        {
            m_camera->processKeyboard(BACKWARD, delta_time);
        }
        if ((unsigned int)EditorCommand::camera_left & m_editor_command)
        {
            m_camera->processKeyboard(LEFT, delta_time);
        }
        if ((unsigned int)EditorCommand::camera_right & m_editor_command)
        {
            m_camera->processKeyboard(RIGHT, delta_time);
        }
        if ((unsigned int)EditorCommand::camera_up & m_editor_command)
        {
            m_camera->processKeyboard(UP, delta_time);
        }
        if ((unsigned int)EditorCommand::camera_down & m_editor_command)
        {
            m_camera->processKeyboard(DOWN, delta_time);
        }
    }

    void EditorInputManager::onKeyInEditorMode(int key, int scancode, int action, int mods)
    {
        if (action == GLFW_PRESS)
        {
            switch (key)
            {
                case GLFW_KEY_A:
                    m_editor_command |= (unsigned int)EditorCommand::camera_left;
                    break;
                case GLFW_KEY_S:
                    m_editor_command |= (unsigned int)EditorCommand::camera_back;
                    break;
                case GLFW_KEY_W:
                    m_editor_command |= (unsigned int)EditorCommand::camera_foward;
                    break;
                case GLFW_KEY_D:
                    m_editor_command |= (unsigned int)EditorCommand::camera_right;
                    break;
                case GLFW_KEY_Q:
                    m_editor_command |= (unsigned int)EditorCommand::camera_up;
                    break;
                case GLFW_KEY_E:
                    m_editor_command |= (unsigned int)EditorCommand::camera_down;
                    break;
                case GLFW_KEY_T:
                    m_editor_command |= (unsigned int)EditorCommand::translation_mode;
                    break;
                case GLFW_KEY_R:
                    m_editor_command |= (unsigned int)EditorCommand::rotation_mode;
                    break;
                case GLFW_KEY_C:
                    m_editor_command |= (unsigned int)EditorCommand::scale_mode;
                    break;
                case GLFW_KEY_DELETE:
                    m_editor_command |= (unsigned int)EditorCommand::delete_object;
                    break;
                default:
                    break;
            }
        }
        else if (action == GLFW_RELEASE)
        {
            switch (key)
            {
                case GLFW_KEY_ESCAPE:
                    m_editor_command &= (k_complement_control_command ^ (unsigned int)EditorCommand::exit);
                    break;
                case GLFW_KEY_A:
                    m_editor_command &= (k_complement_control_command ^ (unsigned int)EditorCommand::camera_left);
                    break;
                case GLFW_KEY_S:
                    m_editor_command &= (k_complement_control_command ^ (unsigned int)EditorCommand::camera_back);
                    break;
                case GLFW_KEY_W:
                    m_editor_command &= (k_complement_control_command ^ (unsigned int)EditorCommand::camera_foward);
                    break;
                case GLFW_KEY_D:
                    m_editor_command &= (k_complement_control_command ^ (unsigned int)EditorCommand::camera_right);
                    break;
                case GLFW_KEY_Q:
                    m_editor_command &= (k_complement_control_command ^ (unsigned int)EditorCommand::camera_up);
                    break;
                case GLFW_KEY_E:
                    m_editor_command &= (k_complement_control_command ^ (unsigned int)EditorCommand::camera_down);
                    break;
                case GLFW_KEY_T:
                    m_editor_command &= (k_complement_control_command ^ (unsigned int)EditorCommand::translation_mode);
                    break;
                case GLFW_KEY_R:
                    m_editor_command &= (k_complement_control_command ^ (unsigned int)EditorCommand::rotation_mode);
                    break;
                case GLFW_KEY_C:
                    m_editor_command &= (k_complement_control_command ^ (unsigned int)EditorCommand::scale_mode);
                    break;
                case GLFW_KEY_DELETE:
                    m_editor_command &= (k_complement_control_command ^ (unsigned int)EditorCommand::delete_object);
                    break;
                default:
                    break;
            }
        }
    }

    void EditorInputManager::onKey(int key, int scancode, int action, int mods)
    {
        onKeyInEditorMode(key, scancode, action, mods);
    }

    void EditorInputManager::onReset()
    {
        // to do
    }

    void EditorInputManager::onCursorPos(double xpos, double ypos)
    {
        if (g_is_editor_mode)
        {
            float angularVelocity =
                180.0f / Math::max(m_engine_window_size.x, m_engine_window_size.y); // 180 degrees while moving full screen
            if (m_mouse_x >= 0.0f && m_mouse_y >= 0.0f)
            {
                if (g_editor_global_context.m_window_system->isMouseButtonDown(GLFW_MOUSE_BUTTON_RIGHT))
                {
                    glfwSetInputMode(
                        g_editor_global_context.m_window_system->getWindow(), GLFW_CURSOR, GLFW_CURSOR_DISABLED);
                        m_camera->processMouseMovement(xpos - m_mouse_x, m_mouse_y - ypos);
                }
                else if (g_editor_global_context.m_window_system->isMouseButtonDown(GLFW_MOUSE_BUTTON_LEFT))
                {
                    g_editor_global_context.m_scene_manager->moveEntity(
                        xpos,
                        ypos,
                        m_mouse_x,
                        m_mouse_y,
                        m_engine_window_pos,
                        m_engine_window_size,
                        m_cursor_on_axis,
                        g_editor_global_context.m_scene_manager->getSelectedObjectMatrix());
                    glfwSetInputMode(g_editor_global_context.m_window_system->getWindow(), GLFW_CURSOR, GLFW_CURSOR_NORMAL);
                }
                else
                {
                    glfwSetInputMode(g_editor_global_context.m_window_system->getWindow(), GLFW_CURSOR, GLFW_CURSOR_NORMAL);

                    if (isCursorInRect(m_engine_window_pos, m_engine_window_size))
                    {
                        Vector2 cursor_uv = Vector2((m_mouse_x - m_engine_window_pos.x) / m_engine_window_size.x,
                                                    (m_mouse_y - m_engine_window_pos.y) / m_engine_window_size.y);
                        updateCursorOnAxis(cursor_uv);
                    }
                }
            }
            m_mouse_x = xpos;
            m_mouse_y = ypos;
        }
    }

    void EditorInputManager::onCursorEnter(int entered)
    {
        if (!entered) // lost focus
        {
            m_mouse_x = m_mouse_y = -1.0f;
        }
    }

    void EditorInputManager::onScroll(double xoffset, double yoffset)
    {
        if (isCursorInRect(m_engine_window_pos, m_engine_window_size))
        {
            if (g_editor_global_context.m_window_system->isMouseButtonDown(GLFW_MOUSE_BUTTON_RIGHT))
            {
                m_camera->processMouseScroll(yoffset, 0);
                m_camera_speed = m_camera->MovementSpeed;
            }
            else
            {
                m_camera->processMouseScroll(yoffset, 1);
            }
        }
    }

    void EditorInputManager::onMouseButtonClicked(int key, int action)
    {
        if (m_cursor_on_axis != 3)
            return;

        std::shared_ptr<Scene> current_active_scene = g_runtime_global_context.m_world_manager->getCurrentActiveScene().lock();
        if (current_active_scene == nullptr)
            return;

        // if (isCursorInRect(m_engine_window_pos, m_engine_window_size))
        // {
        //     if (key == GLFW_MOUSE_BUTTON_LEFT)
        //     {
        //         Vector2 picked_uv((m_mouse_x - m_engine_window_pos.x) / m_engine_window_size.x,
        //                           (m_mouse_y - m_engine_window_pos.y) / m_engine_window_size.y);
        //         size_t  select_mesh_id = g_editor_global_context.m_scene_manager->getGuidOfPickedMesh(picked_uv);

        //         size_t gobject_id = g_editor_global_context.m_render_system->getGObjectIDByMeshID(select_mesh_id);
        //         g_editor_global_context.m_scene_manager->onGObjectSelected(gobject_id);
        //     }
        // }
    }

    void EditorInputManager::onWindowClosed() { g_editor_global_context.m_engine_runtime->shutdownEngine(); }

    bool EditorInputManager::isCursorInRect(Vector2 pos, Vector2 size) const
    {
        return pos.x <= m_mouse_x && m_mouse_x <= pos.x + size.x && pos.y <= m_mouse_y && m_mouse_y <= pos.y + size.y;
    }
} // namespace MiniEngine