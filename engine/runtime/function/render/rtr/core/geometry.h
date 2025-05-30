#pragma once
#include "../global/base.h"
#include "attribute.h"
#include "../math/sphere.h"
#include "../math/box3.h"

namespace ff {

	//Geometry������ʾһ��mesh�Ļ����������ݣ����������Position Color Normal Uv Tangent Bitangent�ȵȵ�Attribute
	//�̳��� std::enable_shared_from_this<Geometry> �ࡣ����ζ�� Geometry �ཫ�ܹ�ʹ�� std::shared_ptr �ṩ�� shared_from_this() ��������������ĳ�Ա�������ܻ��ָ������ shared_ptr
	//��Ӧһ��VAO
	class Geometry:public std::enable_shared_from_this<Geometry> {
	public:

		//ΪAttribute��Map�ֵ����˸����� 
		using AttributeMap = std::unordered_map<std::string, Attributef::Ptr>;

		//����ָ�����&create����
		using Ptr = std::shared_ptr<Geometry>;
		static Ptr create() {
			return std::make_shared <Geometry>();
		}

		Geometry() noexcept;
		
		~Geometry() noexcept;
		
		void setAttribute(const std::string& name, Attributef::Ptr attribute) noexcept;

		Attributef::Ptr getAttribute(const std::string& name) noexcept;

		void setIndex(const Attributei::Ptr& index) noexcept;

		void deleteAttribute(const std::string& name) noexcept;

		bool hasAttribute(const std::string& name) noexcept;

		const AttributeMap& getAttributes() const noexcept;

		ID getID() const noexcept;

		auto getIndex() const noexcept { return mIndexAttribute; }

		void computeBoundingBox() noexcept;

		void computeBoundingSphere() noexcept;

		Sphere::Ptr getBoundingSphere() const noexcept { return mBoundingSphere; }
		Box3::Ptr getBoundingBox() const noexcept { return mBoundingBox; }

		void setupVertexAttributes() noexcept;

		GLuint createVAO() noexcept;

		// �����ı���OpenGL״̬����ʹ֮�󶨵�ǰ��vao
		void bindVAO() noexcept;

		void setEBO() noexcept;
		
	protected:
		ID	mID{ 0 };//ȫ��Ψһid
		AttributeMap mAttributes{};//��������-ֵ�ķ�ʽ��������б�Mesh��Attributes��
		Attributei::Ptr mIndexAttribute{ nullptr };//index��Attribute������ţ���û�мӵ�map����

		Box3::Ptr	mBoundingBox{ nullptr };//��Χ��
		Sphere::Ptr	mBoundingSphere{ nullptr };//��Χ��
	public:
		unsigned int vao{ 0 };
		unsigned int ebo{ 0 };
	};
}