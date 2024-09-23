#include "Model.h"

#include "Utils.h"

#include <utility>

Model loadModel(eastl::string_view filename)
{
    Model model;
    const auto data = util::readFile(filename);

    // vertices
    util::FileReader fr{
        .bytes = data.data(),
    };

    const auto numSubmeshes = fr.GetUInt16();
    model.meshes.reserve(numSubmeshes);

    Vertex vertex;

    for (int i = 0; i < numSubmeshes; ++i) {
        Mesh mesh;

        mesh.numTris = fr.GetUInt16();
        mesh.numQuads = fr.GetUInt16();

        mesh.vertices.reserve(mesh.numTris * 3 + mesh.numQuads * 4);

        for (int j = 0; j < mesh.numTris; ++j) {
            for (int k = 0; k < 3; ++k) {
                fr.ReadObj(vertex);
                mesh.vertices.push_back(vertex);
            }
        }

        for (int j = 0; j < mesh.numQuads; ++j) {
            for (int k = 0; k < 4; ++k) {
                fr.ReadObj(vertex);
                mesh.vertices.push_back(vertex);
            }
        }

        model.meshes.push_back(std::move(mesh));
    }

    return model;
}
