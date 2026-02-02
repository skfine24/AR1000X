# AF1000X Controller  
### PC Hub Transmitter for Micro Drone (RP2040)

---

## 🇰🇷 한국어 설명

### 프로젝트 개요
**AF1000X Controller**는 RP2040 기반의 드론 조종기로,  
PC와 USB(네이티브 CDC)로 연결되어 **시리얼 명령을 드론 제어 신호(RF)** 로 변환하는  
**PC 허브형 조종기** 프로젝트입니다.

- PC → (USB Serial) → AF1000X → (nRF24) → Drone
- 기존 드론 펌웨어 수정 없이 사용 가능
- 교육 / 연구 / 자동비행 / 스크립트 제어 목적

---

### 주요 기능
- 🎮 **물리 조이스틱 조종**
- 💻 **PC 시리얼 명령 기반 제어 (Hub Mode)**
- 🔐 ARM 기반 안전 게이트
- ⏱ 200ms PC 통신 타임아웃 Fail-safe
- 🚨 EMERGENCY 즉시 스로틀 컷
- 🔋 배터리 전압 조회
- 🖥 USB 장치명: `SYUBEA AF1000X Controller`

---

### 하드웨어 구성
- MCU: **RP2040**
- RF: **nRF24L01+**
- USB: RP2040 Native USB (CDC)
- 입력: 조이스틱, 버튼
- 출력: RF 패킷 (드론)

---

### PC 허브 모드 개념
```text
PC Script / App
      ↓ (Serial 115200)
AF1000X Controller
      ↓ (nRF24)
     Drone
```

AF1000X는 **PC 명령을 직접 드론에 전달하지 않고**,  
항상 안전 로직과 상태 검사를 거쳐 RF로 송신합니다.

---

### 시리얼 명령
자세한 명령어 목록은 아래 문서를 참고하세요.

📄 **AR1000X_Serial_Command_Reference.md**

---

### 개발 환경
- Arduino IDE ≥ 2.3.x
- Board: Raspberry Pi Pico / RP2040
- USB Stack: **Pico SDK (Default)** ✅
- Baudrate: 115200

⚠️ 주의  
`Documents/Arduino/libraries/Adafruit_TinyUSB_Library`  
폴더에 외부 TinyUSB 라이브러리가 있으면 삭제(또는 비활성화) 필요

---

### 기본 사용 순서
1. USB 연결 → 포트 인식 확인
2. PC에서 `ARM 1` 전송
3. 이동 / 조종 명령 실행
4. 필요 시 `EMERGENCY`
5. 종료 후 `ARM 0`

---

## 🇺🇸 English Description

### Project Overview
**AF1000X Controller** is an RP2040-based micro drone transmitter that works as a  
**PC hub controller**, converting serial commands from a PC into RF control signals.

- PC → (USB Serial) → AF1000X → (nRF24) → Drone
- No modification required on drone firmware
- Designed for education, research, and autonomous flight

---

### Key Features
- 🎮 Physical joystick control
- 💻 PC-based serial command control (Hub Mode)
- 🧠 dd3-style relative motion commands
- 🔐 ARM-based safety gate
- ⏱ 200ms PC timeout fail-safe
- 🚨 Emergency throttle cut
- 🔋 Battery voltage query
- 🖥 USB Device Name: `SYUBEA AF1000X Controller`

---

### Hardware
- MCU: **RP2040**
- RF: **nRF24L01+**
- USB: RP2040 Native USB (CDC)
- Inputs: Joystick, buttons
- Output: RF packets

---

### PC Hub Architecture
```text
PC Script / Application
          ↓
      USB Serial
          ↓
 AF1000X Controller
          ↓
        nRF24
          ↓
        Drone
```

The controller always acts as a **safety gate**,  
never allowing direct unsafe control from the PC.

---

### Serial Command Reference
See the following document for full command list:

📄 **AF1000X_Serial_Command_Reference.md**

---

### Development Environment
- Arduino IDE ≥ 2.3.x
- Board: Raspberry Pi Pico / RP2040
- USB Stack: **Pico SDK (Default)** ✅
- Baudrate: 115200

⚠️ Important  
Remove or disable external TinyUSB libraries from:
```
Documents/Arduino/libraries/Adafruit_TinyUSB_Library
```

---

### Basic Usage Flow
1. Connect USB and identify port
2. Send `ARM 1` from PC
3. Execute motion/control commands
4. Use `EMERGENCY` if needed
5. Send `ARM 0` before disconnecting

---

## 📜 License / Usage
This project is intended for:
- Educational use
- Research & prototyping
- Robotics and drone development

Commercial use may require permission from **SYUBEA**.

---

© SYUBEA · AF1000X Controller
