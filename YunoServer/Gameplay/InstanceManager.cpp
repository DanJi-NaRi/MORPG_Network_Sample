#include "InstanceManager.h"

#include <algorithm>

namespace yuno::server
{
    namespace
    {
        constexpr std::uint32_t kEnemyMaxHp = 180;
        constexpr std::uint32_t kPlayerMaxHp = 100;
        constexpr std::uint32_t kSkillDamage = 40;
        constexpr std::uint32_t kEnemyRetaliationDamage = 15;
    }

    InstanceManager::EnterResult InstanceManager::EnterPartyInstance(
        std::uint32_t partyId,
        const std::vector<ParticipantSeed>& participants,
        std::uint32_t enemyEntityId)
    {
        if (participants.size() != 3)
            return { yuno::net::packets::InstanceResultCode::PartyNotReady, nullptr };

        for (const ParticipantSeed& participant : participants)
        {
            if (m_sessionToInstance.find(participant.sessionId) != m_sessionToInstance.end())
                return { yuno::net::packets::InstanceResultCode::AlreadyInInstance, nullptr };
        }

        Instance instance{};
        instance.instanceId = m_nextInstanceId++;
        instance.partyId = partyId;
        instance.state = yuno::net::packets::InstanceRuntimeState::Active;
        instance.enemyEntityId = enemyEntityId;
        instance.enemyHp = kEnemyMaxHp;
        instance.enemyMaxHp = kEnemyMaxHp;
        instance.participants.reserve(participants.size());
        for (const ParticipantSeed& participant : participants)
        {
            ParticipantRuntime runtime{};
            runtime.sessionId = participant.sessionId;
            runtime.entityId = participant.entityId;
            runtime.hp = kPlayerMaxHp;
            runtime.maxHp = kPlayerMaxHp;
            runtime.alive = true;
            instance.participants.push_back(runtime);
        }

        const std::uint32_t instanceId = instance.instanceId;
        auto [it, inserted] = m_instances.emplace(instanceId, std::move(instance));
        if (!inserted)
            return { yuno::net::packets::InstanceResultCode::InvalidState, nullptr };

        for (const auto& participant : it->second.participants)
        {
            m_sessionToInstance[participant.sessionId] = instanceId;
        }

        return { yuno::net::packets::InstanceResultCode::None, &it->second };
    }

