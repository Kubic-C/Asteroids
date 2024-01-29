#pragma once

#include "physics.hpp"
#include "network.hpp"

/*
	Adds the neccessary systems and components to integrate physics into an ECS world
*/

struct collisionEvent_t {
    collisionEvent_t() = default;
    collisionEvent_t(collisionManifold_t manifold, flecs::entity self, flecs::entity other)
        : manifold(manifold), entitySelf(self), entityOther(other) {}

    collisionManifold_t manifold;
    flecs::entity entitySelf;
    flecs::entity entityOther;
};

inline void ShapeSet(flecs::iter& iter, physicsWorldComp_t* physicsWorld, transform_t* transforms, shapeComp_t* shapes) {
    for (auto i : iter) {
        transform_t& transform = transforms[i];
        u32_t shapeId = shapes[i].shape;

        shape_t& shape = physicsWorld->physicsWorld->GetShape(shapeId);

        shape.SetPos(transform.GetPos());
        shape.SetRot(transform.GetRot());

        physicsWorld->physicsWorld->InsertShapeIntoTree(shapeId, iter.entity(i));
    }
}

inline bool TestCollision(shape_t& shape1, shape_t& shape2, collisionManifold_t& manifold) {
    switch (shape1.GetType()) {
    case shapeEnum_t::polygon:
        switch (shape2.GetType()) {
        case shapeEnum_t::polygon:
            return TestCollision(dynamic_cast<polygon_t&>(shape1), dynamic_cast<polygon_t&>(shape2), manifold);

        case shapeEnum_t::circle:
            return TestCollision(dynamic_cast<polygon_t&>(shape1), dynamic_cast<circle_t&>(shape2), manifold);
        default:
            throw std::runtime_error("Shape invalid");
        } break;

    case shapeEnum_t::circle:
        switch (shape2.GetType()) {
        case shapeEnum_t::polygon:
            return TestCollision(dynamic_cast<polygon_t&>(shape2), dynamic_cast<circle_t&>(shape1), manifold);

        case shapeEnum_t::circle:
            return TestCollision(dynamic_cast<circle_t&>(shape1), dynamic_cast<circle_t&>(shape2), manifold);

        default:
            throw std::runtime_error("Shape invalid");
        }

    default:
        throw std::runtime_error("Shape invalid");
    }
}

inline struct {
    std::vector<spatialIndexElement_t> results;
} physicsEcsCache;

inline void ShapeCollide(flecs::iter& iter, physicsWorldComp_t* physicsWorld, shapeComp_t* shapes) {
    physicsWorld_t& world = *physicsWorld->physicsWorld;
    spatialIndexTree_t& tree = world.GetTree();

    for (auto i : iter) {
        shape_t& shape = world.GetShape(shapes->shape);
        aabb_t aabb = shape.GetAABB();

        physicsEcsCache.results.clear();
        tree.query(spatial::intersects<2>(aabb.min.data(), aabb.max.data()), std::back_inserter(physicsEcsCache.results));

        for(spatialIndexElement_t& element : physicsEcsCache.results) {
            if(element.shapeId == shapes->shape)
                continue;

            shape_t& foundShape = world.GetShape(element.shapeId);

            collisionManifold_t manifold;
            if(TestCollision(shape, foundShape, manifold)) { 
                iter.world()
                    .event<collisionEvent_t>()
                    .id<shapeComp_t>()
                    .entity(iter.entity(i))
                    .ctx(collisionEvent_t(manifold, iter.entity(i), element.entityId))
                    .emit();
            }
        } 
    }
}

inline void TransformSet(flecs::iter& iter, physicsWorldComp_t* physicsWorld, transform_t* transforms, shapeComp_t* shapes) {
    for (auto i : iter) {
        transform_t& transform = transforms[i];
        u32_t shapeId = shapes[i].shape;

        shape_t& shape = physicsWorld->physicsWorld->GetShape(shapeId);

        transform.SetPos(shape.GetPos());
        transform.SetRot(shape.GetRot());
    }
}

inline void TreeClear(flecs::iter& iter, physicsWorldComp_t* world) {
    world->physicsWorld->ClearTree();
}

inline void OnShapeDestroy(flecs::iter& iter, shapeComp_t* shapes) {
    physicsWorld_t& world = *iter.world().get<physicsWorldComp_t>()->physicsWorld;
    
    for(auto i : iter) {
        if(shapes[i].shape != physicsWorld_t::invalidId) {
            world.EraseShape(shapes[i].shape);
            shapes[i].shape = physicsWorld_t::invalidId; 
        }
    }
}

struct physicsModule_t {
    inline static flecs::entity prePhysics;
    inline static flecs::entity mainPhysics;
    inline static flecs::entity postPhysics;

	physicsModule_t(flecs::world& world) {
		prePhysics = world.entity()
            .add(flecs::Phase)
            .depends_on(flecs::OnUpdate);
	    
        mainPhysics = world.entity()
            .add(flecs::Phase)
            .depends_on(prePhysics);

        postPhysics = world.entity()
            .add(flecs::Phase)
            .depends_on(mainPhysics);

        world.system<physicsWorldComp_t, transform_t, shapeComp_t>().term_at(1).singleton().kind(prePhysics).iter(ShapeSet);
        world.system<physicsWorldComp_t, shapeComp_t>().term_at(1).singleton().kind(mainPhysics).iter(ShapeCollide);
        world.system<physicsWorldComp_t>().term_at(1).singleton().kind(postPhysics).iter(TreeClear);
        world.system<physicsWorldComp_t, transform_t, shapeComp_t>().term_at(1).singleton().kind(postPhysics).iter(TransformSet);
    }
};