#pragma once
#include "base.hpp"

inline float CrossProduct(const sf::Vector2f& v1, const sf::Vector2f& v2) {
	return (v1.x * v2.y) - (v1.y * v2.x);
}

inline float fastSin(float x) {
	return std::sin(x);
}

inline float fastCos(float x) {
	return std::cos(x);
}

inline sf::Vector2f FastRotate(sf::Vector2f v, float a) {
	sf::Vector2f result;
	const float cos = fastCos(a);
	const float sin = fastSin(a);

	result.x = v.x * cos - v.y * sin;
	result.y = v.x * sin + v.y * cos;
	return result;
}

// rotate using precalcuted sin and cos
inline sf::Vector2f fastRotateWithPrecalc(sf::Vector2f v, float sin, float cos) {
	sf::Vector2f result;

	result.x = v.x * cos - v.y * sin;
	result.y = v.x * sin + v.y * cos;
	return result;
}


struct aabb_t {
	aabb_t() {
		min[0] = 0.0f;
		min[1] = 0.0f;
		max[0] = 0.0f;
		max[1] = 0.0f;
	}

	aabb_t(float halfWidth, float halfHeight, sf::Vector2f pos = sf::Vector2f()) {
		min[0] = -halfWidth  + pos.x;
		min[1] = -halfHeight + pos.y;
		max[0] = halfWidth  + pos.x;
		max[1] = halfHeight + pos.y;
	}

	std::array<float, 2> min;
	std::array<float, 2> max;

	bool IsPointInside(sf::Vector2f v) {
		return min[0] <= v.x && v.x <= max[0] && min[1] <= v.y && v.y <= max[1];
	}
};

// doess aabb1 collide with aabb2
inline bool TestCollision(const aabb_t& aabb1, const aabb_t& aabb2) {
	// tooken from box2d: https://github.com/erincatto/box2d, which is licensed under Erin Cato using the MIT license

	sf::Vector2f d1, d2;
	d1 = sf::Vector2f(aabb2.min[0], aabb2.min[1]) - sf::Vector2f(aabb1.max[0], aabb1.max[1]);
	d2 = sf::Vector2f(aabb1.min[0], aabb1.min[1]) - sf::Vector2f(aabb2.max[0], aabb2.max[1]);

	if (d1.x > 0.0f || d1.y > 0.0f)
		return false;

	if (d2.x > 0.0f || d2.y > 0.0f)
		return false;

	return true;
}

template<typename T>
class indirectContainer_t {
public:
	using iterator_t = T*;

	template<typename int_t>
	indirectContainer_t(int_t size, T* data)
		: m_data(data), m_size((size_t)size) {}

	indirectContainer_t(const indirectContainer_t<T>& container) {
		m_data = container.m_data;
		m_size = container.m_size;
	}

	T* data() const {
		return m_data;
	}

	size_t size() const {
		return m_size;
	}

	size_t empty() const {
		return m_size == 0;
	}

	T* begin() { return data(); }
	T* end() { return data() + size(); }
	T* cbegin() const { return data(); }
	T* cend() const { return data() + size(); }

	template<typename int_t>
	T& operator[](int_t i) {
		static_assert(std::is_integral_v<int_t>);
		assert(i < (int_t)m_size);
		return m_data[i];
	}

	template<typename int_t>
	T& operator[](int_t i) const {
		static_assert(std::is_integral_v<int_t>);
		assert(i < (int_t)m_size);
		return m_data[i];
	}

private:
	T* m_data = nullptr;
	size_t m_size = 0;
};

enum class shapeEnum_t: u8_t {
	polygon = 0,
	circle,
	invalid
};

template<typename S>
void serialize(S& s, shapeEnum_t e) {
	s.value1b(e);
}

class shape_t {
public:
	shape_t() 
		: rot(0.0f), pos(0.0f, 0.0f) { MarkFullDirty(); }

	shape_t(sf::Vector2f pos, float rot)
		: pos(pos), rot(rot) { MarkFullDirty(); }

	virtual shapeEnum_t GetType() const = 0;

	virtual float GetRadius() const = 0;

	virtual aabb_t GetAABB() = 0;
	
	float GetRot() const { return rot; }
	void SetRot(float rot) { MarkLocalDirty(); this->rot = rot; }
	
	sf::Vector2f GetPos() const { return pos; }
	sf::Vector2f GetWeightedPos() const { return pos + GetCentroid();}
	virtual sf::Vector2f GetCentroid() const { return {0.0f, 0.0f}; }
	void SetPos(sf::Vector2f pos) { MarkLocalDirty(); this->pos = pos; }

