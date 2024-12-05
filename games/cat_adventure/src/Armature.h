#pragma once

#include <EASTL/vector.h>

#include "Transform.h"

class Renderer;
struct Mesh;

struct Joint {
    Transform localTransform;
    TransformMatrix globalTransform;

    using JointId = std::uint8_t;
    static constexpr JointId NULL_JOINT_ID{0xFF};
    JointId id{NULL_JOINT_ID};
    JointId firstChild{NULL_JOINT_ID};
    JointId nextSibling{NULL_JOINT_ID};
    std::uint16_t boneInfluencesOffset;
    std::uint16_t boneInfluencesSize;
};

struct Armature {
    Joint& getRootJoint() { return joints[0]; }

    eastl::vector<Joint> joints;
    eastl::vector<std::uint16_t> boneInfluences;

    void calculateTransforms();
    void calculateTransforms(Joint& joint, const Joint& parentJoint, bool isRoot = false);

    void drawDebug(Renderer& renderer);
    void drawDebug(Renderer& renderer, const Joint& joint, Joint::JointId childId);

    void dehighlightMeshInfluences(Mesh& mesh, Joint::JointId id) const;
    void highlightMeshInfluences(Mesh& mesh, Joint::JointId id) const;

    Joint::JointId selectedJoint{0};
};
