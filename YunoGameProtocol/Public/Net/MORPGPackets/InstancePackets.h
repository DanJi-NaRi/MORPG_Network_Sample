#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace yuno::net
{
    class ByteWriter;
    class ByteReader;
}

namespace yuno::net::packets
{
    enum class InstanceResultCode : std::uint16_t
    {
        None = 0,
        NotInParty = 1,
        NotPartyLeader = 2,
        PartyNotReady = 3,
        AlreadyInInstance = 4,
        InstanceNotFound = 5,
        NotParticipant = 6,
        InvalidState = 7,
    };

    enum class InstanceRuntimeState : std::uint8_t
    {
        Town = 0,
        Active = 1,
        Cleared = 2,
        Failed = 3,
    };

    struct InstanceParticipantState final
    {
        std::uint32_t entityId = 0;
        std::uint32_t hp = 0;
        std::uint32_t maxHp = 0;
        std::uint8_t alive = 0;

        void Serialize(ByteWriter& w) const;
        static InstanceParticipantState Deserialize(ByteReader& r);
    };

    struct C2S_InstanceEnter final
    {
        void Serialize(ByteWriter& w) const;
        static C2S_InstanceEnter Deserialize(ByteReader& r);
    };

    struct S2C_InstanceState final
    {
        InstanceResultCode resultCode = InstanceResultCode::None;
        std::uint32_t partyId = 0;
        std::uint32_t instanceId = 0;
        InstanceRuntimeState state = InstanceRuntimeState::Town;
        std::uint32_t enemyEntityId = 0;
        std::uint32_t enemyHp = 0;
        std::uint32_t enemyMaxHp = 0;
        std::vector<InstanceParticipantState> participants;

        void Serialize(ByteWriter& w) const;
        static S2C_InstanceState Deserialize(ByteReader& r);
    };

    struct S2C_InstanceResult final
    {
        std::uint8_t success = 0;
        InstanceResultCode resultCode = InstanceResultCode::None;
        std::uint32_t instanceId = 0;
        std::uint32_t rewardItemId = 0;
        std::uint16_t rewardQuantity = 0;
        std::uint32_t goldAward = 0;
        std::string rewardItemCode;

        void Serialize(ByteWriter& w) const;
        static S2C_InstanceResult Deserialize(ByteReader& r);
    };
}