	bool IsNetworkDirty() { return localFlags[NETWORK_DIRTY]; }
	void ResetNetworkDirty() { localFlags[NETWORK_DIRTY] = false; }

	void MarkLocalDirty() { localFlags[LOCAL_DIRTY] = true; }

	template<typename S>
	void serialize(S& s) {
		s.value4b(rot);
		s.object(pos);
	}
protected:
	virtual void Update() {} 
	
	enum shapeFlagsIndexes_t {
		LOCAL_DIRTY = 0,
		NETWORK_DIRTY = 1
	};

	void MarkFullDirty() {
		localFlags[LOCAL_DIRTY] = true;
		localFlags[NETWORK_DIRTY] = true;
	}

	std::bitset<8> localFlags;
	float rot;
	sf::Vector2f pos;
};

class circle_t : public shape_t {
public:
	circle_t()
		: shape_t(), radius(0.0f) {}

	circle_t(float radius)
		: shape_t(), radius(radius) {}

	circle_t(sf::Vector2f pos, float rot, float radius = 0.0f) 
		: shape_t(pos, rot), radius(radius) {}

	virtual shapeEnum_t GetType() const { return shapeEnum_t::circle; }

	float GetRadius() const override { return radius; }

	aabb_t GetAABB() override { 
		return aabb_t(GetRadius(), GetRadius(), pos);
	}

	void SetRadius(float radius) { this->radius = radius; MarkFullDirty(); }

	template<typename S>
	void serialize(S& s) {
		s.ext(*this, bitsery::ext::BaseClass<shape_t>{});
		s.value4b(radius);
	}
private:
	float radius;
};

// Convex polygons only.
class polygon_t : public shape_t {
public:
	typedef indirectContainer_t<sf::Vector2f> vertices_t;

	polygon_t() 
		: shape_t(), verticesCount(0), vertices(), radius(0.0f) {}

	polygon_t(const std::initializer_list<sf::Vector2f>& localVertices)
		: shape_t(), verticesCount((u8_t)localVertices.size()) {
		memcpy(vertices.data(), localVertices.begin(), verticesCount * sizeof(sf::Vector2f));
		FixVertices();
	}

	polygon_t(sf::Vector2f pos, float rot)
		: shape_t(pos, rot), verticesCount(0), vertices(), radius(0.0f) {
	}

	polygon_t(sf::Vector2f pos, float rot, const std::initializer_list<sf::Vector2f>& localVertices)
		: shape_t(pos, rot), verticesCount(localVertices.size()) {
		memcpy(vertices.data(), localVertices.begin(), verticesCount * sizeof(sf::Vector2f));
		FixVertices();
	}

	virtual sf::Vector2f GetCentroid() const override { return centroid; }

	virtual shapeEnum_t GetType() const { return shapeEnum_t::polygon; }

	float GetRadius() const override { return radius; }
	
	aabb_t GetAABB() override {
		aabb_t aabb;

		aabb.min[0] = std::numeric_limits<float>::max();
		aabb.min[1] = std::numeric_limits<float>::max();
		aabb.max[0] = std::numeric_limits<float>::min();
		aabb.max[1] = std::numeric_limits<float>::min();

		if(localFlags[LOCAL_DIRTY])
			ComputeWorldVertices();

		for(u8_t i = 0; i < verticesCount; i++) {
			sf::Vector2f vertex = cache.vertices[i];
			
			if (vertex.x < aabb.min[0]) {
				aabb.min[0] = vertex.x;
			}
			if (vertex.x > aabb.max[0]) {
				aabb.max[0] = vertex.x;
			}
			if (vertex.y < aabb.min[1]) {
				aabb.min[1] = vertex.y;
			}
			if (vertex.y > aabb.max[1]) {
				aabb.max[1] = vertex.y;
			}
		}

		return aabb;
	}

	void SetVertices(u8_t count, sf::Vector2f* localVertices) {
		verticesCount = count;
		memcpy(vertices.data(), localVertices, count * sizeof(sf::Vector2f));
		FixVertices();
		ComputeWorldVertices();
	}

	u8_t GetVerticeCount() const {
		return verticesCount;
	}

	vertices_t GetWorldVertices() {
		if(localFlags[LOCAL_DIRTY])
			ComputeWorldVertices();

		return indirectContainer_t<sf::Vector2f>(verticesCount, cache.vertices.data());
	}

	vertices_t GetWorldNormals() {
		if (localFlags[LOCAL_DIRTY])
			ComputeWorldVertices();

		return indirectContainer_t<sf::Vector2f>(verticesCount, cache.normals.data());
	}

