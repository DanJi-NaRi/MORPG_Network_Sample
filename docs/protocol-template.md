# Protocol Change Template

Use this template before implementing any protocol change.

## 1) Change Type
- [ ] Add (new packet, new optional field, new error code)
- [ ] Modify (field/type/semantic change)
- [ ] Deprecate (soft remove with compatibility period)

## 2) Feature Context
- Feature name:
- Owner module:
- Related issue/ticket:
- Why this protocol change is needed:

## 3) Compatibility Policy
- Client compatibility target:
  - [ ] Fully backward compatible
  - [ ] Requires minimum client version:
- Server rollout policy:
  - [ ] Can run mixed old/new clients
  - [ ] Needs coordinated deploy
- Fallback behavior for old clients:

## 4) Packet Definitions
Write all packet specs explicitly.

### C2S request
- Packet name:
- Route/opcode:
- Fields:
1. name / type / required / validation
2. name / type / required / validation

### S2C response
- Packet name:
- Route/opcode:
- Success fields:
1. name / type / required / notes
- Error fields:
1. code / message / retryable

### Server events (optional)
- Packet name:
- Trigger condition:
- Payload fields:

## 5) Validation Rules
- Authentication required:
- State preconditions (example: character selected):
- Input range checks:
- Rate limit rule:
- Duplicate request handling:

## 6) Error Codes
List all new or changed codes.

1. `WORLD_ENTER_INVALID_STATE`:
- Meaning:
- Client action:
- Retry policy:

2. `WORLD_ENTER_MAP_NOT_FOUND`:
- Meaning:
- Client action:
- Retry policy:

## 7) Server Flow
1. Receive request
2. Validate auth and state
3. Resolve target world/shard
4. Commit session transition
5. Respond with success/failure
6. Emit event/log/metric

## 8) Observability
- Logs to add:
- Metrics to add:
  - request_count
  - success_rate
  - p95_latency_ms
- Trace/span name:

## 9) Test Plan
- Unit tests:
1. Valid request path
2. Invalid state path
3. Rate limit path
- Integration tests:
1. Login -> WorldEnter success
2. Duplicate request handling
3. Disconnect during transition
- Compatibility tests:
1. Old client behavior
2. New client behavior

## 10) Rollout Plan
- Deploy order:
- Feature flag:
- Rollback condition:
- Rollback action:

## 11) Implementation Checklist
- [ ] Protocol spec updated
- [ ] Shared protocol code regenerated/updated
- [ ] Server handler implemented
- [ ] Tests added/updated
- [ ] Build/test passed
- [ ] Smoke test passed
