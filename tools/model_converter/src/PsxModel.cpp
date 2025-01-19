#include "PsxModel.h"

#include <fstream>
#include <iostream>

#include <FsUtil.h>

#include "GTETypes.h"

namespace
{

static const std::uint8_t pad8{0};
static const std::uint16_t pad16{0};

void writeG3Prims(std::ofstream& file, const std::vector<PsxTriFace>& faces)
{
    for (const auto& face : faces) {
        GouraudTriangle tri{
            // 0
            .r0 = face.vs[0].color.x,
            .g0 = face.vs[0].color.y,
            .b0 = face.vs[0].color.z,
            .code = G3_CODE,
            // 1
            .r1 = face.vs[1].color.x,
            .g1 = face.vs[1].color.y,
            .b1 = face.vs[1].color.z,
            // 2
            .r2 = face.vs[2].color.x,
            .g2 = face.vs[2].color.y,
            .b2 = face.vs[2].color.z,
        };
        fsutil::binaryWrite(file, tri);
    }
}

void writeG4Prims(std::ofstream& file, const std::vector<PsxQuadFace>& faces)
{
    for (const auto& face : faces) {
        GouraudQuad quad{
            // 0
            .r0 = face.vs[0].color.x,
            .g0 = face.vs[0].color.y,
            .b0 = face.vs[0].color.z,
            .code = G4_CODE,
            // 1
            .r1 = face.vs[1].color.x,
            .g1 = face.vs[1].color.y,
            .b1 = face.vs[1].color.z,
            // 2
            .r2 = face.vs[2].color.x,
            .g2 = face.vs[2].color.y,
            .b2 = face.vs[2].color.z,
            // 3
            .r3 = face.vs[3].color.x,
            .g3 = face.vs[3].color.y,
            .b3 = face.vs[3].color.z,
        };
        fsutil::binaryWrite(file, quad);
    }
}

void writeGT3Prims(std::ofstream& file, const std::vector<PsxTriFace>& faces)
{
    for (const auto& face : faces) {
        GouraudTexturedTriangle tri{
            // 0
            .r0 = face.vs[0].color.x,
            .g0 = face.vs[0].color.y,
            .b0 = face.vs[0].color.z,
            .code = GT3_CODE,
            .u0 = face.vs[0].uv.x,
            .v0 = face.vs[0].uv.y,
            .clut = static_cast<uint16_t>(face.vs[0].clut),
            // 1
            .r1 = face.vs[1].color.x,
            .g1 = face.vs[1].color.y,
            .b1 = face.vs[1].color.z,
            .p1 = (std::uint8_t)face.bias, // HACK
            .u1 = face.vs[1].uv.x,
            .v1 = face.vs[1].uv.y,
            .tpage = static_cast<uint16_t>(face.vs[0].tpage),
            // 2
            .r2 = face.vs[2].color.x,
            .g2 = face.vs[2].color.y,
            .b2 = face.vs[2].color.z,
            .u2 = face.vs[2].uv.x,
            .v2 = face.vs[2].uv.y,
            .pad2 = (std::uint16_t)face.bias, // HACK!
        };
        fsutil::binaryWrite(file, tri);
    }
}

void writeGT4Prims(std::ofstream& file, const std::vector<PsxQuadFace>& faces)
{
    for (const auto& face : faces) {
        GouraudTexturedQuad quad{
            // 0
            .r0 = face.vs[0].color.x,
            .g0 = face.vs[0].color.y,
            .b0 = face.vs[0].color.z,
            .code = face.semiTrans ? GT4_CODE_SEMITRANS : GT4_CODE,
            .u0 = face.vs[0].uv.x,
            .v0 = face.vs[0].uv.y,
            .clut = static_cast<uint16_t>(face.vs[0].clut),
            // 1
            .r1 = face.vs[1].color.x,
            .g1 = face.vs[1].color.y,
            .b1 = face.vs[1].color.z,
            .u1 = face.vs[1].uv.x,
            .v1 = face.vs[1].uv.y,
            .tpage = static_cast<uint16_t>(face.vs[0].tpage),
            // 2
            .r2 = face.vs[2].color.x,
            .g2 = face.vs[2].color.y,
            .b2 = face.vs[2].color.z,
            .u2 = face.vs[2].uv.x,
            .v2 = face.vs[2].uv.y,
            .pad2 = (std::uint16_t)face.bias, // HACK!
            // 3
            .r3 = face.vs[3].color.x,
            .g3 = face.vs[3].color.y,
            .b3 = face.vs[3].color.z,
            .u3 = face.vs[3].uv.x,
            .v3 = face.vs[3].uv.y,
        };
        fsutil::binaryWrite(file, quad);
    }
}

} // end of anonymous namespace

