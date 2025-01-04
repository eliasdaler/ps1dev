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
    psyqo::GTE::Math::multiplyMatrix33<
        psyqo::GTE::PseudoRegister::Rotation,
        psyqo::GTE::PseudoRegister::V0>(transform.rotation, rotX, &transform.rotation);
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
    case THINK_FACE_ANIMATION.value:
    case ANGRY_FACE_ANIMATION.value:
        return ANGRY_BLINK_FACE_ANIMATION;
    }

    return DEFAULT_BLINK_FACE_ANIMATION;
}

void AnimatedModelObject::update()
{
    calculateWorldMatrix();
    animator.update();

    animator.animate(model.armature, jointGlobalTransforms);

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
}

psyqo::Angle AnimatedModelObject::findInteractionAngle(const Object& other)
{
    const auto diff = other.getPosition() - getPosition();
    return math::atan2(diff.x, diff.z);
}

void AnimatedModelObject::setFaceAnimation(std::uint8_t faceU, std::uint8_t faceV)
{
    if (faceSubmeshIdx == 0xFF) {
        return;
    }

    auto& faceMesh = eastl::get<Mesh>(model.meshes[faceSubmeshIdx]);
    const auto offsetU = faceU - faceOffsetU;
    const auto offsetV = faceV - faceOffsetV;
    for (int i = 0; i < 2; ++i) {
        for (auto& gt3 : faceMesh.gt3[i]) {
            gt3.primitive.uvA.u += offsetU;
            gt3.primitive.uvA.v += offsetV;
            gt3.primitive.uvB.u += offsetU;
            gt3.primitive.uvB.v += offsetV;
            gt3.primitive.uvC.u += offsetU;
            gt3.primitive.uvC.v += offsetV;
        }
        for (auto& gt4 : faceMesh.gt4[i]) {
            gt4.primitive.uvA.u += offsetU;
            gt4.primitive.uvA.v += offsetV;
            gt4.primitive.uvB.u += offsetU;
            gt4.primitive.uvB.v += offsetV;
            gt4.primitive.uvC.u += offsetU;
            gt4.primitive.uvC.v += offsetV;
            gt4.primitive.uvD.u += offsetU;
            gt4.primitive.uvD.v += offsetV;
        }
    }

    faceOffsetU = faceU;
    faceOffsetV = faceV;
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
    }

    if (updateCurrent) {
        currentFaceAnimation = faceName;
    }
}
