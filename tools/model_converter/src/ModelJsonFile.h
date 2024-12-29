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
    int mesh{-1};
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
    int jointId{-1};
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
    std::vector<glm::mat4x4> inverseBindMatrices;
};

struct AnimationKey {
    int frame;
    union
    {
        glm::vec3 translation;
        glm::quat rotation;
    } data;
};

struct AnimationTrack {
    std::uint8_t trackType;
    std::uint32_t jointId;
    std::vector<AnimationKey> keys;
};

struct Animation {
    std::string name;
    std::uint16_t length; // in frames
    std::vector<AnimationTrack> tracks;
};

struct AABB {
    glm::vec3 min;
    glm::vec3 max;
};

struct ModelJson {
    std::vector<Object> objects;
    std::vector<Mesh> meshes;
    std::vector<Material> materials;
    Armature armature;
    std::vector<Animation> animations;

    // level only
    std::vector<AABB> collision;
};

ModelJson parseJsonFile(
    const std::filesystem::path& path,
    const std::filesystem::path& assetDirPath);
