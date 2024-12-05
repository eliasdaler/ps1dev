#pragma once

#include <filesystem>
#include <string>
#include <vector>

#include <glm/gtc/quaternion.hpp>
#include <glm/vec3.hpp>

#include "ImageLoader.h"

struct Transform {
    glm::vec3 position{};
    glm::quat rotation{glm::identity<glm::quat>()};
    glm::vec3 scale{1.f};

    glm::mat4 asMatrix() const;
};

struct Object {
    std::string name;
    Transform transform;
    std::size_t mesh;
};

struct Vertex {
    glm::vec3 position;
    glm::vec3 color;
};

struct Face {
    std::vector<std::size_t> vertices;
    std::vector<glm::vec2> uvs;
};

struct Mesh {
    std::string name;
    std::vector<Vertex> vertices;
    std::vector<Face> faces;
    std::vector<std::size_t> materials;
};

struct Material {
    std::string name;
    std::string texture;
    ImageData imageData;
};

struct Joint {
    std::string name;
    std::uint32_t index{0};
    // transform
    glm::vec3 translation{};
    glm::quat rotation{glm::identity<glm::quat>()};
    std::vector<std::uint8_t> children;
};

struct Armature {
    std::vector<Joint> joints;
    std::vector<std::vector<std::uint16_t>> boneInfluences;
};

struct ModelJson {
    std::vector<Object> objects;
    std::vector<Mesh> meshes;
    std::vector<Material> materials;
    Armature armature;
};

ModelJson parseJsonFile(
    const std::filesystem::path& path,
    const std::filesystem::path& assetDirPath);
