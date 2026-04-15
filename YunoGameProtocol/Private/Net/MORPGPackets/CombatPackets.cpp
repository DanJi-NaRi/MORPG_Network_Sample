#include "pch.h"

#include "CombatPackets.h"

#include "ByteIO.h"

namespace yuno::net::packets
{
    void C2S_SkillCast::Serialize(ByteWriter& w) const
    {
        w.WriteU32LE(casterEntityId);
        w.WriteU32LE(targetEntityId);
        w.WriteU32LE(skillId);
        w.WriteU32LE(castSequence);
    }

    C2S_SkillCast C2S_SkillCast::Deserialize(ByteReader& r)
    {
        C2S_SkillCast pkt{};
        pkt.casterEntityId = r.ReadU32LE();
        pkt.targetEntityId = r.ReadU32LE();
        pkt.skillId = r.ReadU32LE();
        pkt.castSequence = r.ReadU32LE();
        return pkt;
    }

    void S2C_CombatEvent::Serialize(ByteWriter& w) const
    {
        w.WriteU8(static_cast<std::uint8_t>(eventType));
        w.WriteU32LE(eventSequence);
        w.WriteU32LE(sourceEntityId);
        w.WriteU32LE(targetEntityId);
        w.WriteU32LE(skillId);
        w.WriteU32LE(amount);
        w.WriteU32LE(targetHp);
        w.WriteU32LE(targetMaxHp);
        w.WriteU32LE(stateFlags);
    }

    S2C_CombatEvent S2C_CombatEvent::Deserialize(ByteReader& r)
    {
        S2C_CombatEvent pkt{};
        pkt.eventType = static_cast<CombatEventType>(r.ReadU8());
        pkt.eventSequence = r.ReadU32LE();
        pkt.sourceEntityId = r.ReadU32LE();
        pkt.targetEntityId = r.ReadU32LE();
        pkt.skillId = r.ReadU32LE();
        pkt.amount = r.ReadU32LE();
        pkt.targetHp = r.ReadU32LE();
        pkt.targetMaxHp = r.ReadU32LE();
        pkt.stateFlags = r.ReadU32LE();
        return pkt;
    }
}
