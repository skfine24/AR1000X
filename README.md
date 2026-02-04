# AR1000X Remote Controller
### Drone Controller and PC Hub Transmitter for AF1000X

## English

### Overview
**AR1000X Remote Controller** is an RP2040-based transmitter that works as a **PC hub**.  
It converts USB serial commands from a PC into RF control signals for the drone.

- PC (USB Serial) -> AF1000X Controller (nRF24) -> Drone
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
9. GPIO12 user button: short press = drone LED color step (WS2812C on GPIO17), long press (1.5s) = servo toggle on GPIO36 (0~180 deg, 900~2000us)
10. Swarm/AMC mode: control up to 4 drones (D1~D4) from a single controller

### Hardware
1. MCU: RP2040
2. RF: nRF24L01+
3. USB: RP2040 Native USB (CDC)
4. Inputs: Joystick, buttons
5. Output: RF packets

### PC Hub Architecture
```
PC Script / Application
          -> USB Serial (115200)
     AF1000X Controller
          -> nRF24
            Drone
```

The controller always acts as a **safety gate**, never allowing direct unsafe control from the PC.

### Serial Command Reference
See:
- `AR1000X_Serial_Command_Reference.md`
- `AR1000X_Controller_Operation.md` (pins/LEDs/buttons)

### LED Behavior (Power-On / Status)
1. Boot sequence: LED1 -> LED2 -> LED3, 0.5s each (total 1.5s)
2. Low battery: LED1 blinks at 0.5s, LED2/LED3 OFF
3. Bound to drone: LED1 solid ON
4. Not bound: LED1 blinks at 1s
5. Headless ON: LED2 solid ON
6. Trim mode ON: LED3 solid ON (unless AMC mode)
7. AMC mode ON: LED3 blinks 1s ON / 1s OFF

### Swarm / AMC (Up to 4 Drones)
Use AMC mode to control multiple drones.  
Commands prefixed with `D1`~`D4` target a specific drone. Default target is `D1`.

**Example**
```
ARM 1
AMC ON
D1 TAKEOFF
D2 TAKEOFF
D3 TAKEOFF
D4 TAKEOFF
D1 UP 120 1000
D2 DOWN 120 1000
EMERGENCY
AMC OFF
ARM 0
```

### Development Environment
1. Arduino IDE 2.3.x
2. Board: Raspberry Pi Pico / RP2040
3. USB Stack: Pico SDK (Default)
4. Baudrate: 115200

### Required Libraries
Controller (AR1000X):
1. RF24 by TMRh20

Drone (AF1000X):
1. RF24 by TMRh20
2. VL53L1X by Pololu
3. ICM45686 library (provides `ICM45686.h`)
4. Adafruit NeoPixel (WS2812C LED on GPIO17)

Note: `Wire`, `SPI`, `EEPROM`, `Preferences` are provided by the board core.

### Library Links & Versions (tested)
Controller (AR1000X):
1. RF24 by TMRh20 v1.5.0

Drone (AF1000X):
1. RF24 by TMRh20 v1.5.0
2. VL53L1X by Pololu v1.3.1
3. ICM45686 by TDK/Invensense v1.0.6
4. Adafruit NeoPixel v1.15.2

Links:
```
RF24: https://nRF24.github.io/RF24/
VL53L1X: https://github.com/pololu/vl53l1x-arduino
ICM45686: https://github.com/tdk-invn-oss/motion.arduino.ICM45686
Adafruit NeoPixel: https://github.com/adafruit/Adafruit_NeoPixel
```

Note: Versions above are from the current dev environment; Library Manager latest is also acceptable.

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
### AF1000X 드론 컨트롤러 및 PC 허브 송신기

### 개요
**AR1000X Remote Controller**는 RP2040 기반 조종기로 **PC 허브**처럼 동작합니다.  
PC의 USB 시리얼 명령을 RF 제어 신호로 변환해 드론에 전송합니다.

- PC(USB Serial) -> AF1000X Controller(nRF24) -> Drone
- 드론 펌웨어 수정 불필요
- 교육/연구/자동화 목적

