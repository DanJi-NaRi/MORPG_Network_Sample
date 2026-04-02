#pragma once

#include <cstdint>
#include <vector>

namespace yuno::net
{
    class ByteWriter;
    class ByteReader;
}

namespace yuno::net::packets
{
    struct EntityPoseState
    {
        std::uint32_t entityId = 0;
        std::uint32_t stateFlags = 0;
        std::uint32_t lastProcessedInputSequence = 0;
        float x = 0.0f;
        float y = 0.0f;
        float z = 0.0f;
        float yaw = 0.0f;
        float vx = 0.0f;
        float vy = 0.0f;
        float vz = 0.0f;

        void Serialize(ByteWriter& w) const;
        static EntityPoseState Deserialize(ByteReader& r);
    };

    struct S2C_SpawnEntity final
    {
        std::uint32_t entityId = 0;
        std::uint32_t archetypeId = 0;
        float x = 0.0f;
        float y = 0.0f;
        float z = 0.0f;

        void Serialize(ByteWriter& w) const;
        static S2C_SpawnEntity Deserialize(ByteReader& r);
    };

    struct S2C_DespawnEntity final
    {
        std::uint32_t entityId = 0;

        void Serialize(ByteWriter& w) const;
        static S2C_DespawnEntity Deserialize(ByteReader& r);
    };

    struct S2C_WorldSnapshot final
    {
        std::uint32_t serverTick = 0;
        std::uint32_t snapshotId = 0;
        std::uint32_t baseSnapshotId = 0;
        std::vector<EntityPoseState> entities;

        void Serialize(ByteWriter& w) const;
        static S2C_WorldSnapshot Deserialize(ByteReader& r);
    };
}
