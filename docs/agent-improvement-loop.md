# Agent Improvement Loop

Purpose:
- Reduce repeated mistakes and unnecessary cost during Codex Agent tasks.
- Keep a persistent learning record that the user reads and the Agent updates.

## Ownership Model
- User permission model:
  - User is read-only for this file during normal operation.
  - Codex Agent is the writer and must update this file after each meaningful task.

## Read-Then-Write Rule
Before starting a new task, Agent must:
1. Read `C:\Project\MORPG_Network_Sample\docs\agent-active-rules.md` first.
2. Read this history file only when needed:
   - repeated failure
   - unclear root cause
   - conflict between current rules and observed behavior
3. If history is needed, read only the latest 3 entries.

After finishing a task, Agent must:
1. Append one new entry (do not rewrite history).
2. Record mistakes, cost signals, and prevention updates.
3. Update `agent-active-rules.md` only if a rule should be added/removed/changed.

## Non-Negotiable Constraints
- Never delete old entries.
- Never edit past entries except typo fixes.
- Prefer append-only updates.
- Keep entries short and specific.
- Include evidence (file path, command, or symptom) for each mistake claim.

## Cost Control Heuristics
- Minimize avoidable retries:
  - Avoid running full builds before local scope checks.
  - Prefer targeted checks, then full verification.
- Minimize broad edits:
  - Change only files required by current task scope.
- Minimize duplicate analysis:
  - Reuse findings from recent entries when still relevant.

## Mistake Taxonomy
Use one or more labels per issue:
- `REQ_MISS`: requirement misunderstood or omitted.
- `FORMAT_MISS`: output/file format mismatch.
- `PATH_MISS`: wrong module/path placement.
- `COMPAT_MISS`: compatibility impact missed.
- `TEST_MISS`: missing or weak verification.
- `COST_WASTE`: unnecessary command/build/test cycles.
- `COMM_MISS`: missing explanation of alternatives/tradeoffs.

## Entry Template (Append One Block Per Task)
```text
## Entry: <YYYY-MM-DD HH:mm KST> | Task: <short title>
### Summary
- Goal:
- Outcome:

### What Went Well
- 

### Mistakes
- Label: <REQ_MISS|FORMAT_MISS|PATH_MISS|COMPAT_MISS|TEST_MISS|COST_WASTE|COMM_MISS>
  Evidence:
  Root cause:
  Fix applied:

### Cost Signals
- Commands run:
- Build runs:
- Test runs:
- Avoidable retries:

### Prevention Rules (Next Tasks)
- Keep:
- Add:
- Remove:

### Confidence
- Delivery confidence (0-100):
- Verification depth: <low|medium|high>
```

## Bootstrap Entry
## Entry: 2026-04-08 00:00 KST | Task: Bootstrap Improvement Loop
### Summary
- Goal: Create persistent self-improvement process for Codex Agent.
- Outcome: Document created and integrated for future append-only learning.

### What Went Well
- Defined a repeatable template with mistake taxonomy and cost controls.

### Mistakes
- Label: COMM_MISS
  Evidence: Earlier tasks had inconsistent detail in rationale sections.
  Root cause: No single enforced reflection format.
  Fix applied: Added mandatory entry template and read-then-write rule.

### Cost Signals
- Commands run: 0 (bootstrap template only)
- Build runs: 0
- Test runs: 0
- Avoidable retries: 0

### Prevention Rules (Next Tasks)
- Keep: Always include alternatives and tradeoff justification in final reports.
- Add: Read latest 3 entries before task execution.
- Remove: None

### Confidence
- Delivery confidence (0-100): 92
- Verification depth: medium

## Entry: 2026-04-08 13:00 KST | Task: Add Ping RTT Echo Handling
### Summary
- Goal: Implement Ping request/response path to measure client-server delay.
- Outcome: Reused existing C2S_Ping/S2C_Pong protocol, added server immediate echo and client RTT log.

### What Went Well
- Reused existing packet IDs/files to avoid unnecessary protocol expansion cost.
- Verified impacted targets with focused builds (`YunoNetProtocol`, `YunoServer`, `YunoGame`).

### Mistakes
- Label: COST_WASTE
  Evidence: Initial `rg` command failed due execution permission in this environment.
  Root cause: Assumed `rg` availability from prior runs.
  Fix applied: Switched immediately to PowerShell-native file search.

### Cost Signals
- Commands run: 14
- Build runs: 3 (targeted)
- Test runs: 0
- Avoidable retries: 1

### Prevention Rules (Next Tasks)
- Keep: Prefer targeted build scope first.
- Add: If `rg` fails in sandbox, switch to `Get-ChildItem` + `Select-String` without retry loops.
- Remove: None

### Confidence
- Delivery confidence (0-100): 90
- Verification depth: medium