### 주요 기능
1. 물리 조이스틱 조종 (MODE 1/2 지원, 기본 MODE 2)
2. PC 시리얼 명령 제어 (Hub Mode)
3. ARM 안전 게이트 + 200ms PC 타임아웃
4. 긴급 스로틀 컷
5. 배터리 전압 조회
6. 고유 TX ID EEPROM 저장 (부팅 시 GPIO10 홀드하면 재생성)
7. `DBUG` 명령: 스틱/배터리/상태 1초 주기 출력
8. USB 디바이스 이름: `SYUBEA AF1000X Controller`
9. GPIO12 사용자 버튼: 짧게 = 드론 LED 색상 단계(WS2812C, GPIO17), 길게(1.5초) = 서보 토글(GPIO36, 0~180 deg, 900~2000us)
10. 군집/AMC 모드: 1대 조종기로 최대 4대(D1~D4) 제어

### 하드웨어
1. MCU: RP2040
2. RF: nRF24L01+
3. USB: RP2040 Native USB (CDC)
4. 입력: 조이스틱, 버튼
5. 출력: RF 패킷

### PC 허브 구조
```
PC Script / Application
          -> USB Serial (115200)
     AF1000X Controller
          -> nRF24
            Drone
```

조종기는 **안전 게이트**로 동작하며, PC에서 들어오는 위험한 명령을 그대로 통과시키지 않습니다.

### 시리얼 명령
- `AR1000X_Serial_Command_Reference.md`
- `AR1000X_Controller_Operation.md` (핀/LED/버튼)

### LED 동작 규칙
1. 부팅 시퀀스: LED1 -> LED2 -> LED3, 각 0.5초 (총 1.5초)
2. 배터리 전압 낮음: LED1 0.5초 점멸, LED2/LED3 OFF
3. 드론 바인딩 완료: LED1 고정 ON
4. 미연결 상태: LED1 1초 점멸
5. 헤드리스 ON: LED2 고정 ON
6. 트림 모드 ON: LED3 고정 ON (AMC 모드 우선)
7. AMC 모드 ON: LED3 1초 ON / 1초 OFF

### 군집 / AMC (최대 4대)
AMC 모드로 여러 대 드론을 제어합니다.  
명령 앞에 `D1`~`D4`를 붙이면 해당 드론에만 적용됩니다. 기본 타겟은 `D1`입니다.

**예시**
```
ARM 1
AMC ON
D1 TAKEOFF
D2 TAKEOFF
D3 TAKEOFF
D4 TAKEOFF
D1 UP 120 1000
D2 DOWN 120 1000
EMERGENCY
AMC OFF
ARM 0
```

### 개발 환경
1. Arduino IDE 2.3.x
2. Board: Raspberry Pi Pico / RP2040
3. USB Stack: Pico SDK (Default)
4. Baudrate: 115200

### 필요한 라이브러리
Controller (AR1000X):
1. RF24 by TMRh20

Drone (AF1000X):
1. RF24 by TMRh20
2. VL53L1X by Pololu
3. ICM45686 library (`ICM45686.h` 제공)
4. Adafruit NeoPixel (WS2812C LED, GPIO17)

Note: `Wire`, `SPI`, `EEPROM`, `Preferences`는 보드 코어에서 제공됩니다.

### 라이브러리 링크 & 버전(테스트)
Controller (AR1000X):
1. RF24 by TMRh20 v1.5.0

Drone (AF1000X):
1. RF24 by TMRh20 v1.5.0
2. VL53L1X by Pololu v1.3.1
3. ICM45686 by TDK/Invensense v1.0.6
4. Adafruit NeoPixel v1.15.2

Links:
```
RF24: https://nRF24.github.io/RF24/
VL53L1X: https://github.com/pololu/vl53l1x-arduino
ICM45686: https://github.com/tdk-invn-oss/motion.arduino.ICM45686
Adafruit NeoPixel: https://github.com/adafruit/Adafruit_NeoPixel
```

Note: 위 버전은 현재 개발 환경 기준이며, Library Manager 최신 버전도 사용 가능합니다.

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
SYUBEA Co., Ltd. (Korea)  
구매 문의: `hello@1510.co.kr`

---

## 日本語
### AF1000X ドローンコントローラ兼 PC ハブ送信機

### 概要
**AR1000X Remote Controller** は RP2040 ベースの送信機で、**PC ハブ**として動作します。  
PC の USB シリアル命令を RF 制御信号に変換してドローンへ送信します。

- PC(USB Serial) -> AF1000X Controller(nRF24) -> Drone
- ドローン側ファームウェアの変更不要
- 教育・研究・自動化向け

