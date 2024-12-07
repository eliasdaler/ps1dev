#include "Armature.h"

#include "RainbowColors.h"
#include "Renderer.h"

#include <common/syscalls/syscalls.h>

#include <EASTL/fixed_string.h>
#include <psyqo/xprintf.h>

#define USE_GTE_MATH

using namespace psyqo::GTE;
using namespace psyqo::GTE::Kernels;

void Armature::calculateTransforms()
{
    calculateTransforms(getRootJoint(), getRootJoint(), true);
}

void Armature::applySkinning(Mesh& mesh)
{
    applySkinning(mesh, getRootJoint());
}

void Armature::applySkinning(Mesh& mesh, const Joint& joint)
{
    const auto& boneInfluences = this->boneInfluences;
#ifdef USE_GTE_MATH
    psyqo::GTE::writeUnsafe<JOINT_ROTATION_MATRIX_REGISTER>(joint.globalTransform.rotation);
#endif
    for (int i = joint.boneInfluencesOffset;
         i < joint.boneInfluencesOffset + joint.boneInfluencesSize;
         ++i) {
        const auto vid = boneInfluences[i];
        psyqo::Vec3 newPos = mesh.ogVertices[vid].pos;
        // TODO: transformPoint for psyqo::GTE::PackedVec3?
        newPos = joint.globalTransform.transformPoint(newPos);
        mesh.vertices[vid].pos = psyqo::GTE::PackedVec3{newPos};
    }

    auto currentJointId = joint.firstChild;
    while (currentJointId != Joint::NULL_JOINT_ID) {
        auto& child = joints[currentJointId];
        applySkinning(mesh, child);
        currentJointId = child.nextSibling;
    }
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

void Armature::dehighlightMeshInfluences(Mesh& mesh, Joint::JointId id) const
{
    const auto& joint = joints[id];
    const auto& boneInfluences = this->boneInfluences;
    for (int i = joint.boneInfluencesOffset;
         i < joint.boneInfluencesOffset + joint.boneInfluencesSize;
         ++i) {
        mesh.vertices[boneInfluences[i]].col = psyqo::Color{.r = 128, .g = 128, .b = 128};
    }
}

void Armature::highlightMeshInfluences(Mesh& mesh, Joint::JointId id) const
{
    const auto& joint = joints[id];
    const auto& boneInfluences = this->boneInfluences;
    for (int i = joint.boneInfluencesOffset;
         i < joint.boneInfluencesOffset + joint.boneInfluencesSize;
         ++i) {
        mesh.vertices[boneInfluences[i]].col = psyqo::Color{.r = 128, .g = 0, .b = 0};
    }
}