## Entry: 2026-04-08 13:25 KST | Task: Add Protocol Test Automation Policy
### Summary
- Goal: Enforce automatic protocol test script creation, execution log output, and analysis report output.
- Outcome: Added policy links in AGENTS.md, created protocol test operations guide, updated user template, and initialized Result directories.

### What Went Well
- Converted policy into explicit file paths and naming conventions to reduce ambiguity.
- Added fallback policy for blocked runtime environments.

### Mistakes
- Label: COST_WASTE
  Evidence: Initial parallel directory/file creation attempted before parent path was fully available.
  Root cause: Race condition in parallel setup commands.
  Fix applied: Re-ran creation in one sequential command.

### Cost Signals
- Commands run: 10
- Build runs: 0
- Test runs: 0
- Avoidable retries: 1

### Prevention Rules (Next Tasks)
- Keep: For path setup, use sequential command when parent/child directory dependency exists.
- Add: When policy requires artifacts, pre-create `Result\Log` and `Result\Report` before test execution.
- Remove: None

### Confidence
- Delivery confidence (0-100): 93
- Verification depth: medium

## Entry: 2026-04-08 17:46 KST | Task: Execute Ping Protocol Automated Test
### Summary
- Goal: Run Ping/Pong runtime verification and generate required log/report artifacts.
- Outcome: Added `scripts/test_ping.ps1`, executed test, and produced PASS artifacts under `Result/Log` and `Result/Report`.

### What Went Well
- Runtime packet validation confirmed expected `S2C_Pong` type and `reqTime` echo match.
- RTT metric was captured directly from socket round-trip.

### Mistakes
- Label: TEST_MISS
  Evidence: First execution failed due same-path stdout/stderr redirection in `Start-Process`.
  Root cause: Platform-specific PowerShell process redirection constraint.
  Fix applied: Removed conflicting redirection and simplified process start.
- Label: TEST_MISS
  Evidence: Second execution failed due UInt32 conversion overflow for timestamp.
  Root cause: Unsafe cast from Unix millisecond value to UInt32.
  Fix applied: Applied modulo conversion before UInt32 cast.

### Cost Signals
- Commands run: 8
- Build runs: 0
- Test runs: 4
- Avoidable retries: 2

### Prevention Rules (Next Tasks)
- Keep: Validate process launch options in script before first run.
- Add: For 32-bit protocol timestamps, normalize with modulo before cast.
- Remove: None

### Confidence
- Delivery confidence (0-100): 94
- Verification depth: high

## Entry: 2026-04-08 17:52 KST | Task: Set Report Language Policy
### Summary
- Goal: Keep runtime logs unchanged and enforce Korean as default report language policy.
- Outcome: Updated AGENTS.md and protocol test operations policy to require Korean reports by default while preserving raw logs.

### What Went Well
- Policy was added at both global rule level and protocol test operations level.
- `scripts/test_ping.ps1` was stabilized after encoding-related parse issues.

### Mistakes
- Label: FORMAT_MISS
  Evidence: Temporary script parse error due corrupted multi-byte text block.
  Root cause: Unsafe inline edits under mixed encoding display.
  Fix applied: Rewrote script with stable ASCII-safe structure.

### Cost Signals
- Commands run: 8
- Build runs: 0
- Test runs: 1
- Avoidable retries: 1

### Prevention Rules (Next Tasks)
- Keep: For script stability, apply full-file rewrite if encoding corruption appears.
- Add: Keep policy text in docs; keep runtime scripts parse-safe first, then localize output with controlled encoding.
- Remove: None

### Confidence
- Delivery confidence (0-100): 91
- Verification depth: medium

## Entry: 2026-04-08 17:55 KST | Task: Run Ping Test And Write Korean Report
### Summary
- Goal: Execute Ping/Pong runtime test and produce a Korean report while keeping raw log format.
- Outcome: Test passed and report file was rewritten in Korean without changing log content.

### What Went Well
- End-to-end test evidence captured with packet type, reqTime echo, and RTT value.
- Report language policy (Korean) applied directly to the generated artifact.

### Mistakes
- Label: FORMAT_MISS
  Evidence: Auto-generated report from script remained English by default.
  Root cause: Script/report template localization was not yet enforced end-to-end.
  Fix applied: Rewrote the generated report file in Korean for this run.

### Cost Signals
- Commands run: 4
- Build runs: 0
- Test runs: 1
- Avoidable retries: 0

### Prevention Rules (Next Tasks)
- Keep: Preserve raw logs as-is.
- Add: Ensure final report artifact language is Korean before closing protocol test task.
- Remove: None

### Confidence
- Delivery confidence (0-100): 95
- Verification depth: high

