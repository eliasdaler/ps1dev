#include "Json2PsxConverter.h"

#include "ModelJsonFile.h"
#include "PsxModel.h"
#include <FixedPoint.h>

#include <format>
#include <iostream>
#include <stack>

namespace
{

std::uint8_t uvToInt8(float v)
{
    return (std::int8_t)std::clamp(v * 256.f, 0.f, 255.f);
}

// "Fix" rectangular UVs to not bleed into other squares
// E.g. (64, 64, 128, 128) UV will be changed to (64, 64, 127, 127)
void offsetRectUV(std::array<PsxVert, 4>& quad)
{
    std::uint8_t maxU = 0;
    std::uint8_t maxV = 0;
    std::uint8_t minU = 255;
    std::uint8_t minV = 255;

    for (int i = 0; i < 4; ++i) {
        const auto& v = quad[i];
        minU = std::min(minU, v.uv.x);
        minV = std::min(minV, v.uv.y);
        maxU = std::max(maxU, v.uv.x);
        maxV = std::max(maxV, v.uv.y);
    }

    if (minU == maxU && minV == maxV) {
        // "point" UV
        return;
    }

    // check if UV is rectangular
    for (int i = 0; i < 4; ++i) {
        const auto& v = quad[i];
        if ((v.uv.x != minU && v.uv.x != maxU) || (v.uv.y != minV && v.uv.y != maxV)) {
            return;
        }
    }

    for (int i = 0; i < 4; ++i) {
        auto& v = quad[i];
        if (v.uv.x == maxU) {
            v.uv.x = maxU - 1;
        }
        if (v.uv.y == maxV) {
            v.uv.y = maxV - 1;
        }
    }
}

}

