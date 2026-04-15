#pragma once

#include <cstdint>

namespace yuno::net
{
    class ByteWriter;
    class ByteReader;
}

namespace yuno::net::packets
{
    enum class CombatEventType : std::uint8_t
    {
        Damage = 1,
        Death = 2,
        EncounterCleared = 3,
        EncounterFailed = 4,
    };

    struct C2S_SkillCast final
    {
        std::uint32_t casterEntityId = 0;
        std::uint32_t targetEntityId = 0;
        std::uint32_t skillId = 0;
        std::uint32_t castSequence = 0;

        void Serialize(ByteWriter& w) const;
        static C2S_SkillCast Deserialize(ByteReader& r);
    };

    struct S2C_CombatEvent final
    {
        CombatEventType eventType = CombatEventType::Damage;
        std::uint32_t eventSequence = 0;
        std::uint32_t sourceEntityId = 0;
        std::uint32_t targetEntityId = 0;
        std::uint32_t skillId = 0;
        std::uint32_t amount = 0;
        std::uint32_t targetHp = 0;
        std::uint32_t targetMaxHp = 0;
        std::uint32_t stateFlags = 0;

        void Serialize(ByteWriter& w) const;
        static S2C_CombatEvent Deserialize(ByteReader& r);
    };
}
