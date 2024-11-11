#include "Obj2PsxConverter.h"

#include "ObjFile.h"
#include "PsxModel.h"

#include <algorithm>
#include <cassert>
#include <iostream>

namespace
{
std::int16_t floatToInt16(float v, float scaleFactor)
{
    return (std::int16_t)std::clamp(v * scaleFactor, (float)INT16_MIN, (float)INT16_MAX);
}

std::uint8_t uvToInt8(float v)
{
    return (std::int8_t)std::clamp(v * 256.f, 0.f, 255.f);
}
}

PsxModel objToPsxModel(const ObjModel& objModel, const ConversionParams& params)
{
    PsxModel psxModel;
    for (const auto& mesh : objModel.meshes) {
        PsxSubmesh psxMesh;
        for (const auto& face : mesh.faces) {
            std::array<PsxVert, 4> psxFace;
            for (std::size_t i = 0; i < face.numVertices; ++i) {
                const auto& pos = objModel.vertices.at(face.indices[i][POS_ATTR] - 1);
                // This assumes that .obj was exported from Blender with
                // -Z forward axis and Y up axis (defaults)
                psxFace[i].pos = {
                    .x = floatToInt16(pos.x, params.scale),
                    .y = floatToInt16(-pos.y, params.scale), // FLIP Y!
                    .z = floatToInt16(-pos.z, params.scale), // FLIP Z!
                };

                const auto& uv = objModel.uvs.at(face.indices[i][UV_ATTR] - 1);
                // TODO: read from texture!
                int texWidth = 128;
                int texHeight = 128;
                float offset = 1;
                // float offset = 0;
                // int texWidth = 256;
                // int texHeight = 256;
                psxFace[i].uv = {
                    .x = (std::uint8_t)std::clamp(uv.x * (texWidth - offset), 0.f, 255.f),
                    // Y coord is flipped in UV
                    .y = (std::uint8_t)std::clamp((1 - uv.y) * (texHeight - offset), 0.f, 255.f),
                };
                // std::cout << (int)psxFace[i].uv.x << " " << (int)psxFace[i].uv.y << std::endl;
            }
            if (face.numVertices == 3) {
                psxMesh.triFaces.push_back({psxFace[0], psxFace[2], psxFace[1]});
            } else {
                assert(face.numVertices == 4);
                // note the order - that's how PS1 quads work
                psxMesh.quadFaces.push_back({psxFace[3], psxFace[2], psxFace[0], psxFace[1]});
            }
        }
        psxModel.submeshes.push_back(std::move(psxMesh));
    }
    return psxModel;
}
