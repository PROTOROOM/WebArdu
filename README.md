# MatterMatters - WEBARDU

워크숍 환경에서 Arduino 펌웨어의 일부 파라미터만 안전하게 수정하고 업로드하기 위한 최소 웹 인터페이스입니다.

현재 구현은 Blink 템플릿의 interval(ms) 값만 입력받아 `arduino-cli`로 compile/upload를 수행합니다.

## 현재 기능

- 단일 입력값 기반 업로드
  - Blink interval (50~5000ms)
- 템플릿 기반 코드 생성
  - `templates/blink.ino`의 `{{INTERVAL}}` 치환
- Arduino 보드 자동 감지
  - `arduino-cli board list --format json` 결과 기반
  - 첫 번째 지원 보드 자동 선택
- 웹 UI에서 상태/로그 확인
- 레트로 UI (Game Boy 톤)

## 프로젝트 구조

```text
MatterMatters-WEBARDU/
├── frontend/
│   └── index.html
├── backend/
│   └── server.js
├── templates/
│   └── blink.ino
├── generated/
├── PROJECT_PLAN.md
├── PROJECT_DEVLOG.md
├── package.json
└── README.md
```

## 사전 준비

### 1) Node.js

- Node.js 18+ 권장

### 2) arduino-cli 설치

- `arduino-cli`가 PATH에서 실행 가능해야 함

예시 확인:

```bash
arduino-cli version
```

### 3) 보드 코어 설치

UNO 기준 예시:

```bash
arduino-cli core update-index
arduino-cli core install arduino:avr
```

## 실행 방법

프로젝트 루트에서:

```bash
npm start
```

서버 기본 주소:

- `http://127.0.0.1:3000`

## 사용 방법

1. Arduino 보드를 USB로 연결
2. 브라우저에서 `http://127.0.0.1:3000` 접속
3. 상단 `Detected Board`에 자동 선택된 보드/포트 확인
4. 필요 시 `RESCAN` 클릭
5. interval 값 입력 후 `UPLOAD` 클릭
6. 하단 로그로 compile/upload 결과 확인

## API

### `GET /boards`

- 감지된 보드 목록과 자동 선택 보드 반환

응답 예시:

```json
{
  "ok": true,
  "boards": [
    {
      "port": "/dev/tty.usbmodem1101",
      "fqbn": "arduino:avr:uno",
      "boardName": "Arduino Uno"
    }
  ],
  "selected": {
    "port": "/dev/tty.usbmodem1101",
    "fqbn": "arduino:avr:uno",
    "boardName": "Arduino Uno"
  }
}
```

### `POST /upload`

- interval을 검증/보정 후 템플릿 생성 → compile → upload 실행

요청 예시:

```json
{
  "interval": 500,
  "port": "/dev/tty.usbmodem1101",
  "fqbn": "arduino:avr:uno"
}
```

참고:

- 서버는 업로드 직전에 보드 목록을 다시 읽고, 유효한 대상이 없으면 실패를 반환합니다.
- 현재 기본 정책은 "자동 첫 보드 선택"입니다.

## 환경 변수

- `HOST` (기본: `127.0.0.1`)
- `PORT` (기본: `3000`)
- `ARDUINO_FQBN` (기본: `arduino:avr:uno`)

참고:

- 현재 업로드 포트는 자동 감지로 처리하므로 `ARDUINO_PORT` 설정이 필수는 아닙니다.

## 트러블슈팅

- 보드가 감지되지 않음
  - 케이블/포트 연결 확인
  - `arduino-cli board list` 결과 확인
- 업로드 권한 오류
  - macOS/Linux의 포트 접근 권한 확인
- 코어 미설치 오류
  - `arduino-cli core install arduino:avr` 재확인

## 개발 로그

- 구현 진행 내역: `PROJECT_DEVLOG.md`
- 기획 문서: `PROJECT_PLAN.md`
