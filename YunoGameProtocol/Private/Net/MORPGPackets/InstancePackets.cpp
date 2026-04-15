#include "pch.h"

#include "InstancePackets.h"

#include <algorithm>
#include <limits>
#include <stdexcept>

#include "ByteIO.h"

namespace yuno::net::packets
{
    namespace
    {
        constexpr std::uint16_t kMaxInstanceParticipants = 8;

        void WriteStringU16(ByteWriter& w, const std::string& value)
        {
            if (value.size() > static_cast<std::size_t>(std::numeric_limits<std::uint16_t>::max()))
                throw std::runtime_error("S2C_InstanceResult string too long");

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
                throw std::runtime_error("S2C_InstanceResult invalid string length");

            std::string out;
            out.reserve(length);
            for (std::uint16_t i = 0; i < length; ++i)
            {
                out.push_back(static_cast<char>(r.ReadU8()));
            }

            return out;
        }
    }

    void InstanceParticipantState::Serialize(ByteWriter& w) const
    {
        w.WriteU32LE(entityId);
        w.WriteU32LE(hp);
        w.WriteU32LE(maxHp);
        w.WriteU8(alive);
    }

    InstanceParticipantState InstanceParticipantState::Deserialize(ByteReader& r)
    {
        InstanceParticipantState state{};
        state.entityId = r.ReadU32LE();
        state.hp = r.ReadU32LE();
        state.maxHp = r.ReadU32LE();
        state.alive = r.ReadU8();
        return state;
    }

    void C2S_InstanceEnter::Serialize(ByteWriter& w) const
    {
        (void)w;
    }

    C2S_InstanceEnter C2S_InstanceEnter::Deserialize(ByteReader& r)
    {
        (void)r;
        return {};
    }

    void S2C_InstanceState::Serialize(ByteWriter& w) const
    {
        w.WriteU16LE(static_cast<std::uint16_t>(resultCode));
        w.WriteU32LE(partyId);
        w.WriteU32LE(instanceId);
        w.WriteU8(static_cast<std::uint8_t>(state));
        w.WriteU32LE(enemyEntityId);
        w.WriteU32LE(enemyHp);
        w.WriteU32LE(enemyMaxHp);

        const std::uint16_t count = static_cast<std::uint16_t>(std::min<std::size_t>(participants.size(), kMaxInstanceParticipants));
        w.WriteU16LE(count);
        for (std::uint16_t i = 0; i < count; ++i)
        {
            participants[i].Serialize(w);
        }
    }

    S2C_InstanceState S2C_InstanceState::Deserialize(ByteReader& r)
    {
        S2C_InstanceState pkt{};
        pkt.resultCode = static_cast<InstanceResultCode>(r.ReadU16LE());
        pkt.partyId = r.ReadU32LE();
        pkt.instanceId = r.ReadU32LE();
        pkt.state = static_cast<InstanceRuntimeState>(r.ReadU8());
        pkt.enemyEntityId = r.ReadU32LE();
        pkt.enemyHp = r.ReadU32LE();
        pkt.enemyMaxHp = r.ReadU32LE();

        std::uint16_t count = r.ReadU16LE();
        if (count > kMaxInstanceParticipants)
            count = kMaxInstanceParticipants;

        pkt.participants.reserve(count);
        for (std::uint16_t i = 0; i < count; ++i)
        {
            pkt.participants.push_back(InstanceParticipantState::Deserialize(r));
        }
        return pkt;
    }

    void S2C_InstanceResult::Serialize(ByteWriter& w) const
    {
        w.WriteU8(success);
        w.WriteU16LE(static_cast<std::uint16_t>(resultCode));
        w.WriteU32LE(instanceId);
        w.WriteU32LE(rewardItemId);
        w.WriteU16LE(rewardQuantity);
        w.WriteU32LE(goldAward);
        WriteStringU16(w, rewardItemCode);
    }

    S2C_InstanceResult S2C_InstanceResult::Deserialize(ByteReader& r)
    {
        S2C_InstanceResult pkt{};
        pkt.success = r.ReadU8();
        pkt.resultCode = static_cast<InstanceResultCode>(r.ReadU16LE());
        pkt.instanceId = r.ReadU32LE();
        pkt.rewardItemId = r.ReadU32LE();
        pkt.rewardQuantity = r.ReadU16LE();
        pkt.goldAward = r.ReadU32LE();
        pkt.rewardItemCode = ReadStringU16(r);
        return pkt;
    }
}
