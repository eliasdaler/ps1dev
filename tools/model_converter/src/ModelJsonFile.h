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

struct ModelJson {
    std::vector<Object> objects;
    std::vector<Mesh> meshes;
    std::vector<Material> materials;
};

ModelJson parseJsonFile(
    const std::filesystem::path& path,
    const std::filesystem::path& assetDirPath);
