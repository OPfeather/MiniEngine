#include "geometry.h"
#include "../tools/identity.h"
#include "../global/eventDispatcher.h"

namespace ff {

	Geometry::Geometry() noexcept {
		mID = Identity::generateID();
	}

	Geometry::~Geometry() noexcept {
		//通过Dispatcher这个单例，对所有监听函数，发出本Geometry消亡的消息
		EventBase::Ptr e = EventBase::create("geometryDispose");
		e->mTarget = this;
		EventDispatcher::getInstance()->dispatchEvent(e);
	}

	void Geometry::setAttribute(const std::string& name, Attributef::Ptr attribute) noexcept {
		mAttributes[name] = attribute;
		mAttributes[name]->setVBO();
	}

	Attributef::Ptr Geometry::getAttribute(const std::string& name) noexcept {
		auto iter = mAttributes.find(name);
		if (iter != mAttributes.end()) {
			return iter->second;
		}

		return nullptr;
	}

	void Geometry::setIndex(const Attributei::Ptr& index) noexcept {
		mIndexAttribute = index;
	}

	void Geometry::deleteAttribute(const std::string& name) noexcept {
		auto iter = mAttributes.find(name);
		if (iter != mAttributes.end()) {
			mAttributes.erase(iter);
		}
	}

	bool Geometry::hasAttribute(const std::string& name) noexcept {
		auto iter = mAttributes.find(name);
		if (iter == mAttributes.end()) {
			return false;
		}

		return true;
	}

	const Geometry::AttributeMap& Geometry::getAttributes() const noexcept {
		return mAttributes;
	}

	ID Geometry::getID() const noexcept {
		return mID;
	}

	//BoundingBox是包围物体的最小盒子，只要确定盒子xyz都是最大的点和都是最小的点，就能确定这个盒子
	void Geometry::computeBoundingBox() noexcept {
		auto position = getAttribute("position");

		if(position == nullptr) {
			std::cout << "Error: geometry has no position when computeBoundingBox" << std::endl;
			return;
		}

		if (mBoundingBox == nullptr) {
			mBoundingBox = Box3::create();
		}

		mBoundingBox->setFromAttribute(position);
	}

	//BoundingBox的xyz最大的点和最小点连线的中点作为圆心，物体上的点到圆心的最大距离为半径作的球
	void Geometry::computeBoundingSphere() noexcept {
		computeBoundingBox();
		if (mBoundingSphere == nullptr) {
			mBoundingSphere = Sphere::create(glm::vec3(0.0f), 0.0f);
		}

		//包围球跟包围盒共享了一个center
		mBoundingSphere->mCenter = mBoundingBox->getCenter();

		//find smallest sphere :inscribed sphere
		auto position = getAttribute("position");
		if (position == nullptr) {
			return;
		}

		//找到距离当前球心最大距离的点
		float maxRadiusSq = 0;
		for (uint32_t i = 0; i < position->getCount(); ++i) {
			//把每个顶点的xyz装成一个point
			glm::vec3 point = glm::vec3(position->getX(i), position->getY(i), position->getZ(i));

			//计算point到center的距离
			glm::vec3 radiusVector = mBoundingSphere->mCenter - point;

			//原本应该对比每一个点到center的距离，找到最大。但是计算向量长度，必须经过开方这个运算
			//为了性能考虑，直接记录其平方，最后得到最大值，再开二次方
			maxRadiusSq = std::max(glm::dot(radiusVector, radiusVector), maxRadiusSq);
		}

		//开方求取radius
		mBoundingSphere->mRadius = std::sqrt(maxRadiusSq);
	}

	GLuint Geometry::createVAO() noexcept {
		glGenVertexArrays(1, &vao);
		return vao;
	}

	// 真正改变了OpenGL状态机，使之绑定当前的vao
	void Geometry::bindVAO() noexcept {
		glBindVertexArray(vao);
	}

	void Geometry::setupVertexAttributes() noexcept
	{
		for (const auto& iter : mAttributes) {
			auto name = iter.first;
			auto attribute = iter.second;
		
			//itemSize本attribute有多少个数字，比如position就是3个数字
			auto itemSize = attribute->getItemSize();
		
			//每个单独的数据的类型
			auto dataType = attribute->getDataType();
		
			//将本attribute的location(binding)通过attribute的name取出来
			auto bindingIter = LOCATION_MAP.find(name);
			if (bindingIter == LOCATION_MAP.end()) {
				continue;
			}
		
			auto binding = bindingIter->second;
		
			//开始向vao里面做挂钩关系
			glBindBuffer(GL_ARRAY_BUFFER, attribute->vbo);

			//激活对应的binding点
			glEnableVertexAttribArray(binding);
			//向vao里面记录，对于本binding点所对应的attribute，我们应该如何从bkAttribute->mHandle一个vbo里面读取数据
			glVertexAttribPointer(binding, itemSize, toGL(dataType), false, itemSize * toSize(dataType), (void*)0);
			
		}
		setEBO();
		glBindVertexArray(0);
	}

	void Geometry::setEBO() noexcept
	{
		glGenBuffers(1, &ebo);

		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
		auto data = mIndexAttribute->getData();
		glBufferData(GL_ELEMENT_ARRAY_BUFFER, data.size()*sizeof(uint32_t), data.data(), GL_STATIC_DRAW);
	}
}