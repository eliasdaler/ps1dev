#pragma once

#include <EASTL/vector.h>

#include "Transform.h"

class Renderer;

struct Joint {
    Transform localTransform;
    TransformMatrix globalTransform;

    using JointId = std::uint16_t;
    static constexpr JointId NULL_JOINT_ID{0xFFFF};
    JointId id{NULL_JOINT_ID};
    JointId parent{NULL_JOINT_ID};
    JointId firstChild{NULL_JOINT_ID};
    JointId nextSibling{NULL_JOINT_ID};
};

struct Skeleton {
    void makeTestSkeleton();

    Joint& getRootJoint() { return joints[0]; }

    eastl::vector<Joint> joints;

    void calculateTransforms();
    void calculateTransforms(Joint& joint, const Joint& parentJoint, bool isRoot = false);

    void drawDebug(Renderer& renderer);
    void drawDebug(Renderer& renderer, const Joint& joint, Joint::JointId childId);
};
