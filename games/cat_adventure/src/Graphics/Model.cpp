#include "Model.h"

#include <common/syscalls/syscalls.h>
#include <utility>

#include <Core/FileReader.h>

void ModelData::load(const eastl::vector<uint8_t>& data)
{
    util::FileReader fr{
        .bytes = data.data(),
    };

    const auto numSubmeshes = fr.GetUInt16();
    meshes.reserve(numSubmeshes);

    for (int i = 0; i < numSubmeshes; ++i) {
        MeshData mesh;

        mesh.jointId = fr.GetUInt16();
        mesh.numUntexturedTris = fr.GetUInt16();
        mesh.numUntexturedQuads = fr.GetUInt16();
        mesh.numTris = fr.GetUInt16();
        mesh.numQuads = fr.GetUInt16();

        const auto numVertices = mesh.numUntexturedTris * 3 + mesh.numUntexturedQuads * 4 +
                                 mesh.numTris * 3 + mesh.numQuads * 4;
        mesh.vertices.resize(numVertices);
        fr.ReadArr(mesh.vertices.data(), numVertices);

        /* ramsyscall_printf(
            "%d, %d, %d, %d\n",
            mesh.numUntexturedTris,
            mesh.numUntexturedQuads,
            mesh.numTris,
            mesh.numQuads); */

        mesh.g3.resize(mesh.numUntexturedTris);
        fr.ReadArr(mesh.g3.data(), mesh.numUntexturedTris);

        mesh.g4.resize(mesh.numUntexturedQuads);
        fr.ReadArr(mesh.g4.data(), mesh.numUntexturedQuads);

        mesh.gt3.resize(mesh.numTris);
        fr.ReadArr(mesh.gt3.data(), mesh.numTris);

        mesh.gt4.resize(mesh.numQuads);
        fr.ReadArr(mesh.gt4.data(), mesh.numQuads);

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

Mesh MeshData::makeInstance() const
{
    Mesh instance{
        .jointId = jointId,
        .vertices = &vertices,
    };

    for (int i = 0; i < 2; ++i) {
        instance.g3[i] = g3;
        instance.g4[i] = g4;
        instance.gt3[i] = gt3;
        instance.gt4[i] = gt4;
    }

    return instance;
}

Model ModelData::makeInstance() const
{
    Model instance{};
    instance.armature = armature;
    instance.meshes.reserve(meshes.size());
    for (const auto& mesh : meshes) {
        instance.meshes.push_back(mesh.makeInstance());
    }

    return instance;
}
