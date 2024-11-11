#include "Json2FastModel.h"

#include "FastModel.h"
#include "ModelJsonFile.h"

#include <PsyQTypes.h>

namespace
{
std::int16_t floatToInt16(float v, float scaleFactor = 1.f)
{
    return (std::int16_t)std::clamp(v * scaleFactor, (float)INT16_MIN, (float)INT16_MAX);
}

std::uint8_t uvToInt8(float v)
{
    return (std::int8_t)std::clamp(v * 256.f, 0.f, 255.f);
}

template<typename T>
void binaryWrite(std::ostream& os, const T& val)
{
    os.write(reinterpret_cast<const char*>(&val), sizeof(T));
}

template<typename T>
void binaryWrite(std::ostream& os, const T& val, const std::size_t size)
{
    os.write(reinterpret_cast<const char*>(&val), size);
}

Vec4<std::int16_t> toPSXPos(const glm::vec3& pos)
{
    return {
        .x = floatToInt16(pos.x),
        .y = floatToInt16(-pos.z), //  Y = -Z
        .z = floatToInt16(pos.y), // Z = Y
    };
}

Vec2<std::uint8_t> toPSXUV(const glm::vec2& uv, const glm::vec2& texSize)
{
    const auto offset = 0.f;
    return {
        .x = (std::uint8_t)std::clamp(uv.x * (texSize.x - offset), 0.f, 255.f),
        // Y coord is flipped in UV
        .y = (std::uint8_t)std::clamp((1 - uv.y) * (texSize.y - offset), 0.f, 255.f),
    };
}

Vec4<std::uint8_t> toPSXColor(const glm::vec3& color)
{
    return {
        (std::uint8_t)(color.x / 2.f),
        (std::uint8_t)(color.y / 2.f),
        (std::uint8_t)(color.z / 2.f)};
}

}

