#include "Armature.h"

#include "RainbowColors.h"

#include <common/syscalls/syscalls.h>

#include <EASTL/fixed_string.h>
#include <psyqo/gte-kernels.hh>
#include <psyqo/xprintf.h>

#define USE_GTE_MATH

#define TEST

using namespace psyqo::GTE;
using namespace psyqo::GTE::Kernels;

void Armature::calculateTransforms(eastl::vector<TransformMatrix>& jointGlobalTransforms) const
{
    const auto& rootJoint = getRootJoint();
    calculateTransforms(jointGlobalTransforms, rootJoint, rootJoint, true);
}

void Armature::calculateTransforms(
    eastl::vector<TransformMatrix>& jointGlobalTransforms,
    const Joint& joint,
    const Joint& parentJoint,
    bool isRoot) const
{
    auto& jointGlobalTransform = jointGlobalTransforms[joint.id];
    if (isRoot) {
        jointGlobalTransform.rotation = joint.localTransform.rotation.toRotationMatrix();
        jointGlobalTransform.translation = joint.localTransform.translation;
    } else {
        jointGlobalTransform =
            combineTransforms(jointGlobalTransforms[parentJoint.id], joint.localTransform);
    }

    auto currentJointId = joint.firstChild;
    while (currentJointId != Joint::NULL_JOINT_ID) {
        auto& child = joints[currentJointId];
        calculateTransforms(jointGlobalTransforms, child, joint);
        currentJointId = child.nextSibling;
    }
}
