#pragma once

#include <cstdint>

namespace yuno::net
{
    // Packet id range policy
    // - 1..127   : client to server
    // - 128..239 : server to client
    // - 240..255 : system / error
    enum class PacketType : std::uint8_t
    {
        Invalid = 0,

        // C2S (legacy game-specific)
        C2S_MatchEnter = 1,
        C2S_MatchLeave = 2,
        C2S_ReadySet = 3,
        C2S_SubmitWeapon = 4,
        C2S_SubmitCard = 5,
        C2S_Ping = 6,
        C2S_ReadyTurn = 7,
        C2S_SelectCard = 8,
        C2S_Emote = 9,
        C2S_RoundStartReadyOK = 10,
        C2S_Surrender = 11,

        // C2S (MORPG realtime)
        C2S_AuthHello = 12,
        C2S_EnterWorld = 13,
        C2S_LeaveWorld = 14,
        C2S_MoveInput = 15,
        C2S_SkillCast = 16,
        C2S_Interact = 17,
        C2S_AckSnapshot = 18,
        C2S_AuthRegister = 19,
        C2S_AuthLogout = 20,
        C2S_PartyCreate = 21,
        C2S_PartyJoin = 22,
        C2S_PartyLeave = 23,
        C2S_InstanceEnter = 24,
        C2S_PartyList = 25,
        C2S_PartyInvitePlayer = 26,
        C2S_PartyRespondJoinRequest = 27,
        C2S_PartyRespondInvite = 28,
        C2S_PartyBrowsePlayers = 29,

        // S2C (legacy game-specific)
        S2C_EnterOK = 128,
        S2C_ReadyState = 129,
        S2C_CountDown = 130,
        S2C_RoundStart = 131,
        S2C_BattleResult = 133,
        S2C_StartCardList = 134,
        S2C_DrawCandidates = 135,
        S2C_StartTurn = 136,
        S2C_EndGame = 137,
        S2C_Emote = 138,
        S2C_ObstacleResult = 139,
        S2C_Pong = 140,

        // S2C (MORPG realtime)
        S2C_AuthResult = 141,
        S2C_EnterWorldResult = 142,
        S2C_SpawnEntity = 143,
        S2C_DespawnEntity = 144,
        S2C_WorldSnapshot = 145,
        S2C_ServerEvent = 146,
        S2C_PartyState = 147,
        S2C_InstanceState = 148,
        S2C_CombatEvent = 149,
        S2C_InventoryState = 150,
        S2C_InstanceResult = 151,
        S2C_PartyList = 152,
        S2C_PartySocialState = 153,

        // System / Error
        S2C_Error = 240,
        S2C_EndGame_Disconnect = 241,
        S2C_Error_EnterDenied = 255
    };

    inline constexpr bool IsValidPacketType(PacketType t)
    {
        return t != PacketType::Invalid;
    }

    inline constexpr bool IsC2S(PacketType t)
    {
        return (static_cast<std::uint8_t>(t) & 0x80u) == 0u
            && t != PacketType::Invalid;
    }

    inline constexpr bool IsS2C(PacketType t)
    {
        const auto v = static_cast<std::uint8_t>(t);
        return (v & 0x80u) != 0u && (v & 0xF0u) != 0xF0u;
    }

    inline constexpr bool IsSystem(PacketType t)
    {
        return (static_cast<std::uint8_t>(t) & 0xF0u) == 0xF0u;
    }
}

namespace yuno::net::packets
{
    enum class ErrorCode : std::uint16_t
    {
        None = 0,
        EnterDenied = 1,
        AuthFailed = 2,
        InvalidState = 3,
    };

    enum class EnterDeniedReason : std::uint8_t
    {
        None = 0,
        RoomFull = 1,
        AlreadyInMatch = 2,
    };
}