	template<typename S>
	void serialize(S& s) {
		s.ext(*this, bitsery::ext::BaseClass<shape_t>{});
		s.value1b(verticesCount);

		for(int i = 0; i < verticesCount; i++) {
			s.object(vertices[i]);
		}
	}

protected:
	// When a set of vertices is given, we must:
	// 1. Insure Convex- Error
	// 2. Sort CCW
	// 3. Get radius
	// 4. Get normals
	bool FixVertices() {
		assert(3 <= verticesCount && verticesCount <= 8);

		centroid = { 0.0f, 0.0f };
		std::for_each(vertices.begin(), vertices.begin() + verticesCount, [&](const sf::Vector2f& vertex) {
			centroid += vertex;
			});
		centroid /= (float)verticesCount;

		radius = 0.0f;
		std::for_each(vertices.begin(), vertices.begin() + verticesCount, [&](const sf::Vector2f& vertex) {
			float distance = (centroid - vertex).length();

			if (distance > radius) {
				distance = radius;
			}
			});

		sf::Vector2f midpoint2 = { centroid.x, centroid.y + 1.0f };
		sf::Vector2f midsegment = (centroid - midpoint2).normalized();

		// sort vertices to be CCW order
		std::sort(vertices.begin(), vertices.begin() + verticesCount,
			[&](sf::Vector2f v1, sf::Vector2f v2) {
				sf::Angle a1 = midsegment.angleTo(v1 - centroid);
				sf::Angle a2 = midsegment.angleTo(v2 - centroid);

				return a1 < a2;
			});
			
		// normals
		for (size_t i = 0; i < verticesCount; i++) {
			sf::Vector2f va = vertices[i];
			sf::Vector2f vb = vertices[(i + 1) % verticesCount];

			sf::Vector2f edge = vb - va;
			edge = edge.normalized();

			normals[i] = sf::Vector2f(edge.y, -edge.x);
		}

		return true;
	}

	void ComputeWorldVertices() {
		localFlags[LOCAL_DIRTY] = false;

		for(u8_t i = 0; i < verticesCount; i++) {
			cache.vertices[i] = FastRotate(vertices[i], rot) + pos;
		}
		for (u8_t i = 0; i < verticesCount; i++) {
			cache.normals[i] = FastRotate(normals[i], rot);
		}
	}

	struct {
		std::array<sf::Vector2f, 8> vertices;
		std::array<sf::Vector2f, 8> normals;
	} cache;
private:
	float radius;

	sf::Vector2f centroid;
	u8_t verticesCount;
	std::array<sf::Vector2f, 8> vertices;
	std::array<sf::Vector2f, 8> normals;
};


struct collisionManifold_t {
	collisionManifold_t() {
		depth = std::numeric_limits<float>::max();
		normal = { 0.0f, 0.0f };
	}

	// the depth of the penetration occuring on the axis described by normal
	float depth;
	// the minimum translation vector that can be applied on either 
	// shape to reverse the collision
	sf::Vector2f normal;
};

struct projection_t {
	float min, max;
};

inline projection_t Project(const polygon_t::vertices_t& vertices, sf::Vector2f normal) {
	projection_t projection;
	projection.min = normal.dot(vertices[0]);
	projection.max = projection.min;

	for (u32_t i = 1; i < vertices.size(); i++) {
		float projected = normal.dot(vertices[i]);

		if (projected < projection.min) {
			projection.min = projected;
		}
		else if (projected > projection.max) {
			projection.max = projected;
		}
	}

	return projection;
}

inline float PointSegmentDistance(sf::Vector2f p, sf::Vector2f v1, sf::Vector2f v2, sf::Vector2f& cp) {
	// credit goes to https://www.youtube.com/watch?v=egmZJU-1zPU&ab_channel=Two-BitCoding
	// for this function, incredible channel and resource

	sf::Vector2f p_to_v1 = p - v1;
	sf::Vector2f v1_to_v2 = v2 - v1;
	float proj = p_to_v1.dot(v1_to_v2);
	float length = v1_to_v2.lengthSq();

	float d = proj / length;

	if (d <= 0.0f) {
		cp = v1;
	}
	else if (d >= 1.0f) {
		cp = v2;
	}
	else {
		cp = v1 + v1_to_v2 * d;
	}

	return (p - cp).length();
}

