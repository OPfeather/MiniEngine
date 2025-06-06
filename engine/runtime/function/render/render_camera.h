#pragma once

#include <glad/glad.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include "runtime/function/global/global_context.h"

#include <vector>
#include <iostream>

namespace MiniEngine
{
    // Defines several possible options for camera movement. Used as abstraction to stay away from window-system specific input methods
    enum Camera_Movement
    {
        FORWARD,
        BACKWARD,
        LEFT,
        RIGHT,
        UP,
        DOWN
    };

    // Default camera values
    const float YAW = 90.f;
    const float PITCH = -90.f;
    const float SPEED = 2.5f;
    const float SENSITIVITY = 0.1f;
    const float ZOOM = 45.0f;
    const float ASPECT = 16.f/9.f;
    const float NEAR = 0.1f;
    const float FAR = 1000.f;

    // An abstract camera class that processes input and calculates the corresponding Euler Angles, Vectors and Matrices for use in OpenGL
    class Camera
    {
    public:
        // camera Attributes
        glm::vec3 Position;
        glm::vec3 Front;
        glm::vec3 Up;
        glm::vec3 Right;
        glm::vec3 WorldUp;
        // euler Angles
        float Yaw;
        float Pitch;
        // camera options
        float MovementSpeed;
        float MouseSensitivity;
        float Zoom;
        float Aspect;
        float Near;
        float Far;
        float Aperture{0.f};
        float FocusDistance{10.f};
        int FocusMode{0};

        glm::mat4 projection = glm::mat4(1.0f);
        glm::mat4 preProjection = glm::mat4(1.0f);
        glm::mat4 view = glm::mat4(1.0f);
        glm::mat4 preView = glm::mat4(1.0f);

        // constructor with vectors
        Camera(glm::vec3 position = glm::vec3(0.0f, 0.0f, 0.0f),
               glm::vec3 up = glm::vec3(0.0f, 1.0f, 0.0f),
               float yaw = YAW, float pitch = PITCH,
               float aspect = ASPECT,
               float near = NEAR, float far = FAR) : Front(glm::vec3(0.0f, 0.0f, -1.0f)), MovementSpeed(SPEED), MouseSensitivity(SENSITIVITY), Zoom(ZOOM)
        {
            Position = position;
            WorldUp = up;
            Yaw = yaw;
            Pitch = pitch;
            Aspect = aspect;
            Near = near;
            Far = far;
            updateCameraVectors();
        }

        // returns the view matrix calculated using Euler Angles and the LookAt Matrix
        glm::mat4 getViewMatrix()
        {
            view = glm::lookAt(Position, Position + Front, Up);
            return view;
        }

        // returns the proj matrix
        glm::mat4 getPersProjMatrix()
        {
            projection = glm::perspective(glm::radians(Zoom), Aspect, Near, Far);
            return projection;
        }

        glm::mat4 getPreViewMatrix()
        {
            return preView;
        }

        glm::mat4 getPrePersProjMatrix()
        {
            return preProjection;
        }

        void updatePreViewMatrix()
        {
            preView = view;
        }

        void updatePrePersProjMatrix()
        {
            preProjection = projection;
        }

        // returns the ortho matrix
        glm::mat4 getOrthoProjMatrix(float image_aspect)
        {   
            if (Aspect > image_aspect)
            {
                projection = glm::ortho(-Aspect, Aspect, -1.0f, 1.0f, 0.0f, 10.0f);
            }
            else
            {
                projection = glm::ortho(-image_aspect, image_aspect, -image_aspect / Aspect, image_aspect / Aspect, 0.0f, 10.0f);
            }
            return projection;
        }

        // set aspect from ui
        void setAspect(float aspect)
        {
            Aspect = aspect;
        }

        // processes input received from any keyboard-like input system. Accepts input parameter in the form of camera defined ENUM (to abstract it from windowing systems)
        void processKeyboard(Camera_Movement direction, float deltaTime)
        {
            float velocity = MovementSpeed * deltaTime;
            if (direction == FORWARD)
                Position += Front * velocity;
            if (direction == BACKWARD)
                Position -= Front * velocity;
            if (direction == LEFT)
                Position -= Right * velocity;
            if (direction == RIGHT)
                Position += Right * velocity;
            if (direction == UP)
                Position += WorldUp * velocity;
            if (direction == DOWN)
                Position -= WorldUp * velocity;
        }

        // processes input received from a mouse input system. Expects the offset value in both the x and y direction.
        void processMouseMovement(float xoffset, float yoffset, GLboolean constrainPitch = true)
        {
            xoffset *= MouseSensitivity;
            yoffset *= MouseSensitivity;

            Yaw += xoffset;
            Pitch += yoffset;

            // make sure that when pitch is out of bounds, screen doesn't get flipped
            if (constrainPitch)
            {
                if (Pitch > 89.0f)
                    Pitch = 89.0f;
                if (Pitch < -89.0f)
                    Pitch = -89.0f;
            }

            // update Front, Right and Up Vectors using the updated Euler angles
            updateCameraVectors();
        }

        // processes input received from a mouse scroll-wheel event. Only requires input on the vertical wheel-axis
        void processMouseScroll(float yoffset, int mode)
        {
            if (mode)
            {
                Zoom -= (float)yoffset;
                if (Zoom < 10.0f)
                    Zoom = 10.0f;
                if (Zoom > 90.0f)
                    Zoom = 90.0f;
            }
            else
            {
                if (yoffset > 0)
                    MovementSpeed *= 1.2f;
                else
                    MovementSpeed *= 0.8f;
            }
            
        }

        // calculates the front vector from the Camera's (updated) Euler Angles
        void updateCameraVectors()
        {
            // calculate the new Front vector
            glm::vec3 front;
            front.x = cos(glm::radians(Yaw)) * cos(glm::radians(Pitch));
            front.y = sin(glm::radians(Pitch));
            front.z = sin(glm::radians(Yaw)) * cos(glm::radians(Pitch));
            Front = glm::normalize(front);
            // also re-calculate the Right and Up vector
            Right = glm::normalize(glm::cross(Front, WorldUp)); // normalize the vectors, because their length gets closer to 0 the more you look up or down which results in slower movement.
            Up = glm::normalize(glm::cross(Right, Front));
        }
    };
}