# 프로토콜 작성 스타일 가이드 (KO)

패킷 프로토콜을 추가/수정할 때, 기존 프로젝트 형식과 동일하게 맞추기 위한 기준이다.

## 1) 적용 범위
- 클라이언트-서버 패킷 정의와 핸들러 연결 작업 전체에 적용한다.
- 아래 문서와 함께 사용한다.
  - `C:\Project\MORPG_Network_Sample\docs\protocol-template.md`

## 2) 의사결정 우선순위
- 이 가이드와 실제 코드 패턴이 충돌하면:
1. 현재 코드베이스 패턴을 우선 따른다.
2. 충돌 내용을 작업 보고에 명시한다.
3. 가이드 업데이트안을 제안한다.

## 3) 네이밍 규칙
- 요청 패킷: `C2S_<Feature><Action>Req`
- 응답 패킷: `S2C_<Feature><Action>Res`
- 알림 패킷: `S2C_<Feature><Action>Ntf`
- 핸들러 메서드: `Handle<Feature><Action>`
- Feature/Action 이름은 PascalCase를 유지한다.

## 4) 파일 배치 규칙
- 패킷 타입 등록은 필수:
  - 신규 패킷 타입을 `C:\Project\MORPG_Network_Sample\YunoNetProtocol\Public\Net\PacketType.h`에 추가한다.
- 도메인별 루트 모듈 선택:
1. Game 관련 패킷:
   - `C:\Project\MORPG_Network_Sample\YunoGameProtocol\Public\...`
   - `C:\Project\MORPG_Network_Sample\YunoGameProtocol\Private\...`
2. System 관련 패킷:
   - `C:\Project\MORPG_Network_Sample\YunoNetProtocol\Public\...`
   - `C:\Project\MORPG_Network_Sample\YunoNetProtocol\Private\...`
- Public/Private 파일 쌍 규칙:
  - `Public`에는 `.h` 파일 추가
  - `Private`에는 `.cpp` 파일 추가
- 패킷 분류 폴더 규칙(Game/System 공통):
1. C2S 패킷 -> `C2SPackets`
2. S2C 패킷 -> `S2CPackets`
3. Error 패킷 -> `ErrorPackets`
4. 그 외 패킷 -> 별도 목적 폴더를 생성해 관리
- 라우팅/opcode 매핑 및 핸들러 연결은 기존 패킷과 동일한 위치/스타일을 따른다.

## 5) 필드 작성 규칙
- 필드 순서는 아래 책임 순서를 따른다.
1. 사용자/세션 식별 정보
2. 대상 리소스 식별자
3. 액션 파라미터
4. 메타/제어 필드
- 호환성이 필요한 경우, optional 필드는 뒤에 추가(append-only)한다.
- 하나의 필드에 두 가지 의미를 섞지 않는다.

## 6) 검증 규칙
- 비즈니스 로직 전에 프로토콜 경계에서 검증한다.
1. 인증/세션 유효성
2. 상태 전제조건
3. 범위/열거형 값 검증
4. 중복 요청/재전송 검증
- 실패는 일반 실패 대신 명시적 에러코드로 응답한다.

## 7) 에러코드 규칙
- 네이밍: `<FEATURE>_<REASON>` (대문자 스네이크 케이스)
- 에러코드 하나에는 하나의 실패 원인만 담는다.
- 재시도 가능 여부(`retryable`)를 문서화한다.

## 8) 호환성 규칙
- 단순 추가 변경은 기본적으로 하위 호환을 유지한다.
- 비호환 변경이 필요하면:
1. 버전 체크 또는 기능 플래그로 게이트한다.
2. 마이그레이션 기간과 폴백 동작을 정의한다.
3. 롤아웃/롤백 절차를 문서화한다.

## 9) 로그/메트릭 규칙
- 요청 시작 로그 1줄, 실패 로그 1줄(사유 코드 포함)을 남긴다.
- 최소 메트릭:
  - 요청 수
  - 성공 수/성공률
  - 지연시간(p95 또는 p99)

## 10) Agent 결과 보고 필수 항목
- 변경 파일 목록
- 프로토콜 요약:
  - 추가/변경 패킷
  - 필드 변경
  - opcode/route 변경
  - 에러코드 변경
- 대안 비교 및 최종 선택 이유
- 빌드/테스트/스모크 실행 결과

## 11) 최소 프롬프트 스니펫
```text
먼저 읽어:
- C:\Project\MORPG_Network_Sample\docs\protocol-template.md
- C:\Project\MORPG_Network_Sample\docs\protocol-style-guide.ko.md

요구사항:
- 기존 네이밍/라우팅/필드 순서 규칙을 지켜서 프로토콜을 추가해.
- 파일 배치 규칙(PacketType.h + Public/Private + 분류 폴더)을 정확히 적용해.
- 특별한 이유가 없으면 하위 호환 유지.
- 대안 비교와 선택 근거를 결과에 포함해.
```
