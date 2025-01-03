#include "ModelJsonFile.h"

#include <cassert>
#include <fstream>
#include <nlohmann/json.hpp>

glm::vec2 getVec2(const nlohmann::json& j, std::string_view key, const glm::vec2& defaultValue)
{
    auto it = j.find(key);
    if (it == j.end()) {
        return defaultValue;
    }
    const auto& arrObj = *it;
    assert(arrObj.is_array() && arrObj.size() == 2);
    return {arrObj[0], arrObj[1]};
}

glm::vec3 getVec3(const nlohmann::json& j, std::string_view key, const glm::vec3& defaultValue)
{
    auto it = j.find(key);
    if (it == j.end()) {
        return defaultValue;
    }
    const auto& arrObj = *it;
    assert(arrObj.is_array() && arrObj.size() == 3);
    return {arrObj[0], arrObj[1], arrObj[2]};
}

glm::quat getQuat(const nlohmann::json& j, std::string_view key, const glm::quat& defaultValue)
{
    auto it = j.find(key);
    if (it == j.end()) {
        return defaultValue;
    }
    const auto& arrObj = *it;
    assert(arrObj.is_array() && arrObj.size() == 4);
    return glm::quat{arrObj[0], arrObj[1], arrObj[2], arrObj[3]};
}

glm::mat4 Transform::asMatrix() const
{
    auto transformMatrix = glm::translate(glm::mat4{1.f}, position);
    if (rotation != glm::identity<glm::quat>()) {
        transformMatrix *= glm::mat4_cast(rotation);
    }
    transformMatrix = glm::scale(transformMatrix, scale);
    return transformMatrix;
}