PsxModel jsonToPsxModel(const ModelJson& modelJson, const ConversionParams& params)
{
    PsxModel psxModel;

    for (const auto& object : modelJson.objects) {
        if (object.mesh == -1) {
            continue;
        }

        PsxSubmesh psxMesh;
        const auto& mesh = modelJson.meshes[object.mesh];
        const auto tm = object.transform.asMatrix();

        if (object.name.ends_with(".SD")) {
            psxMesh.subdivide = true;
        }

        // TODO: support multiple materials?
        const auto& material = modelJson.materials[mesh.materials[0]];
        bool hasTexture = !material.imageData.pixels.empty();

        // std::cout << "tex:" << material.texture << " " << texWidth << " " << texHeight <<
        // std::endl;

        static const auto zeroUV = glm::vec2{};
        for (const auto& face : mesh.faces) {
            std::array<PsxVert, 4> psxFace;
            if (face.vertices.size() != 3 && face.vertices.size() != 4) {
                throw std::runtime_error(std::format(
                    "bad face in submesh {}: has {} vertices", object.name, face.vertices.size()));
            }
            assert(face.vertices.size() <= 4);
            for (std::size_t i = 0; i < face.vertices.size(); ++i) {
                const auto& v = mesh.vertices[face.vertices[i]];
                // calculate world pos
                const auto pos = glm::vec3{tm * glm::vec4{v.position, 1.f}};

                psxFace[i].originalIndex = face.vertices[i];
                psxFace[i].pos = {
                    .x = floatToFixed<FixedPoint4_12>(pos.x, params.scale), // X' = X
                    .y = floatToFixed<FixedPoint4_12>(pos.z, params.scale), // Y' = Z
                    .z = floatToFixed<FixedPoint4_12>(-pos.y, params.scale), // Z' = -Y
                };

                /* printf(
                    "(%.4f, %.4f, %.4f) -> (%d, %d, %d)\n",
                    pos.x,
                    pos.y,
                    pos.z,
                    psxFace[i].pos.x,
                    psxFace[i].pos.y,
                    psxFace[i].pos.z); */

                float offset = 0;

                if (hasTexture) {
                    const auto texWidth = material.imageData.width;
                    const auto texHeight = material.imageData.height;
                    const auto& uv = face.uvs.empty() ? zeroUV : face.uvs[i];
                    psxFace[i].uv = {
                        .x = (std::uint8_t)std::clamp(uv.x * (texWidth - offset), 0.f, 255.f),
                        // Y coord is flipped in UV
                        .y =
                            (std::uint8_t)std::clamp((1 - uv.y) * (texHeight - offset), 0.f, 255.f),
                    };
                } else {
                    // no need to store uvs actually
                    psxFace[i].uv = {};
                }

                if (hasTexture) {
                    psxFace[i].color =
                        {(std::uint8_t)(v.color.x / 2.f),
                         (std::uint8_t)(v.color.y / 2.f),
                         (std::uint8_t)(v.color.z / 2.f)};
                } else {
                    psxFace[i].color =
                        {(std::uint8_t)(v.color.x),
                         (std::uint8_t)(v.color.y),
                         (std::uint8_t)(v.color.z)};
                }
            }

            if (face.vertices.size() == 3) {
                auto face = PsxTriFace{psxFace[0], psxFace[2], psxFace[1]};
                if (hasTexture) {
                    psxMesh.triFaces.push_back(std::move(face));
                } else {
                    psxMesh.untexturedTriFaces.push_back(std::move(face));
                }
            } else {
                assert(face.vertices.size() == 4);
                // note the order - that's how PS1 quads work
                auto face = PsxQuadFace{psxFace[3], psxFace[2], psxFace[0], psxFace[1]};
                if (hasTexture) {
                    offsetRectUV(face);
                    psxMesh.quadFaces.push_back(std::move(face));
                } else {
                    psxMesh.untexturedQuadFaces.push_back(std::move(face));
                }
            }

            /* for (int i = 0; i < face.vertices.size(); ++i) {
                auto& v = face.vertices.size() == 3 ? psxMesh.triFaces.back()[i] :
                                                      psxMesh.quadFaces.back()[i];
                std::cout << v.pos.x << " " << v.pos.y << " " << v.pos.z << " | ";
                std::cout << (int)v.uv.x << " " << (int)v.uv.y << " | ";
                std::cout << (int)v.color.x << " " << (int)v.color.y << " " << (int)v.color.z
                          << std::endl;
            }
            std::cout << "____\n"; */
        }
        psxModel.submeshes.push_back(std::move(psxMesh));
    }

    if (!modelJson.armature.joints.empty()) {
        auto& armature = psxModel.armature;
        const auto& armatureJson = modelJson.armature;
        const auto numJoints = armatureJson.joints.size();
        armature.joints.reserve(numJoints);
        for (const auto& joint : armatureJson.joints) {
            PsxJoint psxJoint;
            psxJoint.translation = {
                .x = floatToFixed<FixedPoint4_12>(joint.translation.x, params.scale),
                .y = floatToFixed<FixedPoint4_12>(joint.translation.y, params.scale),
                .z = floatToFixed<FixedPoint4_12>(joint.translation.z, params.scale),
            };

            psxJoint.rotation = {
                .x = floatToFixed<FixedPoint4_12>(joint.rotation.w),
                .y = floatToFixed<FixedPoint4_12>(joint.rotation.x),
                .z = floatToFixed<FixedPoint4_12>(joint.rotation.y),
                .w = floatToFixed<FixedPoint4_12>(joint.rotation.z),
            };
            armature.joints.push_back(std::move(psxJoint));
        }

        std::stack<std::uint8_t> jointsToProcess;
        jointsToProcess.push(0);
        while (!jointsToProcess.empty()) {
            const auto id = jointsToProcess.top();
            jointsToProcess.pop();

            auto& joint = armatureJson.joints[id];
            if (joint.children.empty()) {
                continue;
            }

            auto& psxJoint = armature.joints[id];
            psxJoint.firstChild = joint.children[0];
            for (int i = 0; i < joint.children.size(); ++i) {
                const auto childId = joint.children[i];
                auto& child = armature.joints[childId];
                child.nextSibling = (i == joint.children.size() - 1) ? PsxJoint::NULL_JOINT_ID :
                                                                       joint.children[i + 1];
                jointsToProcess.push(childId);
            }
        }

        const auto& object = modelJson.objects[0];

        for (const auto& mesh : modelJson.meshes) {
            const auto tm = object.transform.asMatrix();
            PsxSubmesh psxMesh;

            if (object.name.ends_with(".SD")) {
                psxMesh.subdivide = true;
            }

            assert(mesh.jointId != -1);
            psxMesh.jointId = mesh.jointId;

            // TODO: support multiple materials?
            const auto& material = modelJson.materials[mesh.materials[0]];
            bool hasTexture = !material.imageData.pixels.empty();

            // std::cout << "tex:" << material.texture << " " << texWidth << " " << texHeight <<
            // std::endl;

            static const auto zeroUV = glm::vec2{};
            for (const auto& face : mesh.faces) {
                std::array<PsxVert, 4> psxFace;
                if (face.vertices.size() != 3 && face.vertices.size() != 4) {
                    throw std::runtime_error(std::format(
                        "bad face in submesh {}: has {} vertices",
                        object.name,
                        face.vertices.size()));
                }
                assert(face.vertices.size() <= 4);
                for (std::size_t i = 0; i < face.vertices.size(); ++i) {
                    const auto& v = mesh.vertices[face.vertices[i]];
                    // calculate world pos
                    const auto pos = glm::vec3{tm * glm::vec4{v.position, 1.f}};

                    psxFace[i].originalIndex = face.vertices[i];
                    psxFace[i].pos = {
                        .x = floatToFixed<FixedPoint4_12>(pos.x, params.scale), // X' = X
                        .y = floatToFixed<FixedPoint4_12>(pos.z, params.scale), // Y' = Z
                        .z = floatToFixed<FixedPoint4_12>(-pos.y, params.scale), // Z' = -Y
                    };

                    /* printf(
                        "(%.4f, %.4f, %.4f) -> (%d, %d, %d)\n",
                        pos.x,
                        pos.y,
                        pos.z,
                        psxFace[i].pos.x,
                        psxFace[i].pos.y,
                        psxFace[i].pos.z); */

                    float offset = 0;

                    if (hasTexture) {
                        const auto texWidth = material.imageData.width;
                        const auto texHeight = material.imageData.height;
                        const auto& uv = face.uvs.empty() ? zeroUV : face.uvs[i];
                        psxFace[i].uv = {
                            .x = (std::uint8_t)std::clamp(uv.x * (texWidth - offset), 0.f, 255.f),
                            // Y coord is flipped in UV
                            .y = (std::uint8_t)
                                std::clamp((1 - uv.y) * (texHeight - offset), 0.f, 255.f),
                        };
                    } else {
                        // no need to store uvs actually
                        psxFace[i].uv = {};
                    }

                    if (hasTexture) {
                        psxFace[i].color =
                            {(std::uint8_t)(v.color.x / 2.f),
                             (std::uint8_t)(v.color.y / 2.f),
                             (std::uint8_t)(v.color.z / 2.f)};
                    } else {
                        psxFace[i].color =
                            {(std::uint8_t)(v.color.x),
                             (std::uint8_t)(v.color.y),
                             (std::uint8_t)(v.color.z)};
                    }
                }

                if (face.vertices.size() == 3) {
                    auto face = PsxTriFace{psxFace[0], psxFace[2], psxFace[1]};
                    if (hasTexture) {
                        psxMesh.triFaces.push_back(std::move(face));
                    } else {
                        psxMesh.untexturedTriFaces.push_back(std::move(face));
                    }
                } else {
                    assert(face.vertices.size() == 4);
                    // note the order - that's how PS1 quads work
                    auto face = PsxQuadFace{psxFace[3], psxFace[2], psxFace[0], psxFace[1]};
                    if (hasTexture) {
                        offsetRectUV(face);
                        psxMesh.quadFaces.push_back(std::move(face));
                    } else {
                        psxMesh.untexturedQuadFaces.push_back(std::move(face));
                    }
                }

                /* for (int i = 0; i < face.vertices.size(); ++i) {
                    auto& v = face.vertices.size() == 3 ? psxMesh.triFaces.back()[i] :
                                                          psxMesh.quadFaces.back()[i];
                    std::cout << v.pos.x << " " << v.pos.y << " " << v.pos.z << " | ";
                    std::cout << (int)v.uv.x << " " << (int)v.uv.y << " | ";
                    std::cout << (int)v.color.x << " " << (int)v.color.y << " " << (int)v.color.z
                              << std::endl;
                }
                std::cout << "____\n"; */
            }
            psxModel.submeshes.push_back(std::move(psxMesh));
        }

        /* armature.inverseBindMatrices.resize(numJoints);
        for (int jointId = 0; jointId < numJoints; ++jointId) {
            auto& ibPSX = armature.inverseBindMatrices[jointId];
            const auto& ib = armature.inverseBindMatrices[jointId];
            // TODO?
        } */

        // This is where things get complicated
        // Vertices in the binary files are stored per-face, so
        // the original vertex can end up in mesh.triFaces[35][2] or something
        // We need to iterate through all the vertices and find the correspondence
        // between old vertex index (from Blender) and new vertex index (in PSX mesh)
        std::vector<std::vector<std::uint16_t>> newInfluences;
        newInfluences.resize(numJoints);

        auto& mesh = psxModel.submeshes[0]; // FIXME: handle multiple meshes
        auto& meshJson = modelJson.meshes[0];
        const auto tm = modelJson.objects[0].transform.asMatrix();
        for (auto& mesh : psxModel.submeshes) {
            const auto jointId = mesh.jointId;
            const auto& boneInfluences = armatureJson.boneInfluences[jointId];
            auto& ib = armatureJson.inverseBindMatrices[jointId];
            for (const auto vid : boneInfluences) {
                int newVertexId{0};
                auto findInfluencedVertex = [&]<typename T>(T& facesVector) {
                    int faceIndex = 0;
                    for (auto& face : facesVector) {
                        for (int j = 0; j < face.size(); ++j) {
                            if (face[j].originalIndex == vid) {
                                newInfluences[jointId].push_back(newVertexId);

                                const auto& v = meshJson.vertices[vid];
                                // from Blender to glTF
                                const auto position =
                                    glm::vec3{v.position.x, v.position.z, -v.position.y};
                                // Vertices are stored in joint space
                                const auto pos = glm::vec3{ib * glm::vec4{position, 1.f}};
                                face[j].pos = {
                                    .x = floatToFixed<FixedPoint4_12>(pos.x, params.scale),
                                    .y = floatToFixed<FixedPoint4_12>(pos.y, params.scale),
                                    .z = floatToFixed<FixedPoint4_12>(pos.z, params.scale),
                                };
                            }
                            ++newVertexId;
                        }
                        ++faceIndex;
                    }
                };
                findInfluencedVertex(mesh.untexturedTriFaces);
                findInfluencedVertex(mesh.untexturedQuadFaces);
                findInfluencedVertex(mesh.triFaces);
                findInfluencedVertex(mesh.quadFaces);
            }
        }

        // store "spans" into boneInfluences array (offset + size)
        auto boneInfluencesOffset = 0;
        for (int i = 0; i < armatureJson.joints.size(); ++i) {
            auto& psxJoint = armature.joints[i];
            auto& boneInfluences = newInfluences[i];
            psxJoint.boneInfluencesOffset = boneInfluencesOffset;
            psxJoint.boneInfluencesSize = boneInfluences.size();
            // append joint's influences to the end
            armature.boneInfluences.insert(
                armature.boneInfluences.end(), boneInfluences.begin(), boneInfluences.end());
            boneInfluencesOffset += boneInfluences.size();
        }
    }
    return psxModel;
}
