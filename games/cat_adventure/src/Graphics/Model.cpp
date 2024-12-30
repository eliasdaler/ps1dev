#include "Model.h"

#include <common/syscalls/syscalls.h>
#include <utility>

#include <Core/FileReader.h>

void Model::load(const eastl::vector<uint8_t>& data)
{
    util::FileReader fr{
        .bytes = data.data(),
    };

    const auto numSubmeshes = fr.GetUInt16();
    meshes.reserve(numSubmeshes);

    for (int i = 0; i < numSubmeshes; ++i) {
        Mesh mesh;

        mesh.jointId = fr.GetUInt16();
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
        verticesPtr += mesh.numQuads * 4;

        /* for (const auto& vertex : mesh.vertices) {
            ramsyscall_printf(
                "%d, %d, %d\n", vertex.pos.x.raw(), vertex.pos.y.raw(), vertex.pos.z.raw());
        } */

        meshes.push_back(std::move(mesh));
    }

    if (fr.cursor == data.size()) {
        return;
    }

    const auto numJoints = fr.GetUInt16();
    armature.joints.resize(numJoints);
    for (int i = 0; i < numJoints; ++i) {
        auto& joint = armature.joints[i];
        joint.id = i;

        auto& translation = joint.localTransform.translation;
        translation.x.value = fr.GetInt16();
        translation.y.value = fr.GetInt16();
        translation.z.value = fr.GetInt16();
        fr.SkipBytes(2); // pad

        auto& rotation = joint.localTransform.rotation;
        rotation.w.value = fr.GetInt16();
        rotation.x.value = fr.GetInt16();
        rotation.y.value = fr.GetInt16();
        rotation.z.value = fr.GetInt16();

        joint.firstChild = fr.GetUInt8();
        joint.nextSibling = fr.GetUInt8();
    }
}

void FastModel::load(const eastl::vector<uint8_t>& data)
{
    util::FileReader fr{
        .bytes = data.data(),
    };

    const auto numSubmeshes = fr.GetUInt16();
    meshes.reserve(numSubmeshes);

    for (int i = 0; i < numSubmeshes; ++i) {
        FastMesh mesh;

        mesh.jointId = fr.GetUInt16();
        mesh.numUntexturedTris = fr.GetUInt16();
        mesh.numUntexturedQuads = fr.GetUInt16();
        mesh.numTris = fr.GetUInt16();
        mesh.numQuads = fr.GetUInt16();

        const auto numVertices = mesh.numUntexturedTris * 3 + mesh.numUntexturedQuads * 4 +
                                 mesh.numTris * 3 + mesh.numQuads * 4;
        mesh.vertices.resize(numVertices);
        fr.ReadArr(mesh.vertices.data(), numVertices);

        // ramsyscall_printf("%d, %d\n", mesh.numTris, mesh.numQuads);

        mesh.gt3[0].resize(mesh.numTris);
        fr.ReadArr(mesh.gt3[0].data(), mesh.numTris);

        mesh.gt4[0].resize(mesh.numQuads);
        fr.ReadArr(mesh.gt4[0].data(), mesh.numQuads);

        // TODO: memcpy?
        mesh.gt3[1] = mesh.gt3[0];
        mesh.gt4[1] = mesh.gt4[0];

        meshes.push_back(std::move(mesh));
    }

    return;

    if (fr.cursor == data.size()) {
        return;
    }

    const auto numJoints = fr.GetUInt16();
    armature.joints.resize(numJoints);
    for (int i = 0; i < numJoints; ++i) {
        auto& joint = armature.joints[i];
        joint.id = i;

        auto& translation = joint.localTransform.translation;
        translation.x.value = fr.GetInt16();
        translation.y.value = fr.GetInt16();
        translation.z.value = fr.GetInt16();
        fr.SkipBytes(2); // pad

        auto& rotation = joint.localTransform.rotation;
        rotation.w.value = fr.GetInt16();
        rotation.x.value = fr.GetInt16();
        rotation.y.value = fr.GetInt16();
        rotation.z.value = fr.GetInt16();

        joint.firstChild = fr.GetUInt8();
        joint.nextSibling = fr.GetUInt8();
    }
}
