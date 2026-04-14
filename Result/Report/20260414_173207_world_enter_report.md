# Protocol Test Report: world_enter

## Objective
- Verify the real runtime login -> world-enter -> spawn -> world-snapshot -> snapshot-ack flow.

## Expected Behavior
- Registration or existing-account login succeeds.
- The login response returns a valid world host/port and login token.
- The world server sends SpawnEntity and WorldSnapshot packets.
- After the client ACKs the first snapshot, a later snapshot reflects the ACK via baseSnapshotId.

## Executed Commands
- Start YunoLoginServer port=7000

## Result
- FAIL

## Key Log Evidence
- See the log file for details.

## Failure Analysis (if any)
- Root cause: Login server startup timeout on port 7000
- Scope impact: The login-to-world-enter E2E verification failed or one of the prerequisite servers was not ready.

## Next Actions
- Check login/world server startup prerequisites and DB environment variables.
- Use the world_enter_probe.ps1 log to isolate the failing stage: register, login, enter-world, or snapshot ACK.
