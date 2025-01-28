#pragma once

#include <psyqo/fixed-point.hh>
#include <psyqo/matrix.hh>
#include <psyqo/trigonometry.hh>
#include <psyqo/vector.hh>

#include <EASTL/vector.h>

#include <Collision.h>
#include <Core/Timer.h>
#include <Graphics/Model.h>
#include <Graphics/SkeletonAnimator.h>
#include <Graphics/TextureInfo.h>
#include <Math/Math.h>
#include <Math/Transform.h>

struct Object {
    void calculateWorldMatrix(); // trashes R

    bool hasRotation() const { return rotation.y.value != 0 || rotation.x.value != 0; }

    psyqo::Vec3 getFront() const
    {
        // assume no pitch
        return {
            .x = trig.sin(rotation.y),
            .y = 0.0,
            .z = trig.cos(rotation.y),
        };
    }

    psyqo::Vec3 getRight() const
    {
        // assume no pitch
        return {
            .x = trig.cos(rotation.y),
            .y = 0.0,
            .z = -trig.sin(rotation.y),
        };
    }

    void setPosition(const psyqo::FixedPoint<> x,
        const psyqo::FixedPoint<> y,
        const psyqo::FixedPoint<> z)
    {
        transform.translation.x = x;
        transform.translation.y = y;
        transform.translation.z = z;
    }

    void setPosition(const psyqo::Vec3& t) { transform.translation = t; }
    const psyqo::Vec3& getPosition() const { return transform.translation; }

    void setYaw(psyqo::Angle a)
    {
        math::normalizeAngle(a);
        rotation.y = a;
    }

    void setYaw(psyqo::Angle a, bool resetTarget)
    {
        setYaw(a);
        if (resetTarget) {
            targetYaw = a;
            stopRotation();
        }
    }

    void stopRotation()
    {
        rotationTime = 0.0;
        rotationTime = 0.0;
    }

    bool isRotating() const { return rotationTime != 0.0; }

    psyqo::Angle getYaw() const { return rotation.y; }

    static psyqo::Trig<> trig;

    TransformMatrix transform; // M

    // if rotationSpeed = 0.0 - use current speed
    void rotateTowards(const Object& other, psyqo::FixedPoint<> rotationSpeed = 0.0);
    void rotateTowards(psyqo::Angle angle, psyqo::FixedPoint<> rotationSpeed = 0.0);

    // Find the yaw angle to which to rotate to to face "other"
    psyqo::Angle findFacingAngle(const psyqo::Vec3& pos);
    psyqo::Angle findFacingAngle(const Object& other);

protected:
    psyqo::Vector<2, 10> rotation{}; // rotation stored as pitch/yaw

    // TODO: move into AnimatedModelObject?
    psyqo::Angle startYaw{0.0};
    psyqo::Angle targetYaw{0.0};
    std::uint32_t rotationTimer{};
    std::uint32_t rotationTime{};
    psyqo::FixedPoint<> rotationSpeed{1.0}; // half turns / second
};

struct ModelObject : Object {
    void update();

    Model model;
};

struct MeshObject : Object {
    Mesh mesh;
    bool hasTexture{false};
};

inline constexpr StringHash DEFAULT_FACE_ANIMATION = "Default"_sh;
inline constexpr StringHash DEFAULT_BLINK_FACE_ANIMATION = "Blink"_sh;
inline constexpr StringHash THINK_FACE_ANIMATION = "Think"_sh;
inline constexpr StringHash ANGRY_BLINK_FACE_ANIMATION = "AngryBlink"_sh;
inline constexpr StringHash ANGRY_FACE_ANIMATION = "Angry"_sh;
inline constexpr StringHash SHOCKED_FACE_ANIMATION = "Shocked"_sh;

struct AnimatedModelObject : ModelObject {
    void updateCollision();
    void update(std::uint32_t dt);

    eastl::vector<TransformMatrix> jointGlobalTransforms;
    SkeletonAnimator animator;

    std::uint8_t faceSubmeshIdx{0xFF};
    std::uint8_t faceOffsetU{0};
    std::uint8_t faceOffsetV{0};

    Timer blinkTimer;
    bool isInBlink{false};

    uint32_t blinkPeriod;
    uint32_t closedEyesTime;

    StringHash currentFaceAnimation;

    void setFaceAnimation(std::uint8_t faceOffsetU, std::uint8_t faceOffsetV);
    void setFaceAnimation(StringHash faceName, bool updateCurrent = true);

    static void shiftUVs(MeshData& mesh, int offsetU, int offsetV);

    Circle collisionCircle;
    Circle interactionCircle;

    psyqo::Vec3 velocity{};
};