FastModel makeFastModel(const ModelJson& model)
{
    FastModel fm;

    struct TriFace {
        std::array<Vertex, 3> vs;
        std::array<Vec2<std::uint8_t>, 3> uvs;
    };

    struct QuadFace {
        std::array<Vertex, 4> vs;
        std::array<Vec2<std::uint8_t>, 4> uvs;
    };

    std::vector<TriFace> triFaces;
    std::vector<QuadFace> quadFaces;

    const auto& material = model.materials[model.meshes[0].materials[0]];
    const auto texSize =
        glm::vec2{(float)material.imageData.width, (float)material.imageData.height};

    static const auto zeroUV = Vec2<std::uint8_t>{};

    // FIXME: if face doesn't have UVs - add it to untextured list?
    // Alternatively we can check if the face has all (0,0) UVs and
    // make POLY_F3/POLY_G3/POLY_F4/POLY_G4 out of it
    for (const auto& object : model.objects) {
        const auto& submesh = model.meshes[object.mesh];
        for (const auto& face : submesh.faces) {
            assert(face.vertices.size() == 3 || face.vertices.size() == 4);
            if (face.vertices.size() == 3) {
                triFaces.push_back(TriFace{
                    .vs =
                        {
                            submesh.vertices[face.vertices[0]],
                            submesh.vertices[face.vertices[2]],
                            submesh.vertices[face.vertices[1]],
                        },
                    .uvs =
                        {
                            face.uvs.empty() ? zeroUV : toPSXUV(face.uvs[0], texSize),
                            face.uvs.empty() ? zeroUV : toPSXUV(face.uvs[2], texSize),
                            face.uvs.empty() ? zeroUV : toPSXUV(face.uvs[1], texSize),
                        },
                });
            } else if (face.vertices.size() == 4) {
                quadFaces.push_back(QuadFace{
                    .vs =
                        {
                            submesh.vertices[face.vertices[3]],
                            submesh.vertices[face.vertices[2]],
                            submesh.vertices[face.vertices[0]],
                            submesh.vertices[face.vertices[1]],
                        },
                    .uvs =
                        {
                            face.uvs.empty() ? zeroUV : toPSXUV(face.uvs[3], texSize),
                            face.uvs.empty() ? zeroUV : toPSXUV(face.uvs[2], texSize),
                            face.uvs.empty() ? zeroUV : toPSXUV(face.uvs[0], texSize),
                            face.uvs.empty() ? zeroUV : toPSXUV(face.uvs[1], texSize),
                        },
                });
            }
        }
    }

    fm.triNum = triFaces.size();
    fm.quadNum = quadFaces.size();

    const auto numVertices = triFaces.size() * 3 + quadFaces.size() * 4;
    fm.vertexData.resize(numVertices);

    const auto primDataSize =
        (triFaces.size() * sizeof(POLY_GT3) + quadFaces.size() * sizeof(POLY_GT4));
    fm.primData.resize(primDataSize);

    auto* trisStart = fm.primData.data();
    auto* trisEnd = trisStart + triFaces.size() * sizeof(POLY_GT3);

    const auto trianglePrims = std::span<POLY_GT3>((POLY_GT3*)trisStart, (POLY_GT3*)trisEnd);
    const auto quadPrims = std::span<
        POLY_GT4>((POLY_GT4*)trisEnd, (POLY_GT4*)(trisEnd + quadFaces.size() * sizeof(POLY_GT4)));

    // TODO: support multiple materials?

    std::size_t currentIndex{0};
    // tris
    for (const auto& face : triFaces) {
        for (int i = 0; i < 3; ++i) {
            const auto& pos = face.vs[i].position;
            fm.vertexData[currentIndex].pos = toPSXPos(pos);
            fm.vertexData[currentIndex].color = toPSXColor(face.vs[i].color);
            ++currentIndex;
        }
    }
    // quads
    for (const auto& face : quadFaces) {
        for (int i = 0; i < 4; ++i) {
            const auto& pos = face.vs[i].position;
            fm.vertexData[currentIndex].pos = toPSXPos(pos);
            fm.vertexData[currentIndex].color = toPSXColor(face.vs[i].color);
            ++currentIndex;
        }
    }

    // TODO: get from TIM?
    // For now, we set tpage and clut at runtime
    /* glm::ivec2 prect = {512, 0};
    glm::ivec2 crect = {0, 483};
    const auto tmode = 0; // 4-bit CLUT

    const auto tpage = getTPage(tmode & 0x3, 0, prect.x, prect.y);
    const auto clut = getClut(crect.x, crect.y); */
    const auto tpage = 0;
    const auto clut = 0;

    for (std::uint16_t triIndex = 0; triIndex < trianglePrims.size(); ++triIndex) {
        auto* polygt3 = &trianglePrims[triIndex];
        setPolyGT3(polygt3);

        const auto& v0Col = fm.vertexData[triIndex * 3 + 0].color;
        const auto& v1Col = fm.vertexData[triIndex * 3 + 1].color;
        const auto& v2Col = fm.vertexData[triIndex * 3 + 2].color;

        const auto& face = triFaces[triIndex];

        setUV3(
            polygt3,
            face.uvs[0].x,
            face.uvs[0].y,
            face.uvs[1].x,
            face.uvs[1].y,
            face.uvs[2].x,
            face.uvs[2].y);

        setRGB0(polygt3, v0Col.x, v0Col.y, v0Col.z);
        setRGB1(polygt3, v1Col.x, v1Col.y, v1Col.z);
        setRGB2(polygt3, v2Col.x, v2Col.y, v2Col.z);

        polygt3->tpage = tpage;
        polygt3->clut = clut;
    }

    std::uint16_t startIndex = trianglePrims.size() * 3;
    for (std::uint16_t quadIndex = 0; quadIndex < quadPrims.size(); ++quadIndex) {
        auto* polygt4 = &quadPrims[quadIndex];
        setPolyGT4(polygt4);

        const auto& v0Col = fm.vertexData[startIndex + quadIndex * 4 + 0].color;
        const auto& v1Col = fm.vertexData[startIndex + quadIndex * 4 + 1].color;
        const auto& v2Col = fm.vertexData[startIndex + quadIndex * 4 + 2].color;
        const auto& v3Col = fm.vertexData[startIndex + quadIndex * 4 + 3].color;

        const auto& face = quadFaces[quadIndex];

        setUV4(
            polygt4,
            face.uvs[0].x,
            face.uvs[0].y,
            face.uvs[1].x,
            face.uvs[1].y,
            face.uvs[2].x,
            face.uvs[2].y,
            face.uvs[3].x,
            face.uvs[3].y);

        setRGB0(polygt4, v0Col.x, v0Col.y, v0Col.z);
        setRGB1(polygt4, v1Col.x, v1Col.y, v1Col.z);
        setRGB2(polygt4, v2Col.x, v2Col.y, v2Col.z);
        setRGB3(polygt4, v3Col.x, v3Col.y, v3Col.z);

        polygt4->tpage = tpage;
        polygt4->clut = clut;
    }

    return fm;
}
