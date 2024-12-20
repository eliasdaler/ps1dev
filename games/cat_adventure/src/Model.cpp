#include "Model.h"

#include "FileReader.h"

#include <utility>

#include <common/syscalls/syscalls.h>

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
        joint.boneInfluencesOffset = fr.GetUInt16();
        joint.boneInfluencesSize = fr.GetUInt16();
    }

    const auto numInfluences = fr.GetUInt16();
    armature.boneInfluences.resize(numInfluences);
    fr.ReadArr(armature.boneInfluences.data(), numInfluences);

    meshes[0].ogVertices = meshes[0].vertices;
}
