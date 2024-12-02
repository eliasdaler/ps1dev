#include "Json2PsxConverter.h"

#include "ModelJsonFile.h"
#include "PsxModel.h"

#include <format>
#include <iostream>

namespace
{
std::int16_t floatToFixed4_12(float v, float scaleFactor)
{
    constexpr auto scale = 1 << 12;
    float ld = v * scaleFactor;
    if (std::abs(ld) > 8) {
        throw std::runtime_error("some vertex position is out of 4.12 range:" + std::to_string(ld));
    }
    bool negative = ld < 0;
    std::uint16_t integer = negative ? -ld : ld;
    std::uint16_t fraction = ld * scale - integer * scale + (negative ? -0.5 : 0.5);
    return integer * scale + fraction;
}

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

                psxFace[i].pos = {
                    .x = floatToFixed4_12(pos.x, params.scale), // X = X
                    .y = floatToFixed4_12(pos.z, params.scale), // Z = -Y
                    .z = floatToFixed4_12(-pos.y, params.scale), // Y = Z
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
    return psxModel;
}