    InstanceManager::CastResult InstanceManager::CastSkill(
        std::uint64_t sessionId,
        std::uint32_t casterEntityId,
        std::uint32_t targetEntityId,
        std::uint32_t skillId)
    {
        const auto mapIt = m_sessionToInstance.find(sessionId);
        if (mapIt == m_sessionToInstance.end())
            return { yuno::net::packets::InstanceResultCode::NotParticipant, nullptr, {}, false, false };

        auto instanceIt = m_instances.find(mapIt->second);
        if (instanceIt == m_instances.end())
            return { yuno::net::packets::InstanceResultCode::InstanceNotFound, nullptr, {}, false, false };

        Instance& instance = instanceIt->second;
        if (instance.state != yuno::net::packets::InstanceRuntimeState::Active)
            return { yuno::net::packets::InstanceResultCode::InvalidState, &instance, {}, false, false };

        auto participantIt = std::find_if(
            instance.participants.begin(),
            instance.participants.end(),
            [sessionId, casterEntityId](const ParticipantRuntime& participant)
            {
                return participant.sessionId == sessionId && participant.entityId == casterEntityId;
            });
        if (participantIt == instance.participants.end() || !participantIt->alive)
            return { yuno::net::packets::InstanceResultCode::NotParticipant, &instance, {}, false, false };

        if (targetEntityId != instance.enemyEntityId || skillId == 0)
            return { yuno::net::packets::InstanceResultCode::InvalidState, &instance, {}, false, false };

        CastResult result{};
        result.code = yuno::net::packets::InstanceResultCode::None;

        instance.enemyHp = (instance.enemyHp > kSkillDamage) ? (instance.enemyHp - kSkillDamage) : 0;
        result.events.push_back({
            yuno::net::packets::CombatEventType::Damage,
            m_nextCombatEventSequence++,
            participantIt->entityId,
            instance.enemyEntityId,
            skillId,
            kSkillDamage,
            instance.enemyHp,
            instance.enemyMaxHp,
            (instance.enemyHp == 0) ? 1u : 0u,
        });

        if (instance.enemyHp == 0)
        {
            instance.state = yuno::net::packets::InstanceRuntimeState::Cleared;
            result.events.push_back({
                yuno::net::packets::CombatEventType::EncounterCleared,
                m_nextCombatEventSequence++,
                participantIt->entityId,
                instance.enemyEntityId,
                skillId,
                0,
                instance.enemyHp,
                instance.enemyMaxHp,
                1u,
            });
            result.instance = &instance;
            result.resolved = true;
            result.success = true;
            return result;
        }

        participantIt->hp = (participantIt->hp > kEnemyRetaliationDamage) ? (participantIt->hp - kEnemyRetaliationDamage) : 0;
        result.events.push_back({
            yuno::net::packets::CombatEventType::Damage,
            m_nextCombatEventSequence++,
            instance.enemyEntityId,
            participantIt->entityId,
            skillId,
            kEnemyRetaliationDamage,
            participantIt->hp,
            participantIt->maxHp,
            (participantIt->hp == 0) ? 1u : 0u,
        });

        if (participantIt->hp == 0)
        {
            participantIt->alive = false;
            result.events.push_back({
                yuno::net::packets::CombatEventType::Death,
                m_nextCombatEventSequence++,
                instance.enemyEntityId,
                participantIt->entityId,
                skillId,
                0,
                participantIt->hp,
                participantIt->maxHp,
                1u,
            });

            const bool anyAlive = std::any_of(
                instance.participants.begin(),
                instance.participants.end(),
                [](const ParticipantRuntime& participant)
                {
                    return participant.alive;
                });
            if (!anyAlive)
            {
                instance.state = yuno::net::packets::InstanceRuntimeState::Failed;
                result.events.push_back({
                    yuno::net::packets::CombatEventType::EncounterFailed,
                    m_nextCombatEventSequence++,
                    instance.enemyEntityId,
                    participantIt->entityId,
                    skillId,
                    0,
                    instance.enemyHp,
                    instance.enemyMaxHp,
                    1u,
                });
                result.resolved = true;
                result.success = false;
            }
        }

        result.instance = &instance;
        return result;
    }

    const InstanceManager::Instance* InstanceManager::FindInstanceBySession(std::uint64_t sessionId) const
    {
        const auto mapIt = m_sessionToInstance.find(sessionId);
        if (mapIt == m_sessionToInstance.end())
            return nullptr;
        return FindInstanceById(mapIt->second);
    }

    const InstanceManager::Instance* InstanceManager::FindInstanceById(std::uint32_t instanceId) const
    {
        const auto it = m_instances.find(instanceId);
        return (it != m_instances.end()) ? &it->second : nullptr;
    }

    void InstanceManager::RemoveInstance(std::uint32_t instanceId)
    {
        auto it = m_instances.find(instanceId);
        if (it == m_instances.end())
            return;

        for (const auto& participant : it->second.participants)
        {
            m_sessionToInstance.erase(participant.sessionId);
        }
        m_instances.erase(it);
    }

    void InstanceManager::RemoveDisconnected(std::uint64_t sessionId)
    {
        const auto mapIt = m_sessionToInstance.find(sessionId);
        if (mapIt == m_sessionToInstance.end())
            return;

        auto instanceIt = m_instances.find(mapIt->second);
        if (instanceIt == m_instances.end())
        {
            m_sessionToInstance.erase(mapIt);
            return;
        }

        Instance& instance = instanceIt->second;
        instance.participants.erase(
            std::remove_if(
                instance.participants.begin(),
                instance.participants.end(),
                [sessionId](const ParticipantRuntime& participant)
                {
                    return participant.sessionId == sessionId;
                }),
            instance.participants.end());
        m_sessionToInstance.erase(mapIt);

        if (instance.participants.empty())
            m_instances.erase(instanceIt);
    }
}