ModelJson parseJsonFile(
    const std::filesystem::path& path,
    const std::filesystem::path& assetDirPath)
{
    nlohmann::json root;
    std::ifstream file(path);
    assert(file.good());
    file >> root;

    ModelJson model{};

    const auto& objectsObj = root.at("objects");
    model.objects.reserve(objectsObj.size());
    for (const auto& objectObj : objectsObj) {
        Object object{
            .name = objectObj.at("name"),
        };

        object.transform.position = getVec3(objectObj, "position", glm::vec3{});
        object.transform.rotation = getQuat(objectObj, "rotation", glm::identity<glm::quat>());
        object.transform.scale = getVec3(objectObj, "scale", glm::vec3{1.f});

        if (objectObj.contains("mesh")) {
            object.mesh = objectObj.at("mesh");
        }

        model.objects.push_back(std::move(object));
    }

    const auto& meshesObj = root.at("meshes");
    model.meshes.reserve(meshesObj.size());
    for (const auto& meshObj : meshesObj) {
        Mesh mesh{};

        if (meshObj.contains("joint_id")) {
            mesh.jointId = meshObj.at("joint_id");
        }

        // read vertices
        const auto& verticesObj = meshObj.at("vertices");
        mesh.vertices.reserve(verticesObj.size());
        for (const auto& vertexObj : verticesObj) {
            Vertex vertex;
            vertex.position = getVec3(vertexObj, "pos", {});
            vertex.color = getVec3(vertexObj, "color", {255.f, 255.f, 255.f});
            mesh.vertices.push_back(std::move(vertex));
        }

        // read faces
        const auto& facesObj = meshObj.at("faces");
        mesh.faces.reserve(facesObj.size());
        for (const auto& faceObj : facesObj) {
            Face face;

            // vertices
            const auto& verticesObj = faceObj.at("vertices");
            face.vertices.reserve(verticesObj.size());
            for (const auto& vIdx : verticesObj) {
                face.vertices.push_back(vIdx);
            }

            // uvs
            const auto& uvsObj = faceObj.at("uvs");
            face.uvs.reserve(uvsObj.size());
            for (const auto& uvObj : uvsObj) {
                assert(uvObj.is_array() && uvObj.size() == 2);
                face.uvs.push_back(glm::vec2{uvObj[0], uvObj[1]});
            }

            const auto it = faceObj.find("material");
            if (it != faceObj.end()) {
                face.material = *it;
            } else {
                face.material = -1;
            }

            mesh.faces.push_back(std::move(face));
        }

        model.meshes.push_back(std::move(mesh));
    }

    const auto& materialsObj = root.at("materials");
    model.materials.reserve(materialsObj.size());
    for (const auto& materialObj : materialsObj) {
        Material material{
            .name = materialObj.at("name"),
        };
        if (auto it = materialObj.find("texture"); it != materialObj.end()) {
            material.texture = assetDirPath / *it;
        }
        model.materials.push_back(std::move(material));
    }

    const auto armatureIt = root.find("armature");
    if (armatureIt != root.end()) {
        auto& armature = model.armature;

        const auto& armatureObj = *armatureIt;
        const auto& jointsArr = armatureObj.at("joints");
        const auto numJoints = jointsArr.size();

        armature.joints.resize(numJoints);

        for (std::uint32_t idx = 0; idx < numJoints; ++idx) {
            // load joint
            const auto& jointObj = jointsArr[idx];
            auto& joint = armature.joints[idx];
            joint.name = jointObj.at("name");
            joint.translation = getVec3(jointObj, "translation", glm::vec3{});
            joint.rotation = getQuat(jointObj, "rotation", glm::identity<glm::quat>());

            if (jointObj.contains("children")) {
                joint.children = jointObj.at("children").get<std::vector<std::uint8_t>>();
            }
        }

        armature.inverseBindMatrices.resize(numJoints);
        const auto& ibsObj = armatureObj.at("inverseBindMatrices");
        for (std::uint32_t idx = 0; idx < numJoints; ++idx) {
            auto& ib = armature.inverseBindMatrices[idx];
            const auto& ibObj = ibsObj[idx];
            for (int i = 0; i < 4; ++i) {
                for (int j = 0; j < 4; ++j) {
                    // glm's matrices are column major
                    ib[j][i] = ibObj[i][j];
                }
            }
        }
    }

    const auto animationsIt = root.find("animations");
    if (animationsIt != root.end()) {
        const auto& animationsArr = *animationsIt;
        model.animations.reserve(animationsArr.size());
        for (const auto& anim : animationsArr) {
            Animation animation;
            animation.name = anim.at("name");
            animation.looped = anim.at("looped");
            animation.length = anim.at("length");
            const auto& tracksArr = anim.at("tracks");
            animation.tracks.reserve(tracksArr.size());
            for (const auto& trackJson : tracksArr) {
                AnimationTrack track;
                track.trackType = trackJson.at("track_type");
                track.jointId = trackJson.at("joint_id");
                const auto& keysArr = trackJson.at("keys");
                track.keys.reserve(keysArr.size());
                for (const auto& keyJson : keysArr) {
                    AnimationKey key{};
                    key.frame = keyJson[0];
                    const auto& keyData = keyJson[1];
                    if (track.trackType == 0) {
                        assert(keyData.size() == 4);
                        key.data.rotation =
                            glm::quat(keyData[0], keyData[1], keyData[2], keyData[3]);
                    } else if (track.trackType == 1) {
                        assert(keyData.size() == 3);
                        key.data.translation = glm::vec3(keyData[0], keyData[1], keyData[2]);
                    } else {
                        assert(false && "unknown track type");
                    }
                    track.keys.push_back(std::move(key));
                }
                animation.tracks.push_back(std::move(track));
            }
            model.animations.push_back(std::move(animation));
        }
    }

    const auto collisionIt = root.find("collision");
    if (collisionIt != root.end()) {
        const auto& collisionArr = *collisionIt;
        model.collision.reserve(collisionArr.size());
        for (const auto& collObj : collisionArr) {
            AABB aabb;
            const auto& aabbObj = collObj.at("aabb");
            aabb.min = getVec3(aabbObj, "min", {});
            aabb.max = getVec3(aabbObj, "max", {});
            model.collision.push_back(std::move(aabb));
        }
    }

    return model;
}
