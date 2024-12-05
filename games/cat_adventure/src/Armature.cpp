#include "Armature.h"

#include "RainbowColors.h"
#include "Renderer.h"

#include <common/syscalls/syscalls.h>

#include <EASTL/fixed_string.h>
#include <psyqo/xprintf.h>

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
    for (int i = joint.boneInfluencesOffset;
         i < joint.boneInfluencesOffset + joint.boneInfluencesSize;
         ++i) {
        const auto vid = boneInfluences[i];
        const auto& posStart = mesh.ogVertices[vid].pos;
        auto newPos = psyqo::Vec3{};
        newPos.x.value = posStart.x.value;
        newPos.y.value = posStart.y.value;
        newPos.z.value = posStart.z.value;
        newPos = joint.globalTransform.transformPoint(newPos);
        /* if (joint.id == 11) {
            static eastl::fixed_string<char, 512> str;
            fsprintf(
                str,
                "%d, (%.4f, %.4f, %.4f) -> (%.4f, %.4f, %.4f)",
                (int)boneInfluences[i],
                psyqo::FixedPoint<>(posStart.x),
                psyqo::FixedPoint<>(posStart.y),
                psyqo::FixedPoint<>(posStart.z),
                psyqo::FixedPoint<>(newPos.x),
                psyqo::FixedPoint<>(newPos.y),
                psyqo::FixedPoint<>(newPos.z));
            ramsyscall_printf("vec = %s\n", str.c_str());
        } */
        auto& posNew = mesh.vertices[vid].pos;
        posNew.x.value = newPos.x.value;
        posNew.y.value = newPos.y.value;
        posNew.z.value = newPos.z.value;
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
