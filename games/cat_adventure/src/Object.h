#pragma once

#include <psyqo/matrix.hh>
#include <psyqo/trigonometry.hh>
#include <psyqo/vector.hh>

#include <EASTL/vector.h>

#include <Collision.h>
#include <Graphics/SkeletonAnimator.h>
#include <Graphics/TextureInfo.h>
#include <Math/Transform.h>

struct Model;
struct FastModel;
struct Mesh;

struct Object {
    void calculateWorldMatrix(); // trashes R

    psyqo::Vec3 getFront() const
    {
        // assume no yaw
        return {
            .x = trig.sin(rotation.y),
            .y = 0.0,
            .z = trig.cos(rotation.y),
        };
    }

    psyqo::Vec3 getRight() const
    {
        // assume no yaw
        return {
            .x = trig.cos(rotation.y),
            .y = 0.0,
            .z = -trig.sin(rotation.y),
        };
    }

    void setPosition(
        const psyqo::FixedPoint<> x,
        const psyqo::FixedPoint<> y,
        const psyqo::FixedPoint<> z)
    {
        transform.translation.x = x;
        transform.translation.y = y;
        transform.translation.z = z;
    }

    void setPosition(const psyqo::Vec3& t) { transform.translation = t; }
    const psyqo::Vec3& getPosition() const { return transform.translation; }

    TransformMatrix transform; // M
    psyqo::Vector<2, 10> rotation; // rotation stored as pitch/yaw

    static psyqo::Trig<> trig;
};

struct ModelObject : Object {
    void update();

    const Model* model{nullptr};
    FastModel* fastModel{nullptr};
    TextureInfo texture;
};

struct MeshObject : Object {
    const Mesh* mesh{nullptr};
    TextureInfo texture;
    bool hasTexture{false};
};

struct AnimatedModelObject : ModelObject {
    void updateCollision();
    void update();

    // Find the yaw angle to which to rotate to to face "other"
    // Right now it's a pretty bad impl, but will do for now
    psyqo::Angle findInteractionAngle(const Object& other);

    eastl::vector<TransformMatrix> jointGlobalTransforms;
    SkeletonAnimator animator;

    Circle collisionCircle;
    Circle interactionCircle;

    psyqo::Vec3 velocity{};
};
