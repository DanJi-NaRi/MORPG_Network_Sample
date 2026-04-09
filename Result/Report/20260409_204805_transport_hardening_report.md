# Protocol Test Report: transport_hardening

## Objective
- Transport/Server 경계에서 세션 수명주기와 패킷 처리 안정성을 강화한 변경이 빌드 및 기본 스모크에서 정상 동작하는지 검증한다.

## Expected Behavior
- `build_and_test.ps1`가 대상 모듈 빌드를 모두 통과한다.
- `smoke_world_enter.ps1`가 로그인 서버/월드 서버 기동 후 비정상 종료 없이 종료된다.
- 과도 패킷/세션 한도 방어 로직 추가 후에도 기본 실행 경로가 깨지지 않는다.

## Executed Commands
- `powershell -ExecutionPolicy Bypass -File .\scripts\test_transport_hardening.ps1`
- 내부 실행:
- `powershell -ExecutionPolicy Bypass -File .\scripts\build_and_test.ps1 -Configuration Debug -Platform x64`
- `powershell -ExecutionPolicy Bypass -File .\scripts\smoke_world_enter.ps1 -Configuration Debug -Platform x64`

## Result
- PASS

## Key Log Evidence
- `YunoNetProtocol`, `YunoGameProtocol`, `YunoLoginServer`, `YunoServer`, `YunoGame` 빌드 단계 모두 통과.
- `Starting YunoLoginServer => C:\Project\MORPG_Network_Sample\Bin\x64\Debug\LoginServer\YunoLoginServer.exe`
- `Starting YunoServer => C:\Project\MORPG_Network_Sample\Bin\x64\Debug\Server\YunoServer.exe`
- `==> smoke_world_enter.ps1 completed successfully.`
- `==> test_transport_hardening.ps1 completed successfully`

## Failure Analysis (if any)
- Root cause:
  - 없음 (최종 실행 기준)
- Scope impact:
  - 없음

## Next Actions
- world_enter_probe를 추가해 프로세스 생존성이 아닌 실제 월드 진입 프로토콜 왕복 검증으로 확장.
- 세션 rate-limit 임계치(pps, move-input pps)를 부하 테스트 기반으로 튜닝.
