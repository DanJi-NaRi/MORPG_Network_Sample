# Protocol Test Report: party_instance_combat_inventory

## Objective
- Verify the phased MORPG demo path for party formation, dungeon instance entry, first combat path, and persistence prerequisites.

## Expected Behavior
- Phase 0 ownership is unambiguous across packet IDs, packet serde, and server registration.
- Packet and runtime seams exist for party, instance, combat, and inventory persistence.
- Baseline login -> town world flow is available before party/instance scenarios are attempted.
- The script emits timestamped log/report artifacts under Result/Log and Result/Report.

## Executed Commands
- Phase 0 ownership scan against PacketType.h, YunoGameProtocol, and YunoServerNetwork.cpp
- powershell.exe -ExecutionPolicy Bypass -File C:\Project\MORPG_Network_Sample\.omx\team\execute-omx-plans-prd-morpg-de\worktrees\worker-2\scripts\smoke_world_enter.ps1 -Configuration Debug -Platform x64 -LoginPort 7000 -WorldPort 9000 -WarmupSeconds 3

## Result
- FAIL

## Key Log Evidence
- Phase 0 ownership files present: Packet IDs, packet serde, and runtime registration files were located for manual ownership mapping.
- Phase 0 ownership map => Packet IDs: YunoNetProtocol/Public/Net/PacketType.h; MORPG packet structs + serde: YunoGameProtocol/Public|Private/Net/MORPGPackets; Server packet registration/orchestration: YunoServer/ServerNetwork/YunoServerNetwork.cpp
- Packet type markers declared: Current world/combat packet IDs are declared in PacketType.h.
- Inventory schema baseline present: init_yuno_auth.sql already defines inventory_items for persistence verification.
- Required verification scripts present: build_and_test, smoke_world_enter, and party-instance-combat-inventory scripts are present.
- Prerequisite login-to-world smoke: smoke_world_enter.ps1 passed against built binaries.

## Failure Analysis (if any)
- Root cause: One or more required seams or prerequisite binaries are missing.
- Scope impact: Skill cast server registration present: No C2S_SkillCast registration/handling found in YunoServerNetwork.cpp. | Party protocol artifacts present: No party protocol packet/header files found under YunoNetProtocol or YunoGameProtocol. | Instance protocol artifacts present: No instance/dungeon protocol packet/header files found. | Party runtime seam present: No party runtime files found under YunoServer or protocol projects. | Instance runtime seam present: No instance/dungeon runtime files found under YunoServer or protocol projects. | Combat runtime seam present: No validated combat runtime seam with skill-cast handling found. | Inventory/persistence runtime seam present: No inventory/persistence runtime seam found beyond the SQL schema baseline.

## Next Actions
- Wire the first combat path through YunoServerNetwork orchestration or an extracted manager before combat verification can pass.
- Define party packet contracts before integration Scenario B can be automated.
- Add instance enter/result/state packet contracts before dungeon transfer verification can run.
- Extract or add a bounded party runtime seam before three-player party verification can pass.
- Add an instance manager/runtime before world-to-dungeon transfer can be tested.
- Implement the first combat path and keep YunoServerNetwork as orchestration glue before combat verification can pass.
- Add repository/service code for gameplay persistence before Scenario D can pass.
