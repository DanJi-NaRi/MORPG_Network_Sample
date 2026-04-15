#pragma once

#include <cstdint>
#include <unordered_map>
#include <vector>

#include "CombatPackets.h"
#include "InstancePackets.h"

namespace yuno::server
{
    class InstanceManager final
    {
    public:
        struct ParticipantSeed final
        {
            std::uint64_t sessionId = 0;
            std::uint32_t entityId = 0;
        };

        struct ParticipantRuntime final
        {
            std::uint64_t sessionId = 0;
            std::uint32_t entityId = 0;
            std::uint32_t hp = 100;
            std::uint32_t maxHp = 100;
            bool alive = true;
        };

        struct Instance final
        {
            std::uint32_t instanceId = 0;
            std::uint32_t partyId = 0;
            yuno::net::packets::InstanceRuntimeState state = yuno::net::packets::InstanceRuntimeState::Active;
            std::uint32_t enemyEntityId = 0;
            std::uint32_t enemyHp = 0;
            std::uint32_t enemyMaxHp = 0;
            std::vector<ParticipantRuntime> participants;
        };

        struct EnterResult final
        {
            yuno::net::packets::InstanceResultCode code = yuno::net::packets::InstanceResultCode::None;
            const Instance* instance = nullptr;
        };

        struct CombatEventRecord final
        {
            yuno::net::packets::CombatEventType type = yuno::net::packets::CombatEventType::Damage;
            std::uint32_t sequence = 0;
            std::uint32_t sourceEntityId = 0;
            std::uint32_t targetEntityId = 0;
            std::uint32_t skillId = 0;
            std::uint32_t amount = 0;
            std::uint32_t targetHp = 0;
            std::uint32_t targetMaxHp = 0;
            std::uint32_t stateFlags = 0;
        };

        struct CastResult final
        {
            yuno::net::packets::InstanceResultCode code = yuno::net::packets::InstanceResultCode::None;
            const Instance* instance = nullptr;
            std::vector<CombatEventRecord> events;
            bool resolved = false;
            bool success = false;
        };

        EnterResult EnterPartyInstance(std::uint32_t partyId, const std::vector<ParticipantSeed>& participants, std::uint32_t enemyEntityId);
        CastResult CastSkill(std::uint64_t sessionId, std::uint32_t casterEntityId, std::uint32_t targetEntityId, std::uint32_t skillId);
        const Instance* FindInstanceBySession(std::uint64_t sessionId) const;
        const Instance* FindInstanceById(std::uint32_t instanceId) const;
        void RemoveInstance(std::uint32_t instanceId);
        void RemoveDisconnected(std::uint64_t sessionId);

    private:
        std::unordered_map<std::uint32_t, Instance> m_instances;
        std::unordered_map<std::uint64_t, std::uint32_t> m_sessionToInstance;
        std::uint32_t m_nextInstanceId = 1;
        std::uint32_t m_nextCombatEventSequence = 1;
    };
}