void writePsxModel(const PsxModel& model, const std::filesystem::path& path)
{
    std::ofstream file(path, std::ios::binary);
    writePsxModel(model, file);
}

// appends to a file
void writePsxModel(const PsxModel& model, std::ofstream& file)
{
    std::uint16_t flags{0};
    flags |= (!model.armature.joints.empty());
    // 15 bits unused for now

    fsutil::binaryWrite(file, flags);
    fsutil::binaryWrite(file, static_cast<std::uint16_t>(model.submeshes.size()));
    for (const auto& mesh : model.submeshes) {
        fsutil::binaryWrite(file, static_cast<std::uint16_t>(mesh.jointId));
        fsutil::binaryWrite(file, static_cast<std::uint16_t>(mesh.untexturedTriFaces.size()));
        fsutil::binaryWrite(file, static_cast<std::uint16_t>(mesh.untexturedQuadFaces.size()));
        fsutil::binaryWrite(file, static_cast<std::uint16_t>(mesh.triFaces.size()));
        fsutil::binaryWrite(file, static_cast<std::uint16_t>(mesh.quadFaces.size()));

        for (const auto& face : mesh.untexturedTriFaces) {
            for (int i = 0; i < 3; ++i) {
                fsutil::binaryWrite(file, face.vs[i].pos.x);
                fsutil::binaryWrite(file, face.vs[i].pos.y);
                fsutil::binaryWrite(file, face.vs[i].pos.z);
                fsutil::binaryWrite(file, pad16);
            }
        }
        for (const auto& face : mesh.untexturedQuadFaces) {
            for (int i = 0; i < 4; ++i) {
                fsutil::binaryWrite(file, face.vs[i].pos.x);
                fsutil::binaryWrite(file, face.vs[i].pos.y);
                fsutil::binaryWrite(file, face.vs[i].pos.z);
                fsutil::binaryWrite(file, pad16);
            }
        }
        for (const auto& face : mesh.triFaces) {
            for (int i = 0; i < 3; ++i) {
                fsutil::binaryWrite(file, face.vs[i].pos.x);
                fsutil::binaryWrite(file, face.vs[i].pos.y);
                fsutil::binaryWrite(file, face.vs[i].pos.z);
                fsutil::binaryWrite(file, pad16);
            }
        }
        for (const auto& face : mesh.quadFaces) {
            for (int i = 0; i < 4; ++i) {
                fsutil::binaryWrite(file, face.vs[i].pos.x);
                fsutil::binaryWrite(file, face.vs[i].pos.y);
                fsutil::binaryWrite(file, face.vs[i].pos.z);
                fsutil::binaryWrite(file, pad16);
            }
        }

        writeG3Prims(file, mesh.untexturedTriFaces);
        writeG4Prims(file, mesh.untexturedQuadFaces);
        writeGT3Prims(file, mesh.triFaces);
        writeGT4Prims(file, mesh.quadFaces);
    }

    if (!model.armature.joints.empty()) {
        fsutil::binaryWrite(file, static_cast<std::uint16_t>(model.armature.joints.size()));
        for (const auto& joint : model.armature.joints) {
            fsutil::binaryWrite(file, joint.translation.x);
            fsutil::binaryWrite(file, joint.translation.y);
            fsutil::binaryWrite(file, joint.translation.z);
            fsutil::binaryWrite(file, pad16);

            fsutil::binaryWrite(file, joint.rotation.x);
            fsutil::binaryWrite(file, joint.rotation.y);
            fsutil::binaryWrite(file, joint.rotation.z);
            fsutil::binaryWrite(file, joint.rotation.w);
            fsutil::binaryWrite(file, joint.firstChild);
            fsutil::binaryWrite(file, joint.nextSibling);
        }
    }
}
