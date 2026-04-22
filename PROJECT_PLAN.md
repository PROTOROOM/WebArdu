# PROJECT_PLAN.md

## 1. Overview

This project aims to build a **workshop-friendly web-based interface** that allows users to modify a limited portion of firmware (e.g., parameters) and upload it to an Arduino-compatible board using a local CLI bridge.

The system is intentionally constrained to:
- Prevent user errors
- Remove complexity (no full IDE)
- Provide a smooth, deterministic workshop experience

---

## 2. Core Concept

Instead of exposing full source code editing, users interact with:

> **A parameterized firmware template**

Example:
- Blink firmware
- User only edits: delay interval (ms)

System flow:

1. User inputs value (e.g., 500 ms)
2. Backend injects value into template
3. Temporary sketch is generated
4. `arduino-cli` compiles and uploads

---

## 3. Architecture

```
[Browser UI]
    ↓ (HTTP / WebSocket)
[Local Server]
    ↓
[Firmware Generator]
    ↓
[arduino-cli]
    ↓
[Arduino Board]
```

---

## 4. Components

### 4.1 Frontend (Web UI)

Minimal interface:
- Input box (number)
- Upload button
- Status display (log)

Optional:
- Slider for value control
- Preset buttons (fast / slow)

Tech options:
- Vanilla JS (preferred for simplicity)
- Or lightweight framework (e.g., Svelte)

---

### 4.2 Backend (Local Server)

Responsibilities:
- Receive user input
- Validate input
- Generate firmware file
- Execute CLI commands
- Stream logs back to UI

Tech options:
- Node.js (Express + child_process)
- OR Python (Flask + subprocess)

---

### 4.3 Firmware Template System

Template example:

```cpp
const int LED_PIN = 13;
int interval = {{INTERVAL}};

void setup() {
  pinMode(LED_PIN, OUTPUT);
}

void loop() {
  digitalWrite(LED_PIN, HIGH);
  delay(interval);
  digitalWrite(LED_PIN, LOW);
  delay(interval);
}
```

Template engine:
- Simple string replacement
- OR mustache-style templating

---

### 4.4 Arduino CLI Integration

Required commands:

```bash
arduino-cli compile --fqbn arduino:avr:uno ./generated_sketch
arduino-cli upload -p <PORT> --fqbn arduino:avr:uno ./generated_sketch
```

Setup requirements:
- arduino-cli installed
- board core installed (e.g., arduino:avr)

---

## 5. Data Flow

1. User enters value (e.g., 300)
2. POST /upload
3. Server:
   - Validates (range check)
   - Injects into template
   - Writes to /tmp/sketch_xxx
4. CLI compile
5. CLI upload
6. Logs streamed back to UI

---

## 6. Input Constraints (IMPORTANT)

To avoid failure during workshop:

- Min/max bounds (e.g., 50–5000 ms)
- Only integers
- Fallback default value

Example:

```
if value < 50 → clamp to 50
if value > 5000 → clamp to 5000
```

---

## 7. UX Design Principles

- Zero configuration
- Zero code exposure
- Single action: "Upload"
- Immediate feedback

States:
- Idle
- Compiling...
- Uploading...
- Success
- Error (with simple message)

---

## 8. File Structure

```
project/
├── frontend/
│   └── index.html
├── backend/
│   └── server.js (or app.py)
├── templates/
│   └── blink.ino
├── generated/
│   └── (temp sketches)
└── PROJECT_PLAN.md
```

---

## 9. Advanced Extensions (Optional)

### 9.1 Multi-Parameter Support
- Brightness (PWM)
- Pattern selection

### 9.2 Multiple Templates
- Blink
- Button input
- Serial interaction

### 9.3 Board Auto Detection
- Parse `arduino-cli board list`
- Auto-select first available port

---

## 10. Failure Handling

Common issues:

- Port not found
- Permission denied
- Board reset timing

Mitigation:
- Retry upload once
- Show simple instruction ("Press reset button")

---

## 11. Workshop Deployment Strategy

### Option A: Individual Setup
- Each participant installs CLI

### Option B (Recommended): Host Machine
- One machine per group
- Preconfigured environment

### Option C: Local Network Server
- One server controls multiple boards

---

## 12. Future Direction

- Replace firmware upload with runtime control (Web Serial)
- Hybrid mode:
  - Initial firmware upload
  - Then real-time parameter control via browser

---

## 13. Minimal Milestone (MVP)

Must include:
- Input field (interval)
- Upload button
- Template injection
- CLI compile/upload

Nice-to-have:
- Log streaming
- Auto port detection

---

## 14. Philosophy

This system is not an IDE.

It is a:

> **controlled interface for constrained creativity**

The goal is not flexibility,
but reliability and immediacy in a workshop context.

