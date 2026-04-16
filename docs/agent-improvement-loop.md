
## 2026-04-16 Deep interview capture for party-instance UI scope
- Task: Clarify requirements for party create/join UI, instance dungeon entry, and movement sync before planning/implementation.
- What happened: Ran deep-interview on a brownfield multiplayer slice, grounded questions with existing packet/runtime evidence, and forced an explicit include/exclude correction when the first scope answer mixed both.
- Evidence: Wrote `.omx/context/party-instance-ui-movement-20260416T024652Z.md`, `.omx/interviews/party-instance-ui-movement-20260416T024652Z.md`, and `.omx/specs/deep-interview-party-instance-ui-movement.md`.
- Lesson: When users say “기본 기능만,” treat it as ambiguous until concrete candidate features are classified as in-scope or out-of-scope.
- Keep: In deep-interview mode, if a scope answer mixes includes and excludes, force an explicit feature-by-feature classification before crystallizing.
- Add: For multiplayer UX slices, lock leader authority, join mode, movement input mode, and loading-transition expectations before handing off to planning.
- Remove: None.
