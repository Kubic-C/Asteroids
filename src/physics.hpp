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

inline sf::Vector2f fastRotate(sf::Vector2f v, float a) {
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

template<typename T>
class indirect_container_t {
public:
	using iterator_t = T*;

	template<typename int_t>
	indirect_container_t(int_t size, T* data)
		: m_data(data), m_size((size_t)size) {}

	indirect_container_t(const indirect_container_t<T>& container) {
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

class shape_t {
public:
	shape_t() 
		: rot(0.0f), pos(0.0f, 0.0f), dirty(true) {}

	shape_t(sf::Vector2f pos, float rot)
		: pos(pos), rot(rot), dirty(true) {}

	virtual float GetRadius() const = 0;
	
	float GetRot() const { return rot; }
	void SetRot(float rot) { dirty = true; this->rot = rot; }
	
	sf::Vector2f GetPos() const { return pos; }
	void SetPos(sf::Vector2f pos) { dirty = true; this->pos = pos; }

	template<typename S>
	void serialize(S& s) {
		s.value1b(dirty);
		s.value4b(rot);
		s.object(pos);
	}
protected:
	virtual void Update() {} 

	bool dirty;
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

	float GetRadius() const override { return radius; }
	void SetRadius(float radius) { this->radius = radius; }

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
	typedef indirect_container_t<sf::Vector2f> vertices_t;

	polygon_t() 
		: shape_t(), verticesCount(0), vertices(), radius(0.0f) {}

	polygon_t(const std::initializer_list<sf::Vector2f>& localVertices)
		: shape_t(), verticesCount(localVertices.size()) {
		memcpy(vertices.data(), localVertices.begin(), verticesCount * sizeof(sf::Vector2f));
		FixVertices();
	}

	polygon_t(sf::Vector2f pos, float rot)
		: shape_t(pos, rot){
	}

	polygon_t(sf::Vector2f pos, float rot, const std::initializer_list<sf::Vector2f>& localVertices)
		: shape_t(pos, rot), verticesCount(localVertices.size()) {
		memcpy(vertices.data(), localVertices.begin(), verticesCount * sizeof(sf::Vector2f));
		FixVertices();
	}

	float GetRadius() const override { return radius; }
	
	void SetVertices(u8_t count, sf::Vector2f* localVertices) {
		verticesCount = count;
		memcpy(vertices.data(), localVertices, count * sizeof(sf::Vector2f));
		FixVertices();
		ComputeWorldVertices();
	}

	u8_t GetVerticeCount() const {
		return verticesCount;
	}

	sf::Vector2f GetCentroid() const {
		return centroid;
	}

	vertices_t GetWorldVertices() {
		if(dirty)
			ComputeWorldVertices();

		return indirect_container_t<sf::Vector2f>(verticesCount, cache.vertices.data());
	}

	vertices_t GetWorldNormals() {
		if (dirty)
			ComputeWorldVertices();

		return indirect_container_t<sf::Vector2f>(verticesCount, cache.normals.data());
	}

	template<typename S>
	void serialize(S& s) {
		s.ext(*this, bitsery::ext::BaseClass<shape_t>{});
		s.value4b(radius);
		s.object(centroid);
		s.value1b(verticesCount);
		s.container(vertices);
		s.container(normals);
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
		dirty = false;

		for(u8_t i = 0; i < verticesCount; i++) {
			cache.vertices[i] = fastRotate(vertices[i], rot) + pos;
		}
		for (u8_t i = 0; i < verticesCount; i++) {
			cache.normals[i] = fastRotate(vertices[i], rot);
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


struct collision_manifold_t {
	collision_manifold_t() {
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
inline bool sat_half_test(
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
inline bool TestCollision(polygon_t& poly1, polygon_t& poly2, collision_manifold_t& manifold) {
	auto vertices1 = poly1.GetWorldVertices();
	auto normals1 = poly1.GetWorldNormals();
	auto vertices2 = poly2.GetWorldVertices();
	auto normals2 = poly2.GetWorldNormals();

	if (!sat_half_test(vertices1, vertices2, normals1, manifold.depth, manifold.normal))
		return false;

	if (!sat_half_test(vertices2, vertices1, normals2, manifold.depth, manifold.normal))
		return false;

	return true;
}

inline bool TestCollision(circle_t& circle1, circle_t& circle2, collision_manifold_t& manifold) {
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

inline bool TestCollision(polygon_t& polygon, circle_t& circle, collision_manifold_t& manifold) {
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
