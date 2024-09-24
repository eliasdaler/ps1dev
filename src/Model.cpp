#include "Model.h"

#include "Utils.h"

#include <utility>

void Model::load(eastl::string_view filename)
{
    const auto data = util::readFile(filename);

    // vertices
    util::FileReader fr{
        .bytes = data.data(),
    };

    const auto numSubmeshes = fr.GetUInt16();
    meshes.reserve(numSubmeshes);

    for (int i = 0; i < numSubmeshes; ++i) {
        Mesh mesh;

        mesh.numTris = fr.GetUInt16();
        mesh.numQuads = fr.GetUInt16();

        mesh.vertices.resize(mesh.numTris * 3 + mesh.numQuads * 4);

        fr.ReadArr(mesh.vertices.data(), mesh.numTris * 3);
        fr.ReadArr(&mesh.vertices[mesh.numTris], mesh.numQuads * 4);

        meshes.push_back(std::move(mesh));
    }
}
