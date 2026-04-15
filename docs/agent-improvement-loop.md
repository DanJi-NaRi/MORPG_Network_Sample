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

## Entry: 2026-04-13 21:02 KST | Task: Daily Dev Branch Worklog Sync
- Goal: Collect `Dev` commits since the last automation run, interpret diffs, and sync the daily worklog page without duplicate records.
- Outcome: Confirmed there were no new `Dev` commits after `2026-04-13T11:29:29.062Z` and created the Notion page `2026-04-13 (월)` with explicit `없음` entries to preserve date continuity.
- Verification: Ran `git -c safe.directory='C:/Users/user/.codex/worktrees/6bf5/MORPG_Network_Sample' log Dev --since='2026-04-13T11:29:29.062Z'` and verified Notion search had no existing `2026-04-13 (월)` page before creation.
- Learning: For this automation, no-commit days should still be recorded explicitly, and Notion page creation should use `parent: { page_id: ... }` because connector validation rejects a plain string parent.

## Entry: 2026-04-14 10:56 KST | Task: Add 95% Confidence Gate To AGENTS
- Goal: Add a strict confidence-gate policy requested by user to `AGENTS.md`.
- Outcome: Added two explicit policy lines under `## Do Not` to block changes before 95% confidence and require additional questions until confidence threshold is reached.
- Verification: Updated `C:\Project\MORPG_Network_Sample\AGENTS.md` and confirmed the new lines are present in the `Do Not` section.
- Learning: User-level operating constraints should be captured in `AGENTS.md` immediately to prevent future policy drift.

## Entry: 2026-04-14 10:59 KST | Task: Rewrite Confidence Gate Lines In English
- Goal: Convert the newly added confidence-gate policy lines in `AGENTS.md` from Korean to English.
- Outcome: Replaced the two `Do Not` policy lines with English wording while keeping policy intent unchanged.
- Verification: Confirmed both English lines in `C:\Project\MORPG_Network_Sample\AGENTS.md` under `## Do Not`.
- Learning: For mixed-language policy files, direct line replacement with UTF-8 write avoids recurring display/encoding confusion.

## Entry: 2026-04-14 11:04 KST | Task: Add Korean Interpretation File And Sync Policy
- Goal: Create `Agents_kr.md` as a Korean interpretation of `AGENTS.md` and enforce synchronization between both files.
- Outcome: Added `## Translation Sync Policy` to `AGENTS.md` and created `C:\Project\MORPG_Network_Sample\Agents_kr.md` with full Korean interpretation, including the same sync policy.
- Verification: Confirmed new sync-policy section at the end of `AGENTS.md` and verified `Agents_kr.md` contains matching policy intent.
- Learning: Explicit sync policy in the source document is more reliable than implicit convention for keeping bilingual policy files aligned.

## Entry: 2026-04-14 11:12 KST | Task: Analyze Harness Documents
- Goal: Identify which documents and scripts act as the current test harness for protocol work and summarize their roles.
- Outcome: Confirmed the harness is organized as `docs/protocol-test-ops.md` plus executable PowerShell scripts under `scripts/`, with `test_<feature>.ps1` producing timestamped logs and Markdown reports in `Result/Log` and `Result/Report`.
- Verification: Read `docs/agent-active-rules.md`, `docs/protocol-test-ops.md`, `docs/protocol-template.md`, `docs/protocol-style-guide.en.md`, `docs/protocol-style-guide.ko.md`, and representative scripts `scripts/build_and_test.ps1`, `scripts/smoke_world_enter.ps1`, `scripts/test_ping.ps1`, `scripts/test_transport_hardening.ps1`.
- Learning: In this repository, "harness" is a workflow contract spanning documentation, script naming, artifact paths, and smoke/build chaining rather than a single standalone framework.

## Entry: 2026-04-14 12:20 KST | Task: Rewrite AGENTS for OMX Team Workflow
### Summary
- Goal: Back up the existing `AGENTS.md`, move the workspace onto OMX guidance, and introduce a Korean interpretation file.
- Outcome: Backed up `AGENTS.md`, rewrote the English guidance around OMX, and added `Agents_kr.md`; a follow-up review later found that some repository-specific protocol rules had been dropped and needed restoration.

### What Went Well
- Established the OMX operating model and skill-routing contract in the workspace policy.
- Added a Korean interpretation file so the policy could be read in both languages.

### Mistakes
- Label: POLICY_GAP
  Evidence: The initial rewrite removed repository-specific protocol verification and learning-loop requirements from the English source.
  Root cause: The rewrite optimized for the shared OMX template but did not merge all local workflow constraints back in before completion.
  Fix applied: Recorded the gap for follow-up and restored the missing local rules in a later corrective task.

### Cost Signals
- Commands run: 13
- Build runs: 0
- Test runs: 0
- Avoidable retries: 0

### Prevention Rules (Next Tasks)
- Keep: When adopting a new workflow framework, diff the old policy against the new template before finalizing the rewrite.
- Add: Treat repository-specific verification contracts as mandatory overlays on top of OMX defaults.
- Remove: None

