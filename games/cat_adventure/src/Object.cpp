#include "Object.h"

#include <psyqo/gpu.hh>
#include <psyqo/gte-kernels.hh>
#include <psyqo/gte-registers.hh>

#include <psyqo/soft-math.hh>

#include <EASTL/fixed_string.h>
#include <common/syscalls/syscalls.h>
#include <psyqo/xprintf.h>

#include <Camera.h>
#include <Graphics/Model.h>
#include <Math/Math.h>
#include <Math/Transform.h>
#include <Math/gte-math.h>

namespace
{
constexpr psyqo::Matrix33 I = psyqo::Matrix33{{{1, 0, 0}, {0, 1, 0}, {0, 0, 1}}};
}

psyqo::Trig<> Object::trig{};

void Object::calculateWorldMatrix()
{
    if (rotation.x == 0.0 && rotation.y == 0.0) {
        transform.rotation = I;
        return;
    }

    // yaw
    getRotationMatrix33RH(&transform.rotation, rotation.y, psyqo::SoftMath::Axis::Y, trig);

    if (rotation.x == 0.0) {
        return;
    }

    // pitch
    psyqo::Matrix33 rotX;
    getRotationMatrix33RH(&rotX, rotation.x, psyqo::SoftMath::Axis::X, trig);
    psyqo::GTE::Math::multiplyMatrix33<psyqo::GTE::PseudoRegister::Rotation,
        psyqo::GTE::PseudoRegister::V0>(transform.rotation, rotX, &transform.rotation);
}

psyqo::Angle Object::findFacingAngle(const psyqo::Vec3& pos)
{
    const auto diff = pos - getPosition();
    return math::atan2(diff.x, diff.z);
}

psyqo::Angle Object::findFacingAngle(const Object& other)
{
    return findFacingAngle(other.getPosition());
}

void Object::rotateTowards(const Object& other, psyqo::FixedPoint<> newRotationSpeed)
{
    rotateTowards(findFacingAngle(other), newRotationSpeed);
}

void Object::rotateTowards(psyqo::Angle angle, psyqo::FixedPoint<> newRotationSpeed)
{
    if (angle == targetYaw) {
        return;
    }

    if (newRotationSpeed == 0.0) {
        newRotationSpeed = rotationSpeed;
    } else {
        rotationSpeed = newRotationSpeed;
    }

    startYaw = getYaw();
    targetYaw = angle;

    auto diff = targetYaw - startYaw;
    if (diff > 1.0) {
        diff -= 2.0;
    } else if (diff < -1.0) {
        diff += 2.0;
    }

    if (diff.abs() < 0.001) {
        setYaw(targetYaw, true);
        return;
    }

    // in microseconds
    rotationTimer = 0;
    rotationTime = (psyqo::FixedPoint<>(diff) / newRotationSpeed * 1000.0).abs().integer() * 1000;
}

void AnimatedModelObject::updateCollision()
{
    collisionCircle.center = getPosition();
    collisionCircle.radius = 0.05;

    interactionCircle.center = getPosition() + getFront() * 0.05;
    interactionCircle.radius = 0.07;
}

StringHash getBlinkAnimationName(StringHash faceName)
{
    // hardcoded for cat now, TODO: implement a generic func later
    switch (faceName.value) {
    case DEFAULT_FACE_ANIMATION.value:
    case THINK_FACE_ANIMATION.value:
    case ANGRY_FACE_ANIMATION.value:
        return ANGRY_BLINK_FACE_ANIMATION;
    }

    return DEFAULT_BLINK_FACE_ANIMATION;
}

void AnimatedModelObject::update(std::uint32_t dt)
{
    calculateWorldMatrix();
    animator.update();

    animator.animate(model.armature, jointGlobalTransforms);

    if (useManualHeadRotation) {
        auto& rot = model.armature.joints[4].localTransform.rotation;
        rot = manualHeadRotation;
    }

    model.armature.calculateTransforms(jointGlobalTransforms);

    if (faceSubmeshIdx != 0xFF) {
        blinkTimer.update();
        if (!isInBlink) {
            if (blinkTimer.tick()) {
                isInBlink = true;

                setFaceAnimation(getBlinkAnimationName(currentFaceAnimation), false);
                blinkTimer.reset(closedEyesTime);
            }
        } else {
            if (blinkTimer.tick()) {
                isInBlink = false;
                setFaceAnimation(currentFaceAnimation, false);
                blinkTimer.reset(blinkPeriod);
            }
        }
    }

    if (rotationTime != 0) {
        rotationTimer += dt;
        if (rotationTimer > rotationTime) {
            setYaw(targetYaw, true);
        } else {
            const auto lerpF = psyqo::FixedPoint<>(rotationTimer / 1000, 0) /
                               psyqo::FixedPoint<>(rotationTime / 1000, 0);
            const auto yaw = math::lerpAngle(startYaw, targetYaw, psyqo::FixedPoint<10>(lerpF));
            setYaw(yaw, false);
        }
    }
}

void AnimatedModelObject::setFaceAnimation(std::uint8_t faceU, std::uint8_t faceV)
{
    if (faceSubmeshIdx == 0xFF) {
        return;
    }

    const auto offsetU = faceU - faceOffsetU;
    const auto offsetV = faceV - faceOffsetV;
    shiftUVs(const_cast<MeshData&>(*model.meshes[faceSubmeshIdx].meshData), offsetU, offsetV);

    faceOffsetU = faceU;
    faceOffsetV = faceV;
}

void AnimatedModelObject::shiftUVs(MeshData& mesh, int offsetU, int offsetV)
{
    for (auto& gt3 : mesh.gt3) {
        gt3.uvA.u += offsetU;
        gt3.uvA.v += offsetV;
        gt3.uvB.u += offsetU;
        gt3.uvB.v += offsetV;
        gt3.uvC.u += offsetU;
        gt3.uvC.v += offsetV;
    }
    for (auto& gt4 : mesh.gt4) {
        gt4.uvA.u += offsetU;
        gt4.uvA.v += offsetV;
        gt4.uvB.u += offsetU;
        gt4.uvB.v += offsetV;
        gt4.uvC.u += offsetU;
        gt4.uvC.v += offsetV;
        gt4.uvD.u += offsetU;
        gt4.uvD.v += offsetV;
    }
}

void AnimatedModelObject::setFaceAnimation(StringHash faceName, bool updateCurrent)
{
    // hardcoded for cat now, TODO: implement a generic func later
    switch (faceName.value) {
    case DEFAULT_FACE_ANIMATION.value:
        setFaceAnimation(0, 0);
        break;
    case DEFAULT_BLINK_FACE_ANIMATION.value:
        setFaceAnimation(64, 0);
        break;
    case THINK_FACE_ANIMATION.value:
        setFaceAnimation(0, 32);
        break;
    case ANGRY_BLINK_FACE_ANIMATION.value:
        setFaceAnimation(64, 32);
        break;
    case ANGRY_FACE_ANIMATION.value:
        setFaceAnimation(0, 64);
        break;
    case SHOCKED_FACE_ANIMATION.value:
        setFaceAnimation(0, 96);
        break;
    }

    if (updateCurrent) {
        currentFaceAnimation = faceName;
    }
}
