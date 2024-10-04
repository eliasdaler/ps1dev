#include "Model.h"

#include "FileReader.h"

#include <utility>

void Model::load(const eastl::vector<uint8_t>& data)
{
    // vertices
    util::FileReader fr{
        .bytes = data.data(),
    };

    const auto numSubmeshes = fr.GetUInt16();
    meshes.reserve(numSubmeshes);

    for (int i = 0; i < numSubmeshes; ++i) {
        Mesh mesh;

        mesh.subdivide = (bool)fr.GetUInt16();
        mesh.numUntexturedTris = fr.GetUInt16();
        mesh.numUntexturedQuads = fr.GetUInt16();
        mesh.numTris = fr.GetUInt16();
        mesh.numQuads = fr.GetUInt16();

        const auto numVertices = mesh.numUntexturedTris * 3 + mesh.numUntexturedQuads * 4 +
                                 mesh.numTris * 3 + mesh.numQuads * 4;
        mesh.vertices.resize(numVertices);

        auto verticesPtr = mesh.vertices.data();

        fr.ReadArr(verticesPtr, mesh.numUntexturedTris * 3);
        verticesPtr += mesh.numUntexturedTris * 3;

        fr.ReadArr(verticesPtr, mesh.numUntexturedQuads * 4);
        verticesPtr += mesh.numUntexturedQuads * 4;

        fr.ReadArr(verticesPtr, mesh.numTris * 3);
        verticesPtr += mesh.numTris * 3;

        fr.ReadArr(verticesPtr, mesh.numQuads * 4);
        // verticesPtr += mesh.numUntexturedQuads * 4;

        meshes.push_back(std::move(mesh));
    }
}