### Confidence
- Delivery confidence (0-100): 78
- Verification depth: medium

## Entry: 2026-04-14 12:38 KST | Task: Repair AGENTS Policy Drift After Code Review
### Summary
- Goal: Fix the review findings by restoring repository-specific AGENTS requirements, resynchronizing the Korean interpretation, and correcting the inaccurate improvement-log history.
- Outcome: Re-added the protocol and learning-loop rules to `AGENTS.md`, synced `Agents_kr.md` with the missing references/confidence gate, and replaced the inaccurate mid-file worklog entry with corrected append-only history at the end of the log.

### What Went Well
- The code-review findings translated directly into a small corrective patch instead of another broad rewrite.
- English and Korean policy files were updated together, which removed the previous sync drift.

### Mistakes
- Label: REVIEW_ESCAPE
  Evidence: The earlier AGENTS rewrite landed without checking the English file against the repository-specific protocol contract.
  Root cause: Post-rewrite validation focused on OMX structure and missed a direct old-vs-new requirements diff.
  Fix applied: Restored the missing contract sections and tightened the worklog entry so it matches what actually happened.

### Cost Signals
- Commands run: 12
- Build runs: 0
- Test runs: 0
- Avoidable retries: 1

### Prevention Rules (Next Tasks)
- Keep: Review policy rewrites against the previous version and the translated companion file before marking them done.
- Add: If a worklog entry describes policy preservation, verify the exact clauses are still present in the source files.
- Remove: None

### Confidence
- Delivery confidence (0-100): 95
- Verification depth: high


## Entry: 2026-04-14 14:20 KST | Task: Overall Project Code Review
### Summary
- Goal: Perform a repository-wide code review of the current workspace snapshot with evidence from core networking/auth/game modules and a verification build run.
- Outcome: Identified high-risk disconnect lifecycle and credential-handling issues, plus build reproducibility and maintainability gaps across transport, auth, and game entry flows.

### What Went Well
- Reviewing the module boundaries first (Engine/Game/Protocol/Transport/Login/Server) made it easier to focus on the highest-risk integration seams.
- A real build script run exposed an environment/reproducibility blocker instead of relying only on source inspection.

### Mistakes
- Label: TOOLING_ASSUMPTION
  Evidence: `omx explore` was attempted first per session guidance, but the local harness was unavailable because `cargo`/prebuilt support was missing.
  Root cause: I assumed the advisory explore surface was operational before checking the local install state.
  Fix applied: Fell back immediately to direct repository inspection and command-line evidence gathering.

### Cost Signals
- Commands run: 24
- Build runs: 1
- Test runs: 0
- Avoidable retries: 1

### Prevention Rules (Next Tasks)
- Keep: For broad reviews, inspect architecture first and then validate the top findings with exact file:line evidence.
- Add: When OMX explore is unavailable, fall back immediately and record the reason instead of retrying repeatedly.
- Remove: None

### Confidence
- Delivery confidence (0-100): 92
- Verification depth: medium

## Entry: 2026-04-14 15:00 KST | Task: Apply Code Review Fixes
### Summary
- Goal: Fix the highest-priority issues from the repository-wide review without broad unrelated refactors.
- Outcome: Repaired the transport disconnect notification path, removed hardcoded DB credential fallbacks, added a safer local-only default for plaintext auth handshakes, made the client network restartable, hardened a GameApp null-dereference path, and repaired local vcpkg-backed build reproducibility.

### What Went Well
- The fixes stayed concentrated in transport/auth/bootstrap/build surfaces instead of spreading into the engine core.
- Targeted MSBuild runs caught one restart-related compile issue and one Game post-build path issue before the final verification pass.

### Mistakes
- Label: RESTART_GUARD_TYPE
  Evidence: Reassigning `executor_work_guard` directly in `YunoClientNetwork::Start()` failed to compile because the assignment operator is private.
  Root cause: I optimized for the smallest code diff before checking the guard type’s assignment semantics.
  Fix applied: Switched the member to `std::optional<executor_work_guard<...>>` so lifecycle reset/recreate is explicit and compile-safe.

### Cost Signals
- Commands run: 18
- Build runs: 4
- Test runs: 1
- Avoidable retries: 1

### Prevention Rules (Next Tasks)
- Keep: After lifecycle fixes, run a targeted project build before the full script to catch type-level mistakes cheaply.
- Add: For repo-local vcpkg setups, verify both compile-time include resolution and post-build asset copy paths.
- Remove: None

### Confidence
- Delivery confidence (0-100): 95
- Verification depth: high

## Entry: 2026-04-14 11:14 KST | Task: Set Required DB Credential Environment Variables
- Goal: Prevent DB connection failure caused by missing required credential env vars.
- Outcome: Set `YUNO_DB_USER` and `YUNO_DB_PASS` for current process and persisted them at User scope.
- Verification: Confirmed in elevated context that User scope has `YUNO_DB_USER=***REMOVED***` and `YUNO_DB_PASS` is set.
- Learning: For credential configuration, OS environment variables are safer than source/script hardcoding while keeping runtime behavior deterministic.

