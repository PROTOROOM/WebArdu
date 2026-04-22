# MatterMatters - WEBARDU

워크숍 환경에서 Arduino 펌웨어의 일부 파라미터만 안전하게 수정하고 업로드하기 위한 최소 웹 인터페이스입니다.

현재 구현은 `templates/*.ino` 템플릿을 동적으로 읽어, 템플릿별 파라미터를 입력받아 `arduino-cli`로 compile/upload를 수행합니다.

## 현재 기능

- 멀티 템플릿 업로드
  - `templates/*.ino` 자동 인식
  - 템플릿별 파라미터 UI 자동 생성
- 템플릿 기반 코드 생성
  - `{{PARAM_NAME}}` 치환
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
3. 템플릿을 선택하고 파라미터를 입력
4. 상단 `Detected Board`에 자동 선택된 보드/포트 확인
5. 필요 시 `RESCAN` 클릭
6. `UPLOAD` 클릭
7. 하단 로그로 compile/upload 결과 확인

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

### `GET /templates`

- `templates/*.ino` 목록과 각 템플릿의 파라미터 스키마 반환

응답 예시:

```json
{
  "ok": true,
  "templates": [
    {
      "id": "blink",
      "fileName": "blink.ino",
      "name": "Blink",
      "description": "LED를 고정 간격으로 점멸합니다.",
      "params": [
        {
          "key": "INTERVAL",
          "label": "Blink Interval (ms)",
          "type": "int",
          "min": 50,
          "max": 5000,
          "default": 500
        }
      ]
    }
  ],
  "selected": {
    "id": "blink"
  }
}
```

### `POST /upload`

- interval을 검증/보정 후 템플릿 생성 → compile → upload 실행

요청 예시:

```json
{
  "templateId": "blink",
  "params": {
    "INTERVAL": 500
  },
  "port": "/dev/tty.usbmodem1101",
  "fqbn": "arduino:avr:uno"
}
```

참고:

- 서버는 업로드 직전에 보드 목록을 다시 읽고, 유효한 대상이 없으면 실패를 반환합니다.
- 현재 기본 정책은 "자동 첫 보드 선택"입니다.

## 템플릿 추가 방법

`templates/`에 `.ino` 파일을 추가하면 자동으로 목록에 나타납니다.

권장 형식:

```cpp
/* @WEBARDU_META
{
  "id": "blink",
  "name": "Blink",
  "description": "LED를 고정 간격으로 점멸합니다.",
  "params": [
    {
      "key": "INTERVAL",
      "label": "Blink Interval (ms)",
      "type": "int",
      "min": 50,
      "max": 5000,
      "default": 500
    }
  ]
}
*/

int interval = {{INTERVAL}};
```

메모:
- `key`는 `{{KEY}}` 플레이스홀더와 동일해야 합니다.
- `params`가 없으면 플레이스홀더를 자동 추론해 텍스트 입력으로 노출합니다.
- 지원 타입: `int`, `float`, `bool`, `enum`, `text`, `code`

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
