#include "pch.h"

#include "Terrain.h"

#include <vector>

#include "IRenderer.h"
#include "Mesh.h"
#include "ObjectManager.h"
#include "ObjectTypeRegistry.h"
#include "YunoEngine.h"

namespace
{
    struct TerrainSharedPlaneResources
    {
        MeshHandle mesh = 0;
        MaterialHandle material = 0;
        std::uint32_t cols = 0;
        std::uint32_t rows = 0;
        float halfExtent = 0.0f;
    };

    TerrainSharedPlaneResources& GetSharedPlane()
    {
        static TerrainSharedPlaneResources s{};
        return s;
    }

    bool CreateSharedPlane(IRenderer* renderer, std::uint32_t cols, std::uint32_t rows, float halfExtent)
    {
        if (!renderer || cols == 0 || rows == 0 || halfExtent <= 0.0f)
            return false;

        TerrainSharedPlaneResources& shared = GetSharedPlane();
        if (shared.mesh != 0 && shared.material != 0)
            return true;

        std::vector<VERTEX_Pos> positions;
        std::vector<VERTEX_Nrm> normals;
        std::vector<VERTEX_UV> uvs;
        std::vector<INDEX> indices;

        positions.reserve((cols + 1) * (rows + 1));
        normals.reserve((cols + 1) * (rows + 1));
        uvs.reserve((cols + 1) * (rows + 1));
        indices.reserve(cols * rows * 2);

        for (std::uint32_t row = 0; row <= rows; ++row)
        {
            const float v = static_cast<float>(row) / static_cast<float>(rows);
            const float z = (-halfExtent) + (2.0f * halfExtent * v);

            for (std::uint32_t col = 0; col <= cols; ++col)
            {
                const float u = static_cast<float>(col) / static_cast<float>(cols);
                const float x = (-halfExtent) + (2.0f * halfExtent * u);

                positions.push_back({ x, 0.0f, z });
                normals.push_back({ 0.0f, 1.0f, 0.0f });
                uvs.push_back({ u, v });
            }
        }

        const std::uint32_t stride = cols + 1;
        for (std::uint32_t row = 0; row < rows; ++row)
        {
            for (std::uint32_t col = 0; col < cols; ++col)
            {
                const std::uint32_t i0 = row * stride + col;
                const std::uint32_t i1 = i0 + 1;
                const std::uint32_t i2 = i0 + stride;
                const std::uint32_t i3 = i2 + 1;

                indices.push_back({ i0, i2, i1 });
                indices.push_back({ i1, i2, i3 });
            }
        }

        VertexStreams streams{};
        streams.flags = VSF_Pos | VSF_Nrm | VSF_UV;
        streams.vtx_count = static_cast<std::uint32_t>(positions.size());
        streams.pos = positions.data();
        streams.nrm = normals.data();
        streams.uv = uvs.data();

        shared.mesh = renderer->CreateMesh(streams, indices.data(), static_cast<std::uint32_t>(indices.size()));
        shared.material = renderer->CreateMaterial_Default();
        shared.cols = cols;
        shared.rows = rows;
        shared.halfExtent = halfExtent;

        return shared.mesh != 0 && shared.material != 0;
    }

    struct AutoReg_Terrain
    {
        AutoReg_Terrain()
        {
            ObjectTypeRegistry::Instance().Register(
                L"Terrain",
                [](ObjectManager& om, const UnitDesc& d) { om.CreateObjectInternal<Terrain>(d); });
        }
    } s_reg_Terrain;
}

Terrain::Terrain()
{
    unitType = L"Terrain";
}

Terrain::~Terrain() = default;

bool Terrain::BuildStaticPlaneMesh(std::uint32_t cols, std::uint32_t rows, float halfExtent)
{
    IRenderer* renderer = YunoEngine::GetRenderer();
    if (!CreateSharedPlane(renderer, cols, rows, halfExtent))
        return false;

    TerrainSharedPlaneResources& shared = GetSharedPlane();

    auto node = std::make_unique<MeshNode>();
    auto mesh = std::make_unique<Mesh>();
    mesh->Create(shared.mesh, shared.material);
    node->m_Meshs.push_back(std::move(mesh));
    SetMesh(std::move(node));

    return true;
}

bool Terrain::Update(float dTime)
{
    Unit::Update(dTime);
    return true;
}

bool Terrain::Submit(float dTime)
{
    Unit::Submit(dTime);
    return true;
}

