#include "Armature.h"

#include "RainbowColors.h"
#include "Renderer.h"

#include <common/syscalls/syscalls.h>

#include <EASTL/fixed_string.h>
#include <psyqo/xprintf.h>

#define USE_GTE_MATH

#define TEST

using namespace psyqo::GTE;
using namespace psyqo::GTE::Kernels;

void Armature::calculateTransforms()
{
    calculateTransforms(getRootJoint(), getRootJoint(), true);
}

void Armature::calculateTransforms(Joint& joint, const Joint& parentJoint, bool isRoot)
{
    if (isRoot) {
        joint.globalTransform.rotation = joint.localTransform.rotation.toRotationMatrix();
        joint.globalTransform.translation = joint.localTransform.translation;
    } else {
        joint.globalTransform =
            combineTransforms(parentJoint.globalTransform, joint.localTransform);
    }

    auto currentJointId = joint.firstChild;
    while (currentJointId != Joint::NULL_JOINT_ID) {
        auto& child = joints[currentJointId];
        calculateTransforms(child, joint);
        currentJointId = child.nextSibling;
    }
}

void Armature::drawDebug(Renderer& renderer)
{
    const auto& rootJoint = getRootJoint();
    drawDebug(renderer, rootJoint, rootJoint.firstChild);
}

void Armature::drawDebug(Renderer& renderer, const Joint& joint, Joint::JointId childId)
{
#ifdef USE_GTE_MATH
    psyqo::GTE::writeUnsafe<JOINT_ROTATION_MATRIX_REGISTER>(joint.globalTransform.rotation);
#endif
    static const auto boneStartLocal = psyqo::Vec3{};
    static const auto boneEndNull = psyqo::Vec3{0.0, 0.1f / 8.f, 0.0};
    const auto boneStart = joint.globalTransform.transformPoint(boneStartLocal);

    const auto boneEndLocal = (childId == Joint::NULL_JOINT_ID) ?
                                  boneEndNull :
                                  joints[childId].localTransform.translation;
    const auto boneEnd = joint.globalTransform.transformPoint(boneEndLocal);

    const auto jointColor = (joint.id == selectedJoint) ?
                                psyqo::Color{.r = 255, .g = 255, .b = 255} :
                                psyqo::Color{.r = 255, .g = 0, .b = 255};
    renderer.drawLineLocalSpace(boneStart, boneEnd, jointColor);

    auto currentJointId = joint.firstChild;
    while (currentJointId != Joint::NULL_JOINT_ID) {
        auto& child = joints[currentJointId];
        drawDebug(renderer, child, child.firstChild);
        currentJointId = child.nextSibling;
    }
}
