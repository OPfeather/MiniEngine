#include "light.h"
#include "ltc_matrix.h"

namespace ff {
    // �ж����������Ƿ���ƹ���
    bool isNearlyColinear(const glm::vec3& v1, const glm::vec3& v2, float epsilon = 1e-4f) {
        float dot = glm::dot(glm::normalize(v1), glm::normalize(v2));
        return glm::abs(glm::abs(dot) - 1.0f) < epsilon;
    }

    // ��ȫ�� lookAt ʵ��
    glm::mat4 lookAtSafe(const glm::vec3& eye, const glm::vec3& center, glm::vec3 up) {
        glm::vec3 forward = glm::normalize(center - eye);

        // ��� forward �� up �����������ߣ�ѡ��һ������� up ����
        if (isNearlyColinear(forward, up)) {
            // ѡһ���� forward �����ߵ��� up
            if (!isNearlyColinear(forward, glm::vec3(0, 0, 1))) {
                up = glm::vec3(0, 0, 1);
            }
            else {
                up = glm::vec3(1, 0, 0); // fallback
            }
        }

        return glm::lookAt(eye, center, up);
    }

    OrthographicMatrix::OrthographicMatrix(float left, float right, float bottom, float top, float near, float far) noexcept {
        mLeft = left;
        mRight = right;
        mTop = top;
        mBottom = bottom;
        mNear = near;
        mFar = far;

        updateProjectionMatrix();
    }

    OrthographicMatrix::~OrthographicMatrix() noexcept {}

    glm::mat4 OrthographicMatrix::updateProjectionMatrix() noexcept {
        mProjectionMatrix = glm::ortho(mLeft, mRight, mBottom, mTop, mNear, mFar);
        return mProjectionMatrix;
    }
    
    PerspectiveMatrix::PerspectiveMatrix(float near, float far, float aspect, float fov) noexcept {
        mNear = near;
        mFar = far;
        mAspect = aspect;
        mFov = fov;

        updateProjectionMatrix();
    }

    PerspectiveMatrix::~PerspectiveMatrix() noexcept {}

    glm::mat4 PerspectiveMatrix::updateProjectionMatrix() noexcept {
        mProjectionMatrix = glm::perspective(glm::radians(mFov), mAspect, mNear, mFar);

        return mProjectionMatrix;
    }
    
    void renderSphereLight(unsigned int& sphereVAO, unsigned int& indexCount)
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

    void renderAreaLight(unsigned int& areaLightVAO, std::vector<VertexAL> edgePos)
    {

        if (areaLightVAO == 0)
        {
            ff::VertexAL areaLightVertices[6] = {
            edgePos[0] , // 0 1 5 4
            edgePos[1] ,
            edgePos[3] ,
            edgePos[0] ,
            edgePos[3] ,
            edgePos[2] 
            };
            unsigned int areaLightVBO;
            // AREA LIGHT
            glGenVertexArrays(1, &areaLightVAO);
            glBindVertexArray(areaLightVAO);

            glGenBuffers(1, &areaLightVBO);
            glBindBuffer(GL_ARRAY_BUFFER, areaLightVBO);
            glBufferData(GL_ARRAY_BUFFER, sizeof(areaLightVertices), areaLightVertices, GL_STATIC_DRAW);

            // position
            glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(GLfloat),
                (GLvoid*)0);
            glEnableVertexAttribArray(0);

            // normal
            glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(GLfloat),
                (GLvoid*)(3 * sizeof(GLfloat)));
            glEnableVertexAttribArray(1);

