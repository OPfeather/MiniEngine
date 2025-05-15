#pragma once
#include "../global/base.h"
#include "attribute.h"
#include "../math/sphere.h"
#include "../math/box3.h"

namespace ff {

	//Geometry用来表示一个mesh的基础几何数据，里面包括了Position Color Normal Uv Tangent Bitangent等等的Attribute
	//继承了 std::enable_shared_from_this<Geometry> 类。这意味着 Geometry 类将能够使用 std::shared_ptr 提供的 shared_from_this() 方法，在其自身的成员函数中能获得指向它的 shared_ptr
	//对应一个VAO
	class Geometry:public std::enable_shared_from_this<Geometry> {
	public:

		//为Attribute的Map字典起了个别名 
		using AttributeMap = std::unordered_map<std::string, Attributef::Ptr>;

		//智能指针别名&create函数
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

		// 真正改变了OpenGL状态机，使之绑定当前的vao
		void bindVAO() noexcept;

		void setEBO() noexcept;
		
	protected:
		ID	mID{ 0 };//全局唯一id
		AttributeMap mAttributes{};//按照名称-值的方式存放了所有本Mesh的Attributes们
		Attributei::Ptr mIndexAttribute{ nullptr };//index的Attribute单独存放，并没有加到map里面

		Box3::Ptr	mBoundingBox{ nullptr };//包围盒
		Sphere::Ptr	mBoundingSphere{ nullptr };//包围球
	public:
		unsigned int vao{ 0 };
		unsigned int ebo{ 0 };
	};
}