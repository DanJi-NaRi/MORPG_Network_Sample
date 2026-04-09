# User Protocol Add Prompt Template (KO)

아래 템플릿을 복사해서 값만 채우면 된다.

```text
작업 목표:
- 신규 프로토콜을 추가하고 서버 반영/검증까지 완료해.

사전 참조:
- C:\Project\MORPG_Network_Sample\AGENTS.md
- C:\Project\MORPG_Network_Sample\docs\protocol-template.md
- C:\Project\MORPG_Network_Sample\docs\protocol-style-guide.ko.md
- C:\Project\MORPG_Network_Sample\docs\protocol-test-ops.md

입력 정보:
1) 패킷 name:
- 요청:
- 응답:
- (선택) 에러/알림:

2) 필드:
- 요청 필드:
  - <name>: <type>, <required|optional>, <설명>
- 응답 필드:
  - <name>: <type>, <required|optional>, <설명>

3) 기능:
- 이 프로토콜이 수행해야 하는 기능:

4) 기대 동작:
- 성공 시:
- 실패 시:

5) 실패 케이스/에러코드:
- <ERROR_CODE_1>: <조건/의미>
- <ERROR_CODE_2>: <조건/의미>

6) 호환성 조건:
- 구버전 클라이언트 동작:
- 최소 지원 버전(선택):
- 하위 호환 정책(optional 추가/append-only 등):

자동화 요구사항:
1. scripts 폴더에 테스트 스크립트 생성/갱신:
   - .\scripts\test_<protocol_or_feature>.ps1
2. 테스트 실행 후 결과 산출:
   - 로그: .\Result\Log\<timestamp>_<protocol_or_feature>.log
   - 리포트: .\Result\Report\<timestamp>_<protocol_or_feature>_report.md
3. 환경 이슈로 테스트 불가 시에도:
   - 시도한 명령 로그 + 차단 원인 리포트 생성
4. 리포트 작성 원칙:
   - 로그는 원문 유지
   - 리포트는 한글 작성

결과 보고:
- 변경 파일 목록
- 대안 비교 + 최종 선택 이유
- 테스트 실행 결과(pass/fail, 핵심 로그 근거)
- 리포트 파일 경로
```