## Entry: 2026-04-14 16:35 KST | Task: Add World-Enter E2E Probe and Bounded Script Refactor
### Summary
- Goal: Finish the incomplete world-enter verification work by adding a real login->world-enter->snapshot-ack probe and refactoring only the surrounding test scripts.
- Outcome: Added a reusable world-enter probe, a protocol-specific `test_world_enter.ps1` artifact-producing test, and a small shared process-helper script reused by smoke flow; verified build, smoke, and protocol test successfully.

### What Went Well
- Reusing one probe in both smoke and protocol-test flows reduced duplication while keeping scope inside the scripts directory.
- A fast failing smoke run exposed a PowerShell automatic-variable naming issue before the longer test sequence.

### Mistakes
- Label: PS_AUTO_VARIABLE_COLLISION
  Evidence: Initial scripts used `Host` as a parameter name and PowerShell treated it as the read-only automatic variable `$Host`.
  Root cause: I ported helper naming from network terminology without checking PowerShell reserved automatic variables.
  Fix applied: Renamed helper parameters to `HostName` and re-ran smoke/test verification.
- Label: DB_SCHEMA_ASSUMPTION
  Evidence: The first probe username exceeded the SQL procedure's `VARCHAR(30)` limit and caused `REGISTER_FAILED`.
  Root cause: I assumed a timestamped test username length was unconstrained.
  Fix applied: Shortened and bounded the generated username before rerunning the probe.

### Cost Signals
- Commands run: 16
- Build runs: 1
- Test runs: 2
- Avoidable retries: 2

### Prevention Rules (Next Tasks)
- Keep: For PowerShell scripts, avoid parameter names that collide with automatic variables like `$Host`.
- Add: For protocol E2E probes, check backing SQL/schema limits before generating synthetic identifiers.
- Remove: None

### Confidence
- Delivery confidence (0-100): 96
- Verification depth: high

## Entry: 2026-04-14 17:35 KST | Task: Switch Generated Logs and Reports to English by Default
### Summary
- Goal: Change the workspace policy and active script output so generated logs and Markdown reports are authored in English by default.
- Outcome: Updated the repository policy documents (`AGENTS.md`, `Agents_kr.md`, `docs/protocol-test-ops.md`) and converted the remaining Korean report content in `scripts/test_world_enter.ps1` to English. Verified the generated log/report language with a fresh protocol test artifact.

### What Went Well
- The language policy lived in a small set of central documents, so the policy update stayed localized and easy to audit.
- A fresh generated `world_enter` log/report confirmed the new output language without needing to edit historical artifacts.

### Mistakes
- Label: ENV_ASSUMPTION
  Evidence: The verification run timed out waiting for `YunoLoginServer` because the current session did not have the runtime prerequisites needed for the server to bind successfully.
  Root cause: I optimized for fast verification of the regenerated artifact language before rechecking the current environment prerequisites.
  Fix applied: Validated the newly generated log and report contents directly and recorded the blocker in English, which still proves the new language policy is active.

### Cost Signals
- Commands run: 9
- Build runs: 0
- Test runs: 1
- Avoidable retries: 0

### Prevention Rules (Next Tasks)
- Keep: When the task is about artifact wording, verify the generated artifact text directly even if the runtime workflow itself is blocked.
- Add: Treat language-policy changes as both a documentation update and a generated-output update; verify both surfaces.
- Remove: None

### Confidence
- Delivery confidence (0-100): 95
- Verification depth: medium

## Entry: 2026-04-15 17:06 KST | Task: Cleanup Lore Commit History for MORPG Team Run
### Summary
- Goal: Replace OMX team operational checkpoint history with clean Lore-format commit history.
- Outcome: Preserved a backup branch, audited the final tree against `origin/Network_Refactoring`, and prepared the branch for semantic recommit instead of keeping auto-checkpoint noise.

### What Went Well
- The team hygiene report and final task results made it clear which runtime commits were scaffolding versus real delivery content.
- Verifying the final tree before rewriting reduced the risk of losing completed MORPG server-slice changes.

### Mistakes
- Label: RUNTIME_HISTORY_DRIFT
  Evidence: The branch accumulated many `omx(team): auto-checkpoint ...` commits and repeated merge/cherry-pick scaffolding.
  Root cause: Team execution completed without immediate leader-side history squashing.
  Fix applied: Rebased the cleanup plan around a backup branch plus a semantic Lore recommit strategy.

### Cost Signals
- Commands run: 8
- Build runs: 0
- Test runs: 0
- Avoidable retries: 1

### Prevention Rules (Next Tasks)
- Keep: Before finalizing a completed OMX team run, inspect the hygiene report and rewrite runtime scaffolding into semantic Lore commits immediately.
- Add: When team output is done, prefer a single leader-side semantic recommit from the validated tree over preserving auto-checkpoint churn.
- Remove: None

### Confidence
- Delivery confidence (0-100): 94
- Verification depth: medium
