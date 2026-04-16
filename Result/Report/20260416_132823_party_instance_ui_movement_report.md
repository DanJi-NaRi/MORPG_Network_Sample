# Protocol Test Report: party_instance_ui_movement

## Objective
- Verify the party create/join -> leader-only instance enter -> keyboard-only movement UI slice and capture required evidence artifacts.

## Expected Behavior
- Party discovery/select is available through UI.
- Invite/accept affordances are visible in UI.
- Only the leader can enter the instance.
- Loading text appears during transition.
- Keyboard-only movement remains active in the instance flow.
- Nickname labels/roster text are available from authoritative party state.

## Executed Commands
- powershell.exe -ExecutionPolicy Bypass -File ./scripts/build_and_test.ps1 -Targets YunoNetProtocol,YunoGameProtocol,YunoLoginServer,YunoServer,YunoGame
- powershell.exe -ExecutionPolicy Bypass -File ./scripts/smoke_world_enter.ps1
- powershell.exe -ExecutionPolicy Bypass -File ./scripts/test_party_instance_ui_movement.ps1 -SkipBuild -SkipSmoke
- grep-based sanity checks for new packet types, handlers, UI labels, and removal of click-move helpers

## Result
- FAIL

## Key Log Evidence
- PacketType.h now includes C2S_PartyList and S2C_PartyList.
- PartyPackets.h/cpp now include party list serialization and party member displayName fields.
- YunoClientNetwork.cpp now handles S2C_PartyState, S2C_PartyList, and S2C_InstanceState.
- GameApp.cpp queues UI-driven party/instance commands and no longer references click-to-move helpers.
- TestScene.cpp now contains UI labels/buttons for Create Party, Search Parties, Join Selected Party, Invite Selected Party, Accept Invite, Leave Party, and Leader Enter Instance.
- YunoServerNetwork.cpp now loads display names and serves authoritative party list/state responses.
- All three required PowerShell invocations failed before script startup with the same WSL vsock runtime error.

## Failure Analysis (if any)
- Root cause: Windows executable launch from this WSL environment is failing before PowerShell scripts can start.
- Scope impact: build, smoke, and protocol verification scripts could not execute, so runtime verification evidence is blocked.
- Concrete blocker: "/mnt/c/Windows/System32/WindowsPowerShell/v1.0/powershell.exe" exits immediately with "UtilBindVsockAnyPort:307: socket failed 1".

## Next Actions
- Run the three required PowerShell commands from a Windows shell or a WSL environment that can launch Windows executables successfully.
- Once the runtime blocker is cleared, inspect generated logs/reports to validate three-client create/list/join/leader-enter behavior.
- If build fails after the environment issue is cleared, fix compiler/runtime issues and rerun the full verification sequence.
