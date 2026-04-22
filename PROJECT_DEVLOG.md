# PROJECT_DEVLOG.md

## 2026-04-23

### 1) PLAN 기반 최소 MVP 1차 구현

- `PROJECT_PLAN.md`를 기준으로 초기 구조를 생성함.
- 디렉터리 구성:
  - `frontend/`
  - `backend/`
  - `templates/`
  - `generated/`
- 추가 파일:
  - `templates/blink.ino`
  - `frontend/index.html`
  - `backend/server.js`
  - `package.json`

핵심 구현 내용:

- 프론트엔드 (`frontend/index.html`)
  - interval(ms) 숫자 입력
  - Upload 버튼
  - 상태 표시(Idle/Success/Error)
  - 로그 출력 영역
- 백엔드 (`backend/server.js`)
  - `GET /` : UI 제공
  - `POST /upload` : 업로드 프로세스 실행
  - interval 입력 검증/보정(clamp): 50~5000, 정수, fallback 500
  - 템플릿 치환(`{{INTERVAL}}`) 후 `generated/`에 임시 스케치 생성
  - `arduino-cli compile` 실행
  - 업로드 로그를 응답으로 반환
- 템플릿 (`templates/blink.ino`)
  - blink 간격을 파라미터로 주입 가능한 형태로 구성

검증:

- `node --check backend/server.js` 통과

---

### 2) 포트 수동 설정 제거 + 자동 보드 선택 적용

요청사항:

- 매번 포트를 환경변수로 설정하지 않고, 현재 연결 상태를 확인해 자동으로 하드웨어/포트를 사용하도록 변경

변경 내용:

- 백엔드 (`backend/server.js`)
  - `arduino-cli board list --format json` 기반 보드 감지 로직 추가
  - 감지 결과 정규화 함수 추가(`normalizeBoards`)
  - 자동 선택 로직 추가(`pickBoard`):
    - 요청값이 없으면 첫 번째 지원 보드 자동 선택
  - 업로드 시점마다 보드 재조회 후 compile/upload 수행
  - `GET /boards` API 추가:
    - 감지된 보드 목록 + 기본 자동 선택 보드 반환
  - `POST /upload` 동작 변경:
    - 선택된(또는 자동 선택된) `fqbn`, `port`로 compile/upload 수행
    - 지원 보드 미감지 시 에러 반환
- 프론트엔드 (`frontend/index.html`)
  - 페이지 로드시 `/boards` 조회
  - 자동 선택된 보드/포트 표시
  - `Rescan` 버튼 추가
  - 보드 미감지 시 Upload 비활성화
  - 업로드 요청에 `port`, `fqbn` 포함

효과:

- `ARDUINO_PORT` 수동 지정 없이 원클릭 업로드 가능
- 연결 상태 변화(재연결/포트 변경)에 대한 대응력 향상

검증:

- `node --check backend/server.js` 통과

---

### 3) UI 스타일 변경 - Pico-8 IDE 감성

요청사항:

- 디자인을 Pico-8 IDE 느낌으로 변경

변경 내용 (`frontend/index.html`):

- 레트로 픽셀 UI 스타일 적용
  - 각진 테두리, 하드 섀도우, 픽셀 폰트 계열
  - 스캔라인/레트로 배경 질감
- 버튼/패널/로그 영역을 터미널형 스타일로 재구성
- 모바일 대응 레이아웃 유지

---

### 4) UI 색상 톤 변경 - Nintendo Game Boy 팔레트

요청사항:

- 색톤을 게임보이 느낌으로 변경

변경 내용 (`frontend/index.html`):

- 컬러 시스템을 4톤 그린 계열로 재정의
  - `#0f380f`
  - `#306230`
  - `#8bac0f`
  - `#9bbc0f`
- 기존 보라/핑크 계열 제거
- 배경/패널/입력/버튼/상태/로그 전 영역에 통일 적용

결과:

- 기능은 유지하면서 시각 톤을 게임보이 스타일로 일관되게 전환

---

### 5) UI 색상 톤 재조정 - Game Boy 케이스(회색) 톤

요청사항:

- 기존 스크린 그린톤이 아닌, Game Boy 본체 케이스 느낌의 회색톤으로 변경

변경 내용 (`frontend/index.html`):

- `:root` 컬러 변수를 회색 계열 중심으로 재정의
- 배경의 스캔라인/라디얼 그라디언트도 녹색 계열에서 회색 계열로 전환
- 입력창/버튼/로그 패널의 하드코딩 그린 색상을 제거하고 회색 테마로 통일
- 텍스트/그림자 대비를 회색 팔레트 기준으로 재조정

결과:

- 레트로 픽셀 UI 구조는 유지하면서, 시각 인상을 "Game Boy 케이스" 스타일로 전환

---

### 6) 프로젝트 타이틀 변경 - MatterMatters - WEBARDU

요청사항:

- 프로젝트 제목을 `MatterMatters - WEBARDU`로 통일

변경 파일:

- `frontend/index.html`
  - `<title>` 변경
  - 메인 헤더 텍스트 변경
- `README.md`
  - 문서 타이틀 변경
  - 프로젝트 구조 예시 루트명 표기 조정
- `backend/server.js`
  - 서버 시작 로그 문구 변경
- `package.json`
  - 패키지명을 `mattermatters-webardu`로 변경

결과:

- UI, 문서, 실행 로그, 패키지 메타데이터에서 프로젝트 명칭 일관성 확보