## Entry: 2026-04-09 20:49 KST | Task: Transport Hardening and Server Packet Routing Stabilization
- Goal: Improve connection lifecycle robustness, packet flood defense, and server packet handler maintainability.
- Outcome: Added inbound packet-rate budgeting in TcpSession, added TcpServer session cap/options and targeted session disconnect API, and migrated YunoServerNetwork packet routing to PacketDispatcher with per-session packet budget enforcement.
- Verification: Created and ran `scripts\test_transport_hardening.ps1`; PASS with log `Result\Log\20260409_204805_transport_hardening.log` and report `Result\Report\20260409_204805_transport_hardening_report.md`.
- Learning: Explicit transport/server boundary rate-limits plus dispatcher-based routing reduced ad-hoc branching and made future protocol extension safer.

## Entry: 2026-08-23 KST | Task: Reorganize Portfolio README
### Summary
- Goal: Explain the repository's relationship to YunoEngine and make the C++ network, asynchronous programming, and MySQL evidence easy to review.
- Outcome: Replaced the placeholder README with architecture, module responsibilities, runtime flows, code-review entry points, build/test instructions, design tradeoffs, and known limitations.

### What Went Well
- Every relative source/document link in the README was checked against the repository and resolved successfully.
- Technical claims were tied to concrete source paths and existing test reports.
- The document clearly distinguishes this technical sample from a separate completed game.

### Mistakes
- Label: FORMAT_MISS
  Evidence: The first full-file patch used delete and add operations for the same path and was rejected.
  Root cause: The patch format did not support two operations targeting one file in a single request.
  Fix applied: Replaced the existing content with one update operation and re-ran validation.

### Cost Signals
- Build runs: 0
- Test runs: 0
- Avoidable retries: 1

### Prevention Rules (Next Tasks)
- Keep: For a full rewrite of an existing short document, use one update operation instead of delete plus add.
- Add: Validate every relative link in portfolio documentation before delivery.
- Remove: None

### Confidence
- Delivery confidence (0-100): 95
- Verification depth: medium (documentation-only change; link and diff validation)

## Entry: 2026-08-23 KST | Task: Remove Database Credential and MySQL Path Fallbacks
### Summary
- Goal: Remove committed database credential defaults and developer-machine-specific MySQL paths.
- Outcome: DB user/password are now required through environment variables, and both server projects use only `MYSQL_DIR` for MySQL headers, libraries, and runtime DLL lookup.

### What Went Well
- Added fail-fast runtime messages when `YUNO_DB_USER` or `YUNO_DB_PASS` is missing.
- Added MSBuild validation for an unset or invalid `MYSQL_DIR`.
- Confirmed both project files remain valid XML and no credential fallback or personal MySQL path remains in the current tree.
- Confirmed the missing-`MYSQL_DIR` build path emits the intended actionable error.

### Mistakes
- Label: ENV_BLOCKER
  Evidence: The full Debug x64 build stopped at `YunoLoginServer` because Boost.Asio headers were unavailable.
  Root cause: The local checkout does not contain `vcpkg_installed`, and the required Boost dependencies were not restored.
  Fix applied: Recorded the environment blocker and retained the successful XML, static, and MSBuild negative-path checks.

### Cost Signals
- Build runs: 1
- Test runs: 1 targeted MSBuild configuration validation
- Avoidable retries: 0

### Prevention Rules (Next Tasks)
- Keep: Validate required external SDK properties before invoking a full build.
- Add: Restore the vcpkg manifest dependencies before the next runtime verification.
- Remove: None

### Confidence
- Delivery confidence (0-100): 92
- Verification depth: medium (configuration and static checks passed; full build blocked by missing Boost dependency)

## Entry: 2026-08-23 KST | Task: Purge Exposed Database Credentials From Git History
### Summary
- Goal: Remove the exposed DB user/password values from reachable branch history without discarding the existing development timeline.
- Outcome: Rewrote all 68 mirrored commits, atomically force-pushed all three branches, and synchronized the local `Dev` branch to the rewritten lineage.

### What Went Well
- Preserved per-branch commit counts: backup 8, Dev 22, and network-refactoring 66.
- Preserved the Dev commit author/date/message sequence and the latest Dev file tree.
- Verified the two exposed values have zero history hits across all remote branch refs after a fresh mirror clone.
- Used one atomic force-push so all three branch updates succeeded or failed together.
- Removed the temporary mirrors containing the original history after remote verification.

### Mistakes
- Label: TOOL_MISS
  Evidence: Two force-push dry-run attempts failed before the successful preflight.
  Root cause: The first PowerShell argument array was constructed incorrectly, and the second used a mirror-configured remote that rejects explicit refspecs.
  Fix applied: Added a separate non-mirror remote and repeated the atomic dry run successfully before the real push.

### Cost Signals
- History rewrite runs: 1
- Atomic force-push runs: 1
- Avoidable preflight retries: 2

