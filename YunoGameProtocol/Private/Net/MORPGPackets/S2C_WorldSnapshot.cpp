#include "pch.h"

#include <algorithm>
#include <cstring>
#include <limits>
#include <stdexcept>

#include "S2C_WorldSnapshot.h"
#include "ByteIO.h"

namespace yuno::net::packets
{
    namespace
    {
        constexpr std::uint16_t kMaxSnapshotEntities = 2048;

        void WriteF32(ByteWriter& w, float v)
        {
            std::uint32_t bits = 0;
            static_assert(sizeof(bits) == sizeof(v), "float/u32 size mismatch");
            std::memcpy(&bits, &v, sizeof(bits));
            w.WriteU32LE(bits);
        }

        float ReadF32(ByteReader& r)
        {
            const std::uint32_t bits = r.ReadU32LE();
            float v = 0.0f;
            static_assert(sizeof(bits) == sizeof(v), "float/u32 size mismatch");
            std::memcpy(&v, &bits, sizeof(v));
            return v;
        }

        void WriteStringU16(ByteWriter& w, const std::string& value)
        {
            if (value.size() > static_cast<std::size_t>(std::numeric_limits<std::uint16_t>::max()))
                throw std::runtime_error("S2C_SpawnEntity displayName too long");

            w.WriteU16LE(static_cast<std::uint16_t>(value.size()));
            for (char ch : value)
            {
                w.WriteU8(static_cast<std::uint8_t>(ch));
            }
        }

        std::string ReadStringU16(ByteReader& r)
        {
            const std::uint16_t length = r.ReadU16LE();
            if (!r.Has(length))
                throw std::runtime_error("S2C_SpawnEntity invalid displayName length");

            std::string out;
            out.reserve(length);
            for (std::uint16_t i = 0; i < length; ++i)
            {
                out.push_back(static_cast<char>(r.ReadU8()));
            }
            return out;
        }
    }

    void EntityPoseState::Serialize(ByteWriter& w) const
    {
        w.WriteU32LE(entityId);
        w.WriteU32LE(stateFlags);
        w.WriteU32LE(lastProcessedInputSequence);
        WriteF32(w, x);
        WriteF32(w, y);
        WriteF32(w, z);
        WriteF32(w, yaw);
        WriteF32(w, vx);
        WriteF32(w, vy);
        WriteF32(w, vz);
    }

    EntityPoseState EntityPoseState::Deserialize(ByteReader& r)
    {
        EntityPoseState s{};
        s.entityId = r.ReadU32LE();
        s.stateFlags = r.ReadU32LE();
        s.lastProcessedInputSequence = r.ReadU32LE();
        s.x = ReadF32(r);
        s.y = ReadF32(r);
        s.z = ReadF32(r);
        s.yaw = ReadF32(r);
        s.vx = ReadF32(r);
        s.vy = ReadF32(r);
        s.vz = ReadF32(r);
        return s;
    }

    void S2C_SpawnEntity::Serialize(ByteWriter& w) const
    {
        w.WriteU32LE(entityId);
        w.WriteU32LE(archetypeId);
        WriteF32(w, x);
        WriteF32(w, y);
        WriteF32(w, z);
        WriteStringU16(w, displayName);
    }

    S2C_SpawnEntity S2C_SpawnEntity::Deserialize(ByteReader& r)
    {
        S2C_SpawnEntity s{};
        s.entityId = r.ReadU32LE();
        s.archetypeId = r.ReadU32LE();
        s.x = ReadF32(r);
        s.y = ReadF32(r);
        s.z = ReadF32(r);
        s.displayName = ReadStringU16(r);
        return s;
    }

    void S2C_DespawnEntity::Serialize(ByteWriter& w) const
    {
        w.WriteU32LE(entityId);
    }

    S2C_DespawnEntity S2C_DespawnEntity::Deserialize(ByteReader& r)
    {
        S2C_DespawnEntity s{};
        s.entityId = r.ReadU32LE();
        return s;
    }

    void S2C_WorldSnapshot::Serialize(ByteWriter& w) const
    {
        w.WriteU32LE(serverTick);
        w.WriteU32LE(snapshotId);
        w.WriteU32LE(baseSnapshotId);

        const std::uint16_t count = static_cast<std::uint16_t>(std::min<std::size_t>(entities.size(), kMaxSnapshotEntities));
        w.WriteU16LE(count);

        for (std::uint16_t i = 0; i < count; ++i)
        {
            entities[i].Serialize(w);
        }
    }

    S2C_WorldSnapshot S2C_WorldSnapshot::Deserialize(ByteReader& r)
    {
        S2C_WorldSnapshot s{};
        s.serverTick = r.ReadU32LE();
        s.snapshotId = r.ReadU32LE();
        s.baseSnapshotId = r.ReadU32LE();

        std::uint16_t count = r.ReadU16LE();
        if (count > kMaxSnapshotEntities)
            count = kMaxSnapshotEntities;

        s.entities.reserve(count);
        for (std::uint16_t i = 0; i < count; ++i)
        {
            s.entities.push_back(EntityPoseState::Deserialize(r));
        }

        return s;
    }
}
