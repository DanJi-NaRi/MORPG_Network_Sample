# MORPG Demo Server Phase 0 Ownership Map

## Objective
Lock the brownfield packet/runtime ownership before party, instance, combat, and persistence edits so `YunoServerNetwork` stays orchestration glue.

## Confirmed physical ownership

### Packet ID registry
- `YunoNetProtocol/Public/Net/PacketType.h`
- Owns the canonical opcode map for both login/system and MORPG realtime packets.

### Login / system packets
- Public: `YunoNetProtocol/Public/Net/C2SPackets/*`, `YunoNetProtocol/Public/Net/S2CPackets/*`, `YunoNetProtocol/Public/Net/ErrorPackets/*`
- Private: matching `YunoNetProtocol/Private/Net/*`
- Current examples: `C2S_AuthHello`, `C2S_AuthRegister`, `C2S_AuthLogout`, `S2C_AuthResult`, `S2C_Pong`.

### MORPG realtime packets
- Public: `YunoGameProtocol/Public/Net/MORPGPackets/*`
- Private: `YunoGameProtocol/Private/Net/MORPGPackets/*`
- Current ownership already lives here for:
  - `C2S_EnterWorld`
  - `C2S_MoveInput`
  - `C2S_AckSnapshot`
  - `S2C_SpawnEntity`
  - `S2C_DespawnEntity`
  - `S2C_WorldSnapshot`

### Server-side orchestration glue
- `YunoServer/ServerNetwork/YunoServerNetwork.h`
- `YunoServer/ServerNetwork/YunoServerNetwork.cpp`
- Current responsibilities already present:
  - packet dispatch registration
  - auth token validation / revoke
  - enter-world gating
  - movement ingestion
  - snapshot broadcast
  - per-session packet budget enforcement

### Login/auth persistence seam
- `YunoLoginServer/LoginData/MySqlAuthRepository.*`
- Current ownership:
  - credential validation
  - account create
  - login token issue/revoke
  - password/token hash migration

### Gameplay persistence seam
- `YunoServer/Gameplay/MySqlGameplayRepository.h`
- `YunoServer/Gameplay/MySqlGameplayRepository.cpp`
- Current ownership:
  - ensure demo character exists for authenticated user
  - load inventory/gold for enter-world sync
  - grant and persist dungeon reward/inventory result

### Party / instance runtime seams
- `YunoServer/Gameplay/PartyManager.h`
- `YunoServer/Gameplay/PartyManager.cpp`
- `YunoServer/Gameplay/InstanceManager.h`
- `YunoServer/Gameplay/InstanceManager.cpp`
- Current ownership:
  - party creation/join/leave/disband state
  - instance admission and participant membership
  - encounter/combat state tracked outside `YunoServerNetwork`

### Demo/runtime harness
- `YunoServer/main/main.cpp`
- Current ownership:
  - standalone server entrypoint
  - `--bot` town/world load harness

### Verification / evidence scripts
- `scripts/build_and_test.ps1`
- `scripts/smoke_world_enter.ps1`
- `scripts/TestProcessHelpers.ps1`
- `scripts/world_enter_probe.ps1`
- Required new artifact lane: `scripts/test_party_instance_combat_inventory.ps1`

## Extraction seams approved for implementation

### Keep in `YunoServerNetwork`
- session lookup
- packet registration
- per-session player runtime cache
- world/instance visibility routing
- glue calls into party / instance / persistence seams

### Extract into bounded gameplay seams
- `PartyManager`
  - create/join/leave party
  - maintain party membership + leader
- `InstanceManager`
  - create 3-player dungeon runtime
  - own encounter state, enemy HP, participant HP/death, clear/fail resolution
- `MySqlGameplayRepository` (server-side gameplay persistence)
  - ensure demo character exists for authenticated user
  - load inventory/gold for enter-world sync
  - persist dungeon reward/inventory result

## Packet matrix for this slice

| Domain | Direction | Packet | Purpose | Physical owner |
|---|---|---|---|---|
| Party | C2S | `C2S_PartyCreate` | create 3-player party | `YunoGameProtocol` |
| Party | C2S | `C2S_PartyJoin` | join by party id | `YunoGameProtocol` |
| Party | C2S | `C2S_PartyLeave` | leave/disband current party | `YunoGameProtocol` |
| Party | S2C | `S2C_PartyState` | authoritative party composition/result | `YunoGameProtocol` |
| Instance | C2S | `C2S_InstanceEnter` | request dungeon entry | `YunoGameProtocol` |
| Instance | S2C | `S2C_InstanceState` | active instance state + participant HP | `YunoGameProtocol` |
| Instance | S2C | `S2C_InstanceResult` | clear/fail result + reward summary | `YunoGameProtocol` |
| Combat | C2S | `C2S_SkillCast` | first vertical-slice attack action | `YunoGameProtocol` |
| Combat | S2C | `S2C_CombatEvent` | damage/death/clear/fail events | `YunoGameProtocol` |
| Inventory | S2C | `S2C_InventoryState` | authoritative inventory/gold sync | `YunoGameProtocol` |

## Merge / ownership guidance for team lanes
1. Land opcode + packet serialization first.
2. Land server runtime seams next (`PartyManager`, `InstanceManager`, persistence repo).
3. Land automation/reporting after the runtime contracts are stable.
4. Keep `YunoServerNetwork` focused on routing and broadcast glue; do not move long-lived gameplay rules back into it.