### Prevention Rules (Next Tasks)
- Keep: Rewrite sensitive history only in disposable mirror clones and verify commit counts, metadata, tip trees, and secret scans before force-pushing.
- Add: Use a separate non-mirror push remote when sending explicit sanitized branch refspecs from a mirror clone.
- Add: Request GitHub removal of affected pull-request refs and cached views after sensitive-data history rewrites.
- Remove: None

### Confidence
- Delivery confidence (0-100): 90
- Verification depth: high for branch history; four GitHub-managed pull-request refs still require server-side purge

## Entry: 2026-08-23 KST | Task: Build Dev and Audit Implemented Feature Scope
### Summary
- Goal: Build the Dev branch with its declared dependencies and identify the actually implemented MORPG feature boundary.
- Outcome: Built DirectXTK, both protocol libraries, the transport library, `YunoLoginServer`, and `YunoServer` in Debug x64; the final `YunoGame` link is blocked only by the absent proprietary FMOD Studio API 2.03.06 libraries.

### What Went Well
- Restored the vcpkg manifest dependencies under an ASCII temporary path to avoid the non-ASCII Windows profile compiler failure.
- Added the missing Argon2 link/runtime deployment, declared Assimp in the manifest, aligned Assimp/zlib Debug and Release paths, and made the official build script build DirectXTK first.
- Verified the vcpkg install plan is stable and the vcpkg submodule is clean after the temporary archive-extraction workaround was reverted.
- Confirmed from handlers rather than packet IDs alone that auth, token world entry, authoritative movement, snapshot ACK, prediction/interpolation, and bot load mode are implemented.

### Mistakes
- Label: DEPENDENCY_ASSUMPTION
  Evidence: The first full build attempts discovered Boost, MySQL, Argon2, Assimp, DirectXTK, and FMOD requirements one at a time.
  Root cause: The build script did not validate or build all external prerequisites before entering the solution target loop.
  Fix applied: Added DirectXTK prebuild handling, declared/linkable vcpkg dependencies, and recorded FMOD as the remaining external SDK blocker.
- Label: TOOL_WORKAROUND_COST
  Evidence: Manifest normalization rebuilt PhysX twice after a temporary vcpkg extraction-script change.
  Root cause: CMake 3.31 rejected the valid pugixml archive while Windows tar extracted it successfully, and the temporary core-script change affected package ABI calculation.
  Fix applied: Completed one normal-ABI manifest install, reverted the script, and revalidated a stable dry-run plan.

### Cost Signals
- Full build runs: 6
- Dependency install/normalization runs: 7
- Runtime test runs: 0 (servers require a configured MySQL instance; client link requires FMOD Studio API 2.03.06)
- Avoidable retries: 2

### Prevention Rules (Next Tasks)
- Keep: Separate compilable handlers from enum/schema-only placeholders when reporting feature scope.
- Add: Add a fail-fast FMOD SDK path/version check or a documented audio-disabled sample configuration before the next clean-machine build.
- Add: Validate all generated `.lib` and required runtime `.dll` files before the full solution target loop.
- Remove: None

### Confidence
- Delivery confidence (0-100): 93
- Verification depth: high for network/server compilation and static feature tracing; client runtime remains blocked by FMOD Studio API 2.03.06

## Entry: 2026-08-23 KST | Task: Add an Audio-Optional Portfolio Build
### Summary
- Goal: Allow the public MORPG network sample to build without the proprietary FMOD Studio SDK while preserving the full-engine audio implementation.
- Outcome: Added the `YunoEnableFMOD` build option with a default value of `0`, excluded FMOD source units when disabled, and completed the full Dev Debug x64 build including `YunoGame.exe`.

### What Went Well
- Kept FMOD code intact and limited the default-off behavior to compile/link configuration and three guarded engine lifecycle calls.
- Added a fail-fast dependency check for explicit `-YunoEnableFMOD 1` builds.
- Verified `YunoGame.exe` has no direct FMOD DLL dependency in the default portfolio configuration.
- Documented both the default public build and the opt-in full-engine FMOD command.

### Mistakes
- Label: PATCH_CONTEXT
  Evidence: The first combined patch failed before applying any changes.
  Root cause: The expected context around `fmodPCH.h` did not include the file's blank-line layout.
  Fix applied: Split the work into small file-specific patches and validated the resulting XML and PowerShell syntax.

### Cost Signals
- Full build runs: 1
- Targeted negative-path tests: 1
- Avoidable retries: 1

### Prevention Rules (Next Tasks)
- Keep: Default proprietary integrations off in public portfolio configurations while retaining an explicit opt-in path.
- Add: Inspect exact small-file context before composing a multi-file patch.
- Remove: None

### Confidence
- Delivery confidence (0-100): 98
- Verification depth: high (full Debug x64 build, binary existence, import-table inspection, and FMOD guard negative-path test)
