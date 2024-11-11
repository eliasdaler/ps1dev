#include "PsxModel.h"

#include <fstream>
#include <iostream>

#include <FsUtil.h>

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

} // end of anonymous namespace

void writePsxModel(const PsxModel& model, const std::filesystem::path& path)
{
    std::ofstream file(path, std::ios::binary);
    fsutil::binaryWrite(file, static_cast<std::uint16_t>(model.submeshes.size()));

    for (const auto& mesh : model.submeshes) {
        fsutil::binaryWrite(file, static_cast<std::uint16_t>(mesh.subdivide));
        fsutil::binaryWrite(file, static_cast<std::uint16_t>(mesh.untexturedTriFaces.size()));
        fsutil::binaryWrite(file, static_cast<std::uint16_t>(mesh.untexturedQuadFaces.size()));
        fsutil::binaryWrite(file, static_cast<std::uint16_t>(mesh.triFaces.size()));
        fsutil::binaryWrite(file, static_cast<std::uint16_t>(mesh.quadFaces.size()));

        writePsxModelVerts(file, mesh.untexturedTriFaces);
        writePsxModelVerts(file, mesh.untexturedQuadFaces);
        writePsxModelVerts(file, mesh.triFaces);
        writePsxModelVerts(file, mesh.quadFaces);
    }
}
