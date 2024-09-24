#include "Model.h"

#include "Utils.h"

#include <utility>

Model loadModel(eastl::string_view filename)
{
    const auto data = util::readFile(filename);

    // vertices
    util::FileReader fr{
        .bytes = data.data(),
    };

    Model model;

    const auto numSubmeshes = fr.GetUInt16();
    model.meshes.reserve(numSubmeshes);

    for (int i = 0; i < numSubmeshes; ++i) {
        Mesh mesh;

        mesh.numTris = fr.GetUInt16();
        mesh.numQuads = fr.GetUInt16();

        mesh.vertices.resize(mesh.numTris * 3 + mesh.numQuads * 4);

        fr.ReadArr(mesh.vertices.data(), mesh.numTris * 3);
        fr.ReadArr(&mesh.vertices[mesh.numTris], mesh.numQuads * 4);

        model.meshes.push_back(std::move(mesh));
    }

    return model;
}
