# AF1000X Remote Controller
### PC Hub Transmitter for Micro Drone (RP2040)

## English

### Overview
**AF1000X Remote Controller** is an RP2040-based transmitter that works as a **PC hub**.  
It converts USB serial commands from a PC into RF control signals for the drone.

- PC (USB Serial) → AF1000X Controller (nRF24) → Drone
- No modification required on drone firmware
- Designed for education, research, and automation

### Key Features
1. Physical joystick control (Mode 1/2 supported, Mode 2 default)
2. PC serial command control (Hub Mode)
3. ARM safety gate + 200ms PC timeout failsafe
4. Emergency throttle cut
5. Battery voltage query
6. Unique TX ID stored in EEPROM (boot with GPIO10 held to regenerate)
7. `DBUG` command: 1s debug output for sticks/battery/status
8. USB device name: `SYUBEA AF1000X Controller`

### Hardware
1. MCU: RP2040
2. RF: nRF24L01+
3. USB: RP2040 Native USB (CDC)
4. Inputs: Joystick, buttons
5. Output: RF packets

### PC Hub Architecture
```
PC Script / Application
          ↕ USB Serial (115200)
     AF1000X Controller
          ↕ nRF24
            Drone
```

The controller always acts as a **safety gate**, never allowing direct unsafe control from the PC.

### Serial Command Reference
See:
- `AR1000X_Serial_Command_Reference.md`
- `AR1000X_Controller_Operation.md` (pins/LEDs/buttons)

### Development Environment
1. Arduino IDE 2.3.x
2. Board: Raspberry Pi Pico / RP2040
3. USB Stack: Pico SDK (Default)
4. Baudrate: 115200

Important:  
Remove or disable external TinyUSB libraries from:
```
Documents/Arduino/libraries/Adafruit_TinyUSB_Library
```

### Basic Usage Flow
1. Connect USB and identify port
2. Send `ARM 1` from PC
3. Execute motion/control commands
4. Use `EMERGENCY` if needed
5. Send `ARM 0` before disconnecting

### Contact
This controller is developed by SYUBEA Co., Ltd. (Korea).  
For purchase inquiries, please contact `hello@1510.co.kr`.

---

## 한국어

### 개요
**AF1000X Remote Controller**는 RP2040 기반 조종기이며 **PC 허브**로 동작합니다.  
PC의 USB 시리얼 명령을 RF 제어 신호로 변환해 드론에 전달합니다.

- PC(USB Serial) → AF1000X Controller(nRF24) → Drone
- 드론 펌웨어 수정 없이 사용 가능
- 교육/연구/자동화 목적에 적합

### 주요 기능
1. 물리 조이스틱 조종 (MODE 1/2 동시 지원, 기본 MODE 2)
2. PC 시리얼 명령 제어 (Hub Mode)
3. ARM 안전 게이트 + 200ms PC 통신 타임아웃 페일세이프
4. EMERGENCY 즉시 스로틀 컷
5. 배터리 전압 조회
6. 고유 TX ID EEPROM 저장 (부팅 시 GPIO10 홀드하면 재생성)
7. `DBUG` 명령: 스틱/배터리/상태 1초 주기 출력
8. USB 장치명: `SYUBEA AF1000X Controller`

### 하드웨어
1. MCU: RP2040
2. RF: nRF24L01+
3. USB: RP2040 Native USB (CDC)
4. 입력: 조이스틱, 버튼
5. 출력: RF 패킷

### PC 허브 구조
```
PC Script / Application
          ↕ USB Serial (115200)
     AF1000X Controller
          ↕ nRF24
            Drone
```

컨트롤러는 항상 **안전 게이트**로 동작하며,  
PC에서 들어오는 위험한 명령을 직접 전달하지 않습니다.

### 시리얼 명령
- `AR1000X_Serial_Command_Reference.md`
- `AR1000X_Controller_Operation.md` (핀/LED/버튼)

### 개발 환경
1. Arduino IDE 2.3.x
2. Board: Raspberry Pi Pico / RP2040
3. USB Stack: Pico SDK (Default)
4. Baudrate: 115200

중요:  
외부 TinyUSB 라이브러리가 있으면 충돌할 수 있습니다.  
아래 경로의 라이브러리는 제거/비활성화 해주세요.
```
Documents/Arduino/libraries/Adafruit_TinyUSB_Library
```

### 기본 사용 흐름
1. USB 연결 후 포트 확인
2. PC에서 `ARM 1` 전송
3. 이동/제어 명령 실행
4. 필요 시 `EMERGENCY`
5. 종료 전 `ARM 0` 전송

### 문의
한국 주식회사 슈베아에서 개발한 드론 조종기이며,  
제품 구입 문의는 `hello@1510.co.kr` 로 연락 부탁드립니다.

---

## 日本語

### 概要
**AF1000X Remote Controller**はRP2040ベースの送信機で、**PCハブ**として動作します。  
PCのUSBシリアル命令をRF制御信号に変換してドローンへ送信します。

- PC(USB Serial) → AF1000X Controller(nRF24) → Drone
- ドローン側のファームウェア改修は不要
- 教育/研究/自動化用途に適合

### 主な機能
1. 物理ジョイスティック操作（MODE 1/2対応、MODE 2が標準）
2. PCシリアル命令による制御（Hub Mode）
3. ARM安全ゲート + 200msタイムアウトフェイルセーフ
4. EMERGENCY 即時スロットルカット
5. バッテリー電圧照会
6. 固有TX IDのEEPROM保存（起動時にGPIO10を押し続けると再生成）
7. `DBUG` コマンドで1秒周期のデバッグ出力
8. USBデバイス名: `SYUBEA AF1000X Controller`

### ハードウェア
1. MCU: RP2040
2. RF: nRF24L01+
3. USB: RP2040 Native USB (CDC)
4. 入力: ジョイスティック、ボタン
5. 出力: RFパケット

### PCハブ構成
```
PC Script / Application
          ↕ USB Serial (115200)
     AF1000X Controller
          ↕ nRF24
            Drone
```

コントローラは常に**安全ゲート**として動作し、  
PCからの危険な命令を直接通しません。

### シリアルコマンド
- `AR1000X_Serial_Command_Reference.md`
- `AR1000X_Controller_Operation.md` (ピン/LED/ボタン)

### 開発環境
1. Arduino IDE 2.3.x
2. Board: Raspberry Pi Pico / RP2040
3. USB Stack: Pico SDK (Default)
4. Baudrate: 115200

重要:  
外部TinyUSBライブラリがあると競合する場合があります。  
以下のライブラリは削除/無効化してください。
```
Documents/Arduino/libraries/Adafruit_TinyUSB_Library
```

### 基本使用手順
1. USB接続後、ポートを確認
2. PCから `ARM 1` を送信
3. 移動/制御コマンドを実行
4. 必要に応じて `EMERGENCY`
5. 切断前に `ARM 0` を送信

### お問い合わせ
本ドローンコントローラは韓国のSYUBEA株式会社が開発しました。  
製品購入のお問い合わせは `hello@1510.co.kr` までご連絡ください。

---

© SYUBEA