/**
 * @brief Performs a SAT test on only one set of normals. Returns true if the
 * a collision is detected
 *
 * @param vertices1 A set of vertices that will be tested against vertices2
 * @param vertices2 A set of vertices that will be tested against vertices1
 * @param normals1 A set of normals that were calculated from vertices 1
 * @param depth An out parameter of the depth of the penetration
 * @param normal An out parameter of the normal of the minimum translation vector
 * @return True if a collision was found, false otherwise
 */
inline bool SatHalfTest(
	const polygon_t::vertices_t& vertices1,
	const polygon_t::vertices_t& vertices2,
	const polygon_t::vertices_t& normals1,
	float& depth, sf::Vector2f& normal) {
	for (u32_t i = 0; i < normals1.size(); i++) {
		projection_t proj1 = Project(vertices1, normals1[i]);
		projection_t proj2 = Project(vertices2, normals1[i]);

		if (!(proj1.max >= proj2.min && proj2.max >= proj1.min)) {
			// they are not collding
			return false;
		}
		else {
			float new_depth = std::max(0.0f, std::min(proj1.max, proj2.max) - std::max(proj1.min, proj2.min));

			if (new_depth <= depth) {
				normal = normals1[i];
				depth = new_depth;
			}
		}
	}

	return true;
}

/**
 * @brief Takes in 2 polygons whose vertices and normals will be compared
 * to eachother to determine if a collision has occurred. Resulting information
 * found will be put into manifold
 *
 * @param shape1 a shape that will be compared to shape2 for a collision
 * @param shape2 a shape that will be compared to shape1 for a collision
 * @param manifold an output parameter. The members normal and depth will be changed, whether
 * or not a collision was found.
 * @return true if a collision was to be found, false otherwise
 */
inline bool TestCollision(polygon_t& poly1, polygon_t& poly2, collisionManifold_t& manifold) {
	auto vertices1 = poly1.GetWorldVertices();
	auto normals1 = poly1.GetWorldNormals();
	auto vertices2 = poly2.GetWorldVertices();
	auto normals2 = poly2.GetWorldNormals();

	if (!SatHalfTest(vertices1, vertices2, normals1, manifold.depth, manifold.normal))
		return false;

	if (!SatHalfTest(vertices2, vertices1, normals2, manifold.depth, manifold.normal))
		return false;

	return true;
}

inline bool TestCollision(circle_t& circle1, circle_t& circle2, collisionManifold_t& manifold) {
	float total_radius = circle1.GetRadius() + circle2.GetRadius();
	sf::Vector2f  dir = circle2.GetPos() - circle1.GetPos();
	float length = dir.length();

	if (total_radius > length) {
		manifold.normal = dir.normalized();
		manifold.depth = total_radius - length;
		return true;
	}

	return false;
}

inline bool TestCollision(polygon_t& polygon, circle_t& circle, collisionManifold_t& manifold) {
	auto vertices = polygon.GetWorldVertices();
	auto normals = polygon.GetWorldNormals();

	for (u32_t i = 0; i < vertices.size(); i++) {
		const sf::Vector2f& v1 = vertices[i];
		const sf::Vector2f& v2 = vertices[(i + 1) % vertices.size()];
		sf::Vector2f cp = {};

		float dist = PointSegmentDistance(circle.GetPos(), v1, v2, cp);
		if (dist < manifold.depth) {
			manifold.depth = dist;
			manifold.normal = normals[i];
		}
	}

	if (manifold.depth < circle.GetRadius()) {
		manifold.depth = circle.GetRadius() - manifold.depth;
		return true;
	}

	return false;
}

template<typename T>
class freeList_t {
public:
	static constexpr u32_t invalidId = std::numeric_limits<u32_t>::max();

	freeList_t() {
		nextFreeId = invalidId;
		list = {};
	}

	void SetMaxRange(size_t range) {
		if(range <= list.size())
			throw std::runtime_error("Attempt to set max range beneath current max range.");
		
		u32_t startIndex = list.size();
		list.resize(range);
		for(u32_t i = startIndex; i < list.size(); i++) {
			list[i].emplace<freeIndex_t>(i + 1, i - 1);
		}

		freeIndex_t& firstNewIndex = std::get<freeIndex_t>(list[startIndex]);
		freeIndex_t& lastNewIndex = std::get<freeIndex_t>(list.back());
	
		lastNewIndex.next = nextFreeId;
		firstNewIndex.prev = invalidId;
		nextFreeId = startIndex;
	}
	
