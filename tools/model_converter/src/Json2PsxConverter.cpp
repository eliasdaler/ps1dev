#include "Json2PsxConverter.h"

#include "ModelJsonFile.h"
#include "PsxModel.h"
#include "TexturesData.h"

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

PsxSubmesh processMesh(
    const ModelJson& modelJson,
    const TexturesData& textures,
    const ConversionParams& params,
    const Object& object,
    const Mesh& mesh)
{
    static const auto zeroUV = glm::vec2{};
    static const glm::mat4 I{1.0};

    const auto tm = object.transform.asMatrix();

    PsxSubmesh psxMesh;

    if (object.name.ends_with(".SD")) {
        psxMesh.subdivide = true;
    }

    // TODO: support multiple materials?
    const auto& material = modelJson.materials[mesh.materials[0]];
    bool hasTexture = !material.imageData.pixels.empty();

    const TextureData* td{nullptr};
    if (hasTexture) {
        td = &textures.get(material.texture);
    }

    // Vertices are stored in joint space if model is affected by a joint
    glm::mat4 ib = mesh.jointId != -1 ? modelJson.armature.inverseBindMatrices[mesh.jointId] : I;
    psxMesh.jointId = mesh.jointId;

    for (const auto& face : mesh.faces) {
        std::array<PsxVert, 4> psxFace;
        if (face.vertices.size() != 3 && face.vertices.size() != 4) {
            throw std::runtime_error(std::format(
                "bad face in submesh {}: has {} vertices", object.name, face.vertices.size()));
        }
        assert(face.vertices.size() <= 4);
        for (std::size_t i = 0; i < face.vertices.size(); ++i) {
            const auto& v = mesh.vertices[face.vertices[i]];

            // Note: if not affected by a joint, ib will be I here
            const auto pos = glm::vec3{ib * tm * glm::vec4{v.position, 1.f}};

            psxFace[i].originalIndex = face.vertices[i];
            psxFace[i].pos = {
                .x = floatToFixed<FixedPoint4_12>(pos.x, params.scale),
                .y = floatToFixed<FixedPoint4_12>(pos.y, params.scale),
                .z = floatToFixed<FixedPoint4_12>(pos.z, params.scale),
            };

            float offset = 0;

            if (hasTexture) {
                const auto texWidth = material.imageData.width;
                const auto texHeight = material.imageData.height;
                const auto& uv = face.uvs.empty() ? zeroUV : face.uvs[i];
                psxFace[i].uv = {
                    .x = (std::uint8_t)std::clamp(uv.x * (texWidth - offset), 0.f, 255.f),
                    // Y coord is flipped in UV
                    .y = (std::uint8_t)std::clamp((1 - uv.y) * (texHeight - offset), 0.f, 255.f),
                };
                psxFace[i].clut = td->clut;
                psxFace[i].tpage = td->tpage;
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
    }
    return psxMesh;
}

} // end of anonymous namespace

PsxModel jsonToPsxModel(
    const ModelJson& modelJson,
    const TexturesData& textures,
    const ConversionParams& params)
{
    PsxModel psxModel;

    for (const auto& object : modelJson.objects) {
        if (object.mesh == -1) {
            continue;
        }
        const auto& mesh = modelJson.meshes[object.mesh];
        psxModel.submeshes.push_back(processMesh(modelJson, textures, params, object, mesh));
    }

    if (!modelJson.armature.joints.empty()) {
        // convert submeshes - previous loop won't be writing them because
        // the object with armature doesn't have a mesh set (all meshes are submeshes
        // of this object in JSON)
        assert(modelJson.objects.size() == 1);
        const auto& object = modelJson.objects[0];
        for (const auto& mesh : modelJson.meshes) {
            psxModel.submeshes.push_back(processMesh(modelJson, textures, params, object, mesh));
        }

        // convert joints
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
    }
    return psxModel;
}
