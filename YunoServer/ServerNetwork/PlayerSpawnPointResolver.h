#pragma once

#include <cstdint>
#include <unordered_map>

namespace yuno::server
{
    struct SpawnPoint
    {
        float x = 0.0f;
        float y = 0.0f;
        float z = 0.0f;
    };

    class PlayerSpawnPointResolver final
    {
    public:
        PlayerSpawnPointResolver()
            : m_defaultSpawn{ 0.0f, 0.0f, 0.0f }
        {
            // Starter presets for future region-based entry.
            m_regionSpawns.emplace(0u, m_defaultSpawn);
            m_regionSpawns.emplace(1u, SpawnPoint{ 6.0f, 0.0f, 6.0f });
            m_regionSpawns.emplace(2u, SpawnPoint{ -6.0f, 0.0f, -6.0f });
        }

        SpawnPoint Resolve(std::uint32_t spawnRegionId) const
        {
            const auto it = m_regionSpawns.find(spawnRegionId);
            if (it != m_regionSpawns.end())
                return it->second;

            return m_defaultSpawn;
        }

        SpawnPoint ResolveDefault() const
        {
            return m_defaultSpawn;
        }

        void SetDefault(SpawnPoint spawnPoint)
        {
            m_defaultSpawn = spawnPoint;
            m_regionSpawns[0u] = spawnPoint;
        }

        void SetRegionSpawn(std::uint32_t spawnRegionId, SpawnPoint spawnPoint)
        {
            m_regionSpawns[spawnRegionId] = spawnPoint;
        }

    private:
        SpawnPoint m_defaultSpawn{};
        std::unordered_map<std::uint32_t, SpawnPoint> m_regionSpawns;
    };
}
