# AGENTS.md (한국어 해석본)

## 목표
- 이 저장소를 느슨한 단일 에이전트 세션이 아니라 Oh My Codex 팀 워크플로우로 운영한다.
- 의사결정은 근거와 검증 중심으로 유지한다.
- 프로젝트 고유의 프로토콜/테스트 요구사항을 OMX 운영 모델 안에 보존한다.

## OMX 운영 모델
- 기본 자세: 수행자라기보다 지휘자처럼 행동하고, 계획/리뷰/검증 단계를 명시적으로 유지한다.
- 직접 큰 변경을 한 번에 넣기보다 다음 단계형 전달을 선호한다:
1. 요청을 이해하고 의도를 분류한다.
2. 승인 기준과 리스크를 계획한다.
3. 작은 범위로 구현한다.
4. 먼저 타깃 검증을 하고, 그 다음 필수 빌드/테스트/스모크를 수행한다.
5. 근거, 리스크, 다음 액션을 보고한다.
- 요청이 모호하면 구현 전에 멈추고 가정을 명시한다.

## 필수 응답 계약
- 모든 기술적 의사결정에는 다음을 포함한다:
1. 선택한 접근
2. 현실적인 대안 1~2개
3. 최종 선택의 트레이드오프 기반 이유
- 왜 그 접근을 선택했는지 설명 없이 결과만 제시하지 않는다.
- 사용자가 학습 중일 때는 짧은 결과보다 의사결정 과정을 우선한다.

## Quality-First 규칙
- 검증 근거 없이 완료를 주장하지 않는다.
- 가능하면 전체 빌드/테스트 전에 타깃 검증을 우선한다.
- 정확성이나 안전한 통합에 필요하지 않다면 관련 없는 리팩터링을 피한다.
- 코드 배치는 기존 모듈 구조와 일관되게 유지한다.
- 서버/네트워크 코드에서는 다음 리스크를 구체적으로 점검한다:
1. 연결 수명주기
2. 패킷 경계와 유효성 검증
3. 스레드 안전성
4. 실패 처리와 로그

## 기본 팀 파이프라인
- 정식 OMX 흐름은 다음으로 본다:
1. deep-interview 또는 의도 명확화
2. plan
3. execute
4. verify
5. 필요 시 fix
- 중간 규모 이상 작업에서는 요청 직후 곧바로 코드 작성으로 들어가지 않는다.
- 리스크가 있는 작업에서는 구현 전에 승인 기준을 명시한다.

## 저장소 전용 제약
- 정당한 이유가 없는 한 기존 스타일과 구조를 유지한다.
- 안정적인 모듈을 불필요하게 재작성하지 않는다.
- 버그 수정 시 다음을 기록한다:
1. 재현 조건
2. 수정 범위
3. 검증 방법
- 문서와 구현이 다르면 둘 다 갱신하고 불일치를 명시적으로 보고한다.

## 프로토콜 작업 레인
- 프로토콜 관련 작업 전 아래 문서를 읽는다:
  - `C:\Project\MORPG_Network_Sample\docs\protocol-template.md`
  - `C:\Project\MORPG_Network_Sample\docs\protocol-style-guide.en.md`
  - `C:\Project\MORPG_Network_Sample\docs\protocol-style-guide.ko.md`
  - `C:\Project\MORPG_Network_Sample\docs\agent-active-rules.md`
  - `C:\Project\MORPG_Network_Sample\docs\protocol-test-ops.md`
- 프로토콜 파일 배치 규칙은 유지한다:
  - 패킷 타입은 `C:\Project\MORPG_Network_Sample\YunoNetProtocol\Public\Net\PacketType.h`에 추가한다.
  - `.h`는 `Public`, `.cpp`는 `Private`에 둔다.
  - `C2SPackets`, `S2CPackets`, `ErrorPackets` 또는 정당한 전용 폴더를 사용한다.
- 필수 빌드/테스트/스모크 단계가 실행되고 결과가 보고되기 전에는 프로토콜 기능 완료를 선언하지 않는다.

## 프로토콜 검증 계약
- 프로토콜 기능 구현 시 다음을 실행한다:
  - `powershell -ExecutionPolicy Bypass -File .\scripts\build_and_test.ps1`
  - `powershell -ExecutionPolicy Bypass -File .\scripts\smoke_world_enter.ps1`
- 또한 `C:\Project\MORPG_Network_Sample\scripts\` 아래에 프로토콜 전용 테스트 스크립트를 생성 또는 갱신한다:
  - `test_<protocol_or_feature>.ps1`
- 실행 전에 아래 결과 디렉터리 존재를 보장한다:
  - `C:\Project\MORPG_Network_Sample\Result\Log`
  - `C:\Project\MORPG_Network_Sample\Result\Report`
- 실행 로그는 다음 위치에 저장한다:
  - `C:\Project\MORPG_Network_Sample\Result\Log\<timestamp>_<protocol_or_feature>.log`
- Markdown 보고서는 다음 위치에 생성한다:
  - `C:\Project\MORPG_Network_Sample\Result\Report\<timestamp>_<protocol_or_feature>_report.md`
- 로그/보고서 언어 정책: 기본 영어
- 보고서에는 반드시 다음을 포함한다:
1. 목적과 기대 동작
2. 실행한 명령
3. Pass/Fail 결과
4. 핵심 로그 근거
5. 실패 원인
6. 다음 액션
- 런타임 제약으로 실행이 막혀도 다음은 반드시 생성한다:
1. 시도한 명령 로그
2. 구체적인 누락 전제조건이 포함된 blocker 분석 보고서

## Agent 학습 루프
- 모든 의미 있는 작업 전에 `C:\Project\MORPG_Network_Sample\docs\agent-active-rules.md`를 읽는다.
- 더 새로운 근거가 없으면 활성 `Keep` 및 `Add` 규칙을 적용한다.
- `docs\agent-improvement-loop.md`는 다음 경우에만 읽는다:
  - 반복 실패
  - 원인 불명
  - 정책 충돌
- 모든 의미 있는 작업 후 `C:\Project\MORPG_Network_Sample\docs\agent-improvement-loop.md`에 새 항목 1개를 append 한다.

## 금지 사항
- 근거 없는 주장을 하지 않는다.
- 대안 없는 단일 해법만 제시하지 않는다.
- 검증을 건너뛰고 성공을 선언하지 않는다.
- 계획과 제약이 명확해지기 전에 범위가 넓은 위험한 변경을 하지 않는다.
- 영향이 큰 모호한 변경은 확신이 95% 이상이 되기 전까지 진행하지 말고, 필요한 확인/조사로 확신을 끌어올린다.

## 번역 동기화 정책
- `Agents_kr.md`는 이 문서의 한국어 해석본이다.
- `AGENTS.md`와 `Agents_kr.md`는 같은 작업에서 동기화 상태를 유지해야 한다.
- 두 파일 사이에 불일치가 있으면 작업 미완료이며 명시적으로 보고해야 한다.
