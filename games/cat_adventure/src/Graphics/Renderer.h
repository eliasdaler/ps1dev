#pragma once

#define PSYQO_RELEASE

#include <EASTL/array.h>
#include <EASTL/bitset.h>

#include <psyqo/bump-allocator.h>
#include <psyqo/gpu.hh>
#include <psyqo/gte-kernels.hh>
#include <psyqo/primitives/quads.hh>
#include <psyqo/primitives/triangles.hh>
#include <psyqo/trigonometry.hh>

#include <Graphics/Model.h>
#include <Graphics/TextureInfo.h>

struct MeshObject;
struct ModelObject;
struct AnimatedModelObject;
struct Camera;
struct Circle;
struct AABB;
struct TimFile;

struct TileIndex;
struct Tile;
struct Tileset;
struct TileMap;

class Renderer {
public:
    Renderer(psyqo::GPU& gpu);

    void init();

    [[nodiscard]] TextureInfo uploadTIM(const TimFile& tim);

    void drawAnimatedModelObject(
        AnimatedModelObject& object,
        const Camera& camera,
        bool setViewRot = true);
    void drawMeshArmature(
        const AnimatedModelObject& object,
        const Camera& camera,
        const Armature& armature,
        const Mesh& mesh);

    void drawMeshObject(MeshObject& object, const Camera& camera);
    void drawModelObject(ModelObject& object, const Camera& camera, bool setViewRot = true);
    void drawModel(const Model& model);

    void drawMeshFog(const MeshData& meshData);
    void drawMesh(const MeshData& meshData);

    void drawMeshStaticFog(const Mesh& mesh);
    void drawMeshStatic(const Mesh& mesh);

    void drawQuadSubdiv(const psyqo::Prim::GouraudTexturedQuad& quad2d, int avgZ, int addBias);

    void drawTiles(const ModelData& tileModels, const TileMap& tileMap, const Camera& camera);

    void drawTileFog(
        TileIndex tileIndex,
        const Tile& tile,
        const Tileset& tileset,
        const ModelData& tileMeshes,
        const Camera& camera);

    void drawTile(
        TileIndex tileIndex,
        const Tile& tile,
        const Tileset& tileset,
        const ModelData& tileMeshes,
        const Camera& camera);

    void drawTileMeshFog(
        TileIndex tileIndex,
        const Tile& tile,
        const Tileset& tileset,
        const MeshData& mesh,
        const Camera& camera);

    void drawTileMesh(
        TileIndex tileIndex,
        const Tile& tile,
        const Tileset& tileset,
        const MeshData& mesh,
        const Camera& camera);

    static constexpr auto OT_SIZE = 4096 * 2;
    using OrderingTableType = psyqo::OrderingTable<OT_SIZE>;
    eastl::array<OrderingTableType, 2> ots;

    // TODO: can make much smaller once static geometry stores primitives on heap
    static constexpr int PRIMBUFFLEN = 32768 * 10;
    using PrimBufferAllocatorType = psyqo::BumpAllocator<PRIMBUFFLEN>;
    eastl::array<PrimBufferAllocatorType, 2> primBuffers;

    OrderingTableType& getOrderingTable() { return ots[gpu.getParity()]; }
    PrimBufferAllocatorType& getPrimBuffer() { return primBuffers[gpu.getParity()]; }

    psyqo::GPU& getGPU() { return gpu; }

    void calculateViewModelMatrix(
        const Object& object,
        const Camera& camera,
        bool setViewRot = true);

    int bias{0};

    void drawObjectAxes(const Object& object, const Camera& camera);

    /* This function assumes that V*M is already loaded into R and T, e.g. call
     * renderer.calculateViewModelMatrix(object, camera, true); before calling this
     */
    void drawLineLocalSpace(const psyqo::Vec3& a, const psyqo::Vec3& b, const psyqo::Color& c);

    /*
     * This function is kinda limited - can only draw lines that are ~16
     * units long. But they can start and end at any world coord.
     */
    void drawLineWorldSpace(
        const Camera& camera,
        const psyqo::Vec3& a,
        const psyqo::Vec3& b,
        const psyqo::Color& c,
        bool cameraViewLoaded = false);

    void drawPointWorldSpace(
        const Camera& camera,
        const psyqo::Vec3& p,
        const psyqo::Color& c,
        bool cameraViewLoaded = false);

    void drawAABB(const Camera& camera, const AABB& aabb, const psyqo::Color& c);

    void drawCircle(const Camera& camera, const Circle& circle, const psyqo::Color& c);

    void drawArmature(const AnimatedModelObject& object, const Camera& camera);

    void setFogNearFar(psyqo::FixedPoint<> near, psyqo::FixedPoint<> far);
    void setFarColor(const psyqo::Color& c);
    uint32_t calcInterpFactor(uint32_t sz);

    void setFOV(uint32_t nh);

    void setFogEnabled(bool b) { fogEnabled = b; };
    bool isFogEnabled() const { return fogEnabled; }

    void setFogColor(psyqo::Color c) { fogColor = c; }
    psyqo::Color getFogColor() const { return fogColor; }

    psyqo::Color fogColor = psyqo::Color{.r = 108, .g = 100, .b = 116};

    int numTilesDrawn{0};

    // size of the tile visibility bitfield (MAX_TILES_DIM * MAX_TILES_DIM)
    static constexpr auto MAX_TILES_DIM = 32;

private:
    bool shouldCullObject(const Object& object, const Camera& camera) const;
    void calculateTileVisibility(const Camera& camera);

    psyqo::GPU& gpu;
    psyqo::Trig<> trig;

    void drawArmature(
        const Armature& armature,
        const AnimatedModelObject& object,
        const Joint& joint,
        Joint::JointId childId);

    std::uint32_t dqa{};
    std::uint32_t dqb{};
    std::uint32_t h{300};

    bool fogEnabled{true};

    // used to draw a "fade rect" when doing fog for dynamic objects
    // calculated while drawing prims in drawMeshFog
    uint32_t minAvgZ;
    uint32_t minAvgP;
    int16_t minSX;
    int16_t minSY;
    int16_t maxSX;
    int16_t maxSY;

    // min/max tile visible in the current frame
    int minTileX, maxTileX;
    int minTileZ, maxTileZ;

    // Tiles which can be seen by the camera
    // Stored in relative coordinates to the minimum point of frustum's AABB in XZ plane
    eastl::bitset<MAX_TILES_DIM * MAX_TILES_DIM> tileSeen;
};