	template<typename ... params>
	u32_t Push(params&& ... args) {
		if(nextFreeId != invalidId) {
			assert(list[nextFreeId].index() == freeIndex);

			u32_t elementIndex = nextFreeId;
			std::variant<freeIndex_t, T>& element = list[nextFreeId];
			nextFreeId = std::get<freeIndex_t>(element).next;

			if(nextFreeId != invalidId) {
				std::get<freeIndex_t>(list[nextFreeId]).prev = invalidId;
			}

			assert(std::get<freeIndex_t>(element).prev == invalidId);

			element.emplace<T>(std::forward<params>(args)...);
			return elementIndex;
		} else {
			list.emplace_back().emplace<T>(std::forward<params>(args)...);
			return list.size() - 1;
		}
	} 

	// returns index element was inserted at, invalidId otherwise
	template<typename ... params>
	u32_t Insert(u32_t index, params&& ... args) {
		if(index >= list.size()) {
			SetMaxRange(index + 1);
			return Insert(index);
		}

		if(InUse(index))
			return invalidId;

		list[index].emplace<T>(std::forward<params>(args)...);

		return index;
	}

	void Erase(u32_t index) {
		assert(list[index].index() == objectIndex);

		if(nextFreeId != invalidId)
			std::get<freeIndex_t>(list[nextFreeId]).prev = index;
		list[index] = freeIndex_t(nextFreeId, invalidId);
		nextFreeId = index;
	}

	bool InUse(u32_t index) {
		return index < list.size() && list[index].index() == objectIndex;
	}

	T& operator[](u32_t index) {
		assert(list[index].index() == objectIndex);
		return std::get<T>(list[index]);
	}

protected:
	enum {
		freeIndex = 0,
		objectIndex = 1
	};

	struct freeIndex_t {
		freeIndex_t() : next(invalidId), prev(invalidId) {}
		freeIndex_t(u32_t next, u32_t prev) : next(next), prev(prev) {} 

		u32_t next;
		u32_t prev;
	};

private:
	u32_t nextFreeId;
	std::vector<std::variant<freeIndex_t, T>> list;
};

struct spatialIndexElement_t : aabb_t {
	u32_t shapeId;
	flecs::entity entityId;

	bool operator==(const spatialIndexElement_t& other) {
		return shapeId == other.shapeId;
	}
};

struct Indexable {
	const float* min(const spatialIndexElement_t& value) const { return value.min.data(); }
	const float* max(const spatialIndexElement_t& value) const { return value.max.data(); }
};

typedef spatial::RTree<float, spatialIndexElement_t, 2, 8, 4, Indexable> spatialIndexTree_t;

struct physicsWorld_t {
public:
	static constexpr u32_t invalidId = freeList_t<int>::invalidId;

	bool DoesShapeExist(u32_t id) {
		return shapes.InUse(id);
	}

	circle_t& GetCircle(u32_t circleId) {
		assert(shapes[circleId].index() == circleIndex);
		return std::get<circle_t>(shapes[circleId]);
	}

	polygon_t& GetPolygon(u32_t polygonId) {
		assert(shapes[polygonId].index() == polygonIndex);
		return std::get<polygon_t>(shapes[polygonId]);
	}

	shape_t& GetShape(u32_t shapeId) {
		shape_t* base;
		std::visit([&](auto&& data){
				base = &data;
			}, shapes[shapeId]);

		return *base;
	}

	template<typename shape_t, typename ... params>
	u32_t CreateShape(params&& ... args) {
		u32_t newId = shapes.Push();
		shapes[newId].emplace<shape_t>(std::forward<params>(args)...);
		return newId;
	}

	template<typename shape_t, typename ... params>
	u32_t InsertShape(u32_t id, params&& ... args) {
		u32_t newId = shapes.Insert(id);
		if(newId != shapes.invalidId)
			shapes[newId].emplace<shape_t>(std::forward<params>(args)...);
		return newId;
	}

	void EraseShape(u32_t id) {
		shapes.Erase(id);
	}

	/* Inserts a shape into the spatial tree allowing for collision detection */
	void InsertShapeIntoTree(u32_t id, flecs::entity flecsId) {
		shape_t& shape = GetShape(id);
		
		spatialIndexElement_t element;
		element.entityId = flecsId;
		element.shapeId = id;
		aabb_t aabb = shape.GetAABB();
		element.min = aabb.min;
		element.max = aabb.max;

		rtree.insert(element);
	}

	spatialIndexTree_t& GetTree() { return rtree; }

	void ClearTree() {
		rtree.clear();
	}

private:
	enum {
		circleIndex = 0,
		polygonIndex = 1
	};

private:
	spatialIndexTree_t rtree;
	freeList_t<std::variant<circle_t, polygon_t>> shapes;
};

struct physicsWorldComp_t {
	std::shared_ptr<physicsWorld_t> physicsWorld;
};