            // texcoord
            glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(GLfloat),
                (GLvoid*)(6 * sizeof(GLfloat)));
            glEnableVertexAttribArray(2);
            glBindVertexArray(0);
        }

        glBindVertexArray(areaLightVAO);
        glDrawArrays(GL_TRIANGLES, 0, 6);
        glBindVertexArray(0);
    }

	Light::Light(glm::vec3 lightPos) noexcept {
        mPos = lightPos;
        mViewMatrix = lookAtSafe(mPos, glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 1.0f, 0.0f));
        mPerspectiveMatrix = PerspectiveMatrix::create(1.0f, 100.0f, 1.0, 90.0f);
        mOrthographicMatrix = OrthographicMatrix::create(-100, 100, -100, 100, 1, 100);

        //垂直于世界坐标x轴高为4，宽为4的正方形光源，法线指向x轴正方向
        edgePos.push_back({ {0, 5, -2.0f}, {1.0f, 0.0f, 0.0f}, {0.0f, 0.0f} });//右上
        edgePos.push_back({ {0, 5,  2.0f}, {1.0f, 0.0f, 0.0f}, {0.0f, 1.0f} });//左上
        edgePos.push_back({ {0, 0, -2.0f}, {1.0f, 0.0f, 0.0f}, {1.0f, 0.0f} });//右下
        edgePos.push_back({ {0, 0,  2.0f}, {1.0f, 0.0f, 0.0f}, {1.0f, 1.0f} });//左下
	}

	Light::~Light() noexcept {}

    void Light::updateViewMatrix() noexcept {
        glm::vec3 direction = glm::vec3(0.0f, 0.0f, 0.0f);
        if (mType == AREA_LIGHT)
        {
            glm::mat4 model = glm::mat4(1.0f);
            glm::vec3& lightRot = rotation;
            glm::quat quatX = glm::angleAxis(lightRot.x, glm::vec3(1.0f, 0.0f, 0.0f));
            glm::quat quatY = glm::angleAxis(lightRot.y, glm::vec3(0.0f, 1.0f, 0.0f));
            glm::quat quatZ = glm::angleAxis(lightRot.z, glm::vec3(0.0f, 0.0f, 1.0f));
            glm::quat finalQuat = quatX * quatY * quatZ; // 顺序敏感
            glm::mat4 rotationMatrix = glm::mat4_cast(finalQuat);
            direction = rotationMatrix * glm::vec4(edgePos[0].normal, 0);
            direction = mPos + direction;
        }
        mViewMatrix = lookAtSafe(mPos, direction, glm::vec3(0.0f, 1.0f, 0.0f));
    }

    void Light::updatePosition(glm::vec3 pos) noexcept {
        mPos = pos;
        updateViewMatrix();
    }

	void Light::light_shape_render()
	{
		switch(mType)
		{
		case DIRECTION_LIGHT:
		case POINT_LIGHT:
            renderSphereLight(sphereLightVAO, lightIndexCount);
            break;
        case AREA_LIGHT:
            renderAreaLight(areaLightVAO, edgePos);
            break;
        default:
            renderSphereLight(sphereLightVAO, lightIndexCount);
		}
	}

    glm::mat4 Light::getProjectionMatrix()
    {
        if (mType == DIRECTION_LIGHT || mType == AREA_LIGHT)
        {
            return mOrthographicMatrix->getProjectionMatrix();
        }
        else if (mType == POINT_LIGHT)
        {
            return mPerspectiveMatrix->getProjectionMatrix();
        }
    }

    GLuint Light::loadMinvTexture()
    {
        if (M_INV == 0)
        {
            glGenTextures(1, &M_INV);
            glBindTexture(GL_TEXTURE_2D, M_INV);

            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, 64, 64,
                0, GL_RGBA, GL_FLOAT, LTC1);

            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

            glBindTexture(GL_TEXTURE_2D, 0);
        }
        
        return M_INV;
    }

    GLuint Light::loadFGTexture()
    {
        if (FG == 0)
        {
            glGenTextures(1, &FG);
            glBindTexture(GL_TEXTURE_2D, FG);

            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, 64, 64,
                0, GL_RGBA, GL_FLOAT, LTC2);

            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

            glBindTexture(GL_TEXTURE_2D, 0);
        }
        return FG;
    }
}