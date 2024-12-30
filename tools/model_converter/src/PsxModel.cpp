#include "PsxModel.h"

#include <fstream>
#include <iostream>

#include <FsUtil.h>

#include "GTETypes.h"

namespace
{

static const std::uint8_t pad8{0};
static const std::uint16_t pad16{0};

template<std::size_t N>
void writePsxModelVerts(std::ofstream& file, const std::vector<std::array<PsxVert, N>>& faces)
{
    for (const auto& face : faces) {
        for (int i = 0; i < N; ++i) {
            fsutil::binaryWrite(file, face[i].pos.x);
            fsutil::binaryWrite(file, face[i].pos.y);
            fsutil::binaryWrite(file, face[i].pos.z);
            fsutil::binaryWrite(file, pad16);

            fsutil::binaryWrite(file, face[i].uv.x);
            fsutil::binaryWrite(file, face[i].uv.y);
            fsutil::binaryWrite(file, pad8);
            fsutil::binaryWrite(file, pad8);

            fsutil::binaryWrite(file, face[i].color.x);
            fsutil::binaryWrite(file, face[i].color.y);
            fsutil::binaryWrite(file, face[i].color.z);
            fsutil::binaryWrite(file, pad8);
        }
    }
}

void writeGT3Prims(std::ofstream& file, const std::vector<std::array<PsxVert, 3>>& faces)
{
    auto clut = getClut(0, 241); // TODO: don't hardcode
    auto tpage = getTPage(1, 1, 512, 0);
    if (faces.size() == 16) {
        clut = getClut(0, 240);
        tpage = getTPage(1, 1, 320, 0);
    }

    for (const auto& face : faces) {
        fsutil::binaryWrite(file, static_cast<std::uint32_t>(0)); // head

        GouraudTexturedTriangle quad{
            // 0
            .r0 = face[0].color.x,
            .g0 = face[0].color.y,
            .b0 = face[0].color.z,
            .code = 0x34,
            .u0 = face[0].uv.x,
            .v0 = face[0].uv.y,
            .clut = static_cast<uint16_t>(clut),
            // 1
            .r1 = face[1].color.x,
            .g1 = face[1].color.y,
            .b1 = face[1].color.z,
            .u1 = face[1].uv.x,
            .v1 = face[1].uv.y,
            .tpage = static_cast<uint16_t>(tpage),
            // 2
            .r2 = face[2].color.x,
            .g2 = face[2].color.y,
            .b2 = face[2].color.z,
            .u2 = face[2].uv.x,
            .v2 = face[2].uv.y,
        };
        fsutil::binaryWrite(file, quad);
    }
}

void writeGT4Prims(std::ofstream& file, const std::vector<std::array<PsxVert, 4>>& faces)
{
    auto clut = getClut(0, 241); // TODO: don't hardcode
    auto tpage = getTPage(1, 1, 512, 0);
    if (faces.size() == 955) {
        clut = getClut(0, 240);
        tpage = getTPage(1, 1, 320, 0);
    }

    for (const auto& face : faces) {
        fsutil::binaryWrite(file, static_cast<std::uint32_t>(0)); // head

        GouraudTexturedQuad quad{
            // 0
            .r0 = face[0].color.x,
            .g0 = face[0].color.y,
            .b0 = face[0].color.z,
            .code = 0x3c,
            .u0 = face[0].uv.x,
            .v0 = face[0].uv.y,
            .clut = static_cast<uint16_t>(clut),
            // 1
            .r1 = face[1].color.x,
            .g1 = face[1].color.y,
            .b1 = face[1].color.z,
            .u1 = face[1].uv.x,
            .v1 = face[1].uv.y,
            .tpage = static_cast<uint16_t>(tpage),
            // 2
            .r2 = face[2].color.x,
            .g2 = face[2].color.y,
            .b2 = face[2].color.z,
            .u2 = face[2].uv.x,
            .v2 = face[2].uv.y,
            // 3
            .r3 = face[3].color.x,
            .g3 = face[3].color.y,
            .b3 = face[3].color.z,
            .u3 = face[3].uv.x,
            .v3 = face[3].uv.y,
        };
        fsutil::binaryWrite(file, quad);
    }
}

} // end of anonymous namespace

void writePsxModel(const PsxModel& model, const std::filesystem::path& path)
{
    std::ofstream file(path, std::ios::binary);
    fsutil::binaryWrite(file, static_cast<std::uint16_t>(model.submeshes.size()));

    for (const auto& mesh : model.submeshes) {
        fsutil::binaryWrite(file, static_cast<std::uint16_t>(mesh.jointId));
        fsutil::binaryWrite(file, static_cast<std::uint16_t>(mesh.untexturedTriFaces.size()));
        fsutil::binaryWrite(file, static_cast<std::uint16_t>(mesh.untexturedQuadFaces.size()));
        fsutil::binaryWrite(file, static_cast<std::uint16_t>(mesh.triFaces.size()));
        fsutil::binaryWrite(file, static_cast<std::uint16_t>(mesh.quadFaces.size()));

        writePsxModelVerts(file, mesh.untexturedTriFaces);
        writePsxModelVerts(file, mesh.untexturedQuadFaces);
        writePsxModelVerts(file, mesh.triFaces);
        writePsxModelVerts(file, mesh.quadFaces);
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

void writeFastPsxModel(const PsxModel& model, const std::filesystem::path& path)
{
    std::ofstream file(path, std::ios::binary);
    fsutil::binaryWrite(file, static_cast<std::uint16_t>(model.submeshes.size()));

    for (const auto& mesh : model.submeshes) {
        fsutil::binaryWrite(file, static_cast<std::uint16_t>(mesh.jointId));
        // fsutil::binaryWrite(file, static_cast<std::uint16_t>(mesh.untexturedTriFaces.size()));
        // fsutil::binaryWrite(file, static_cast<std::uint16_t>(mesh.untexturedQuadFaces.size()));
        // fsutil::binaryWrite(file, static_cast<std::uint16_t>(mesh.triFaces.size()));

        fsutil::binaryWrite(file, static_cast<std::uint16_t>(0));
        fsutil::binaryWrite(file, static_cast<std::uint16_t>(0));
        fsutil::binaryWrite(file, static_cast<std::uint16_t>(mesh.triFaces.size()));
        fsutil::binaryWrite(file, static_cast<std::uint16_t>(mesh.quadFaces.size()));

        for (const auto& face : mesh.triFaces) {
            for (int i = 0; i < 3; ++i) {
                fsutil::binaryWrite(file, face[i].pos.x);
                fsutil::binaryWrite(file, face[i].pos.y);
                fsutil::binaryWrite(file, face[i].pos.z);
                fsutil::binaryWrite(file, pad16);
            }
        }
        for (const auto& face : mesh.quadFaces) {
            for (int i = 0; i < 4; ++i) {
                fsutil::binaryWrite(file, face[i].pos.x);
                fsutil::binaryWrite(file, face[i].pos.y);
                fsutil::binaryWrite(file, face[i].pos.z);
                fsutil::binaryWrite(file, pad16);
            }
        }

        // writePsxModelVerts(file, mesh.untexturedTriFaces);
        // writePsxModelVerts(file, mesh.untexturedQuadFaces);
        // writePsxModelVerts(file, mesh.triFaces);
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
