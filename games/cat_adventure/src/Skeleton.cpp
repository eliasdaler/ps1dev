#include "Skeleton.h"

#include "RainbowColors.h"
#include "Renderer.h"

void Skeleton::makeTestSkeleton()
{
    joints.resize(4);
    for (std::uint16_t i = 0; i < joints.size(); ++i) {
        joints[i].id = i;
    }

    auto& joint = joints[0];
    auto& joint1 = joints[1];
    auto& joint2 = joints[2];
    auto& joint3 = joints[3];

    joint.firstChild = 1;
    joint.parent = 0;
    joint1.firstChild = 2;
    joint1.parent = 1;
    joint2.parent = 2;
    joint2.nextSibling = 3;
    joint3.parent = 2;

    joint.localTransform.rotation = {
        -3.2025173624106174e-08,
        -4.40346141772352e-08,
        -0.3660619556903839,
        0.9305905103683472,

    };
    joint.localTransform.translation = {};

    joint1.localTransform.rotation = {
        -0.21838730573654175,
        0.06075707823038101,
        0.4552289545536041,
        0.8610355854034424,
    };
    joint1.localTransform.translation = {
        -3.1086244689504383e-15,
        1,
        7.105427357601002e-15,
    };

    joint2.localTransform.rotation = {
        -0.25159820914268494,
        0.3440193235874176,
        0.06417443603277206,
        0.9023473262786865,
    };
    joint2.localTransform.translation = {
        0,
        0.623515248298645,
        -3.725290298461914e-08,
    };

    joint3.localTransform.rotation = {
        0.25015076994895935,
        -2.1718289389127676e-08,
        -0.4493105113506317,
        0.8576390147209167,
    };

    joint3.localTransform.translation = {
        0,
        0.623515248298645,
        -3.725290298461914e-08,
    };
}

void Skeleton::calculateTransforms()
{
    calculateTransforms(getRootJoint(), getRootJoint(), true);
}

void Skeleton::calculateTransforms(Joint& joint, const Joint& parentJoint, bool isRoot)
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

void Skeleton::drawDebug(Renderer& renderer)
{
    const auto& rootJoint = getRootJoint();
    drawDebug(renderer, rootJoint, rootJoint.firstChild);
}

void Skeleton::drawDebug(Renderer& renderer, const Joint& joint, Joint::JointId childId)
{
    static const auto boneStartLocal = psyqo::Vec3{};
    static const auto boneEndNull = psyqo::Vec3{0.0, 0.5, 0.0};
    const auto boneStart = joint.globalTransform.transformPoint(boneStartLocal);

    const auto boneEndLocal = (childId == Joint::NULL_JOINT_ID) ?
                                  boneEndNull :
                                  joints[childId].localTransform.translation;
    const auto boneEnd = joint.globalTransform.transformPoint(boneEndLocal);

    // note that we need to divide by 8 because we start from unscaled coords
    renderer.drawLineLocalSpace(boneStart / 8.f, boneEnd / 8.f, getRainbowColor(joint.id));

    auto currentJointId = joint.firstChild;
    while (currentJointId != Joint::NULL_JOINT_ID) {
        auto& child = joints[currentJointId];
        drawDebug(renderer, child, child.firstChild);
        currentJointId = child.nextSibling;
    }
}
