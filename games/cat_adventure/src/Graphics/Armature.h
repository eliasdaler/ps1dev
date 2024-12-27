#pragma once

#include <EASTL/vector.h>

#include <Math/Transform.h>

class Renderer;
struct Mesh;
struct Object;
struct Camera;

struct Joint {
    Transform localTransform;

    using JointId = std::uint8_t;
    static constexpr JointId NULL_JOINT_ID{0xFF};
    JointId id{NULL_JOINT_ID};
    JointId firstChild{NULL_JOINT_ID};
    JointId nextSibling{NULL_JOINT_ID};
};

struct Armature {
    const Joint& getRootJoint() const { return joints[0]; }
    Joint& getRootJoint() { return joints[0]; }

    eastl::vector<Joint> joints;

    void calculateTransforms(eastl::vector<TransformMatrix>& jointGlobalTransforms) const;
    void calculateTransforms(
        eastl::vector<TransformMatrix>& jointGlobalTransforms,
        const Joint& joint,
        const Joint& parentJoint,
        bool isRoot = false) const;

    Joint::JointId selectedJoint{0};
};