### 主な機能
1. 物理ジョイスティック操作 (MODE 1/2 対応、既定 MODE 2)
2. PC シリアル命令制御 (Hub Mode)
3. ARM 安全ゲート + 200ms PC タイムアウト
4. 緊急スロットルカット
5. バッテリー電圧照会
6. 固有 TX ID を EEPROM に保存 (起動時 GPIO10 押下で再生成)
7. `DBUG` コマンド: 1秒周期でスティック/バッテリー/状態を出力
8. USB デバイス名: `SYUBEA AF1000X Controller`
9. GPIO12 ユーザーボタン: 短押し=ドローン LED 色変更(WS2812C, GPIO17)、長押し(1.5秒)=サーボ切替(GPIO36, 0~180 deg, 900~2000us)
10. 群飛行/AMC モード: 1台のコントローラで最大4台(D1~D4)制御

### ハードウェア
1. MCU: RP2040
2. RF: nRF24L01+
3. USB: RP2040 Native USB (CDC)
4. 入力: ジョイスティック, ボタン
5. 出力: RF パケット

### PC ハブ構成
```
PC Script / Application
          -> USB Serial (115200)
     AF1000X Controller
          -> nRF24
            Drone
```

コントローラは **安全ゲート**として動作し、PC からの危険な入力をそのまま通しません。

### シリアルコマンド
- `AR1000X_Serial_Command_Reference.md`
- `AR1000X_Controller_Operation.md` (ピン/LED/ボタン)

### LED 動作ルール
1. ブートシーケンス: LED1 -> LED2 -> LED3、各0.5秒 (合計1.5秒)
2. 低電圧: LED1が0.5秒点滅、LED2/LED3 OFF
3. ドローンとバインド完了: LED1 常時点灯
4. 未接続: LED1 1秒点滅
5. ヘッドレス ON: LED2 常時点灯
6. トリムモード ON: LED3 常時点灯 (AMC優先)
7. AMC モード ON: LED3 1秒ON / 1秒OFF

### 群飛行 / AMC (最大4台)
AMC モードで複数ドローンを制御します。  
`D1`~`D4` を付けると指定ドローンのみに適用されます。既定ターゲットは `D1` です。

**例**
```
ARM 1
AMC ON
D1 TAKEOFF
D2 TAKEOFF
D3 TAKEOFF
D4 TAKEOFF
D1 UP 120 1000
D2 DOWN 120 1000
EMERGENCY
AMC OFF
ARM 0
```

### 開発環境
1. Arduino IDE 2.3.x
2. Board: Raspberry Pi Pico / RP2040
3. USB Stack: Pico SDK (Default)
4. Baudrate: 115200

### 必要ライブラリ
Controller (AR1000X):
1. RF24 by TMRh20

Drone (AF1000X):
1. RF24 by TMRh20
2. VL53L1X by Pololu
3. ICM45686 library (`ICM45686.h` 提供)
4. Adafruit NeoPixel (WS2812C LED, GPIO17)

Note: `Wire`, `SPI`, `EEPROM`, `Preferences` はボードコアに含まれます。

### ライブラリリンク & バージョン(テスト)
Controller (AR1000X):
1. RF24 by TMRh20 v1.5.0

Drone (AF1000X):
1. RF24 by TMRh20 v1.5.0
2. VL53L1X by Pololu v1.3.1
3. ICM45686 by TDK/Invensense v1.0.6
4. Adafruit NeoPixel v1.15.2

Links:
```
RF24: https://nRF24.github.io/RF24/
VL53L1X: https://github.com/pololu/vl53l1x-arduino
ICM45686: https://github.com/tdk-invn-oss/motion.arduino.ICM45686
Adafruit NeoPixel: https://github.com/adafruit/Adafruit_NeoPixel
```

Note: 上記は現在の開発環境のバージョンです。Library Manager の最新版も使用可能です。

重要:  
外部 TinyUSB ライブラリがあると競合する可能性があります。  
以下のパスのライブラリは削除/無効化してください。
```
Documents/Arduino/libraries/Adafruit_TinyUSB_Library
```

### 基本使用手順
1. USB 接続後、ポートを確認
2. PC から `ARM 1` を送信
3. 移動/制御コマンドを実行
4. 必要時 `EMERGENCY`
5. 終了前に `ARM 0` を送信

### お問い合わせ
SYUBEA Co., Ltd. (Korea)  
購入問い合わせ: `hello@1510.co.kr`
