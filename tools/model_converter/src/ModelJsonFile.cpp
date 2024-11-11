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

        object.mesh = objectObj.at("mesh");

        model.objects.push_back(std::move(object));
    }

    const auto& meshesObj = root.at("meshes");
    model.meshes.reserve(meshesObj.size());
    for (const auto& meshObj : meshesObj) {
        Mesh mesh{};

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

            mesh.faces.push_back(std::move(face));
        }

        // read material indices
        const auto& materialsObj = meshObj.at("materials");
        mesh.materials.reserve(materialsObj.size());
        for (const auto& material : materialsObj) {
            mesh.materials.push_back(material);
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
            material.imageData = util::loadImage(material.texture);
            if (material.imageData.pixels.empty()) {
                std::cout << "Failed to open " << material.texture << std::endl;
            }
        }
        model.materials.push_back(std::move(material));
    }

    return model;
}
