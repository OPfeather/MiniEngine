#pragma once

#include <glad/glad.h> // holds all OpenGL type declarations

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include "runtime/function/render/render_shader.h"

#include <string>
#include <vector>
#include "runtime/function/render/render_texture.h"

namespace MiniEngine
{
    struct Vertex
    {
        glm::vec3 Position;
        glm::vec3 Normal;
        glm::vec2 Texcoord;
        glm::vec3 Tangent;

        bool operator==(const Vertex &other) const
        {
            return Position == other.Position && Texcoord == other.Texcoord;
        }
    };

    struct Material
    {
        std::string name;

        glm::vec3 Kd; // diffuse reflectance of material, map_Kd is the texture file path.
        glm::vec3 Ks; // specular reflectance of material.
        glm::vec3 Ke; // emission intensity of light.
        glm::vec3 Tr; // transmittance of material.
        float Ns;     // shiness, the exponent of phong lobe.
        float Ni;     // Index of Refraction(IOR) of transparent object like glass and water.

        std::string map_Kd;
        std::string map_Ks;
        std::shared_ptr<Image> diffuse_map;
        std::shared_ptr<Image> specular_map;
    };

    class Mesh
    {
    public:
        // mesh Data
        std::vector<Vertex> vertices;
        std::vector<unsigned int> indices;
        Material material;

        unsigned int VAO;

        Mesh()=default;

        // constructor
        Mesh(std::vector<Vertex> vertices, std::vector<unsigned int> indices, Material material)
        {
            this->vertices = vertices;
            this->indices = indices;
            this->material = material;

            // now that we have all the required data, set the vertex buffers and its attribute pointers.
            setupMesh();
        }

        // render the mesh
        void Draw(std::shared_ptr<Shader> shader)
        {
            shader->setInt("diffuse_map", 0);
            shader->setInt("specular_map", 1);
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, diffuse_tex);
            glActiveTexture(GL_TEXTURE1);
            glBindTexture(GL_TEXTURE_2D, specular_tex);
            // draw mesh
            glBindVertexArray(VAO);
            glDrawElements(GL_TRIANGLES, static_cast<unsigned int>(indices.size()), GL_UNSIGNED_INT, 0);
            glBindVertexArray(0);

            // always good practice to set everything back to defaults once configured.
        }

    private:
        // render data
        unsigned int VBO, EBO;
        unsigned int diffuse_tex, specular_tex;

        // initializes all the buffer objects/arrays
        void setupMesh()
        {
            // create buffers/arrays
            glGenVertexArrays(1, &VAO);
            glGenBuffers(1, &VBO);
            glGenBuffers(1, &EBO);

            glBindVertexArray(VAO);
            // load data into vertex buffers
            glBindBuffer(GL_ARRAY_BUFFER, VBO);
            // A great thing about structs is that their memory layout is sequential for all its items.
            // The effect is that we can simply pass a pointer to the struct and it translates perfectly to a glm::vec3/2 array which
            // again translates to 3/2 floats which translates to a byte array.
            glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(Vertex), &vertices[0], GL_STATIC_DRAW);

            glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
            glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(unsigned int), &indices[0], GL_STATIC_DRAW);

            // set the vertex attribute pointers
            // vertex Positions
            glEnableVertexAttribArray(0);
            glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void *)0);
            // vertex normals
            glEnableVertexAttribArray(1);
            glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void *)offsetof(Vertex, Normal));
            // vertex texture coords
            glEnableVertexAttribArray(2);
            glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void *)offsetof(Vertex, Texcoord));
            // vertex tangent
            glEnableVertexAttribArray(3);
            glVertexAttribPointer(3, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void *)offsetof(Vertex, Tangent));
            glBindVertexArray(0);

            glGenTextures(1, &diffuse_tex);
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, diffuse_tex);

            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);   
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

            if (material.diffuse_map && material.diffuse_map->data)
            {
                glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, material.diffuse_map->width, material.diffuse_map->height, 0, GL_RGB, GL_UNSIGNED_BYTE, material.diffuse_map->data);
                glGenerateMipmap(GL_TEXTURE_2D);  //生成mipmap
            }
            else
            {
                std::cout << "Failed to load diffuse texture" << std::endl;
            }
            //glBindTexture(GL_TEXTURE_2D, 0);

            glGenTextures(1, &specular_tex);
            glActiveTexture(GL_TEXTURE1);
            glBindTexture(GL_TEXTURE_2D, specular_tex);

            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);   
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

            if (material.specular_map && material.specular_map->data)
            {
                glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, material.specular_map->width, material.specular_map->height, 0, GL_RGB, GL_UNSIGNED_BYTE, material.specular_map->data);
                glGenerateMipmap(GL_TEXTURE_2D);  //生成mipmap
            }
            else
            {
                std::cout << "Failed to load specular texture" << std::endl;
            }
            //glBindTexture(GL_TEXTURE_2D, 0);
        }
    };
}
