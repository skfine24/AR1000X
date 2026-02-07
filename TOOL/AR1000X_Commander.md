# AR1000X Commander Command Reference / 명령어 사용법 / コマンドリファレンス

---

## [EN] English

### 1. Basic Syntax
Commands are case-insensitive.
```text
COMMAND [Power] [Duration(ms)]
```
*   **Command**: Action keyword (e.g., `UP`, `FORWARD`).
*   **Power**: 0 ~ 255 (Optional). Represents speed or intensity.
*   **Duration**: Time in milliseconds (Optional).
    *   Default: **500ms** (0.5s).
    *   `TAKEOFF`, `LAND` Default: **3000ms** (3.0s).

### 2. Flight Control Commands
| Command | Description | Example | Note |
| :--- | :--- | :--- | :--- |
| **START** | Arming (Start Motors) | `START` | Required before flight |
| **TAKEOFF** | Auto Takeoff | `TAKEOFF` | Hover at ~1m |
| **LAND** | Auto Landing | `LAND` | Land and stop motors |
| **UP** | Ascend (Throttle +) | `UP 150 1000` | |
| **DOWN** | Descend (Throttle -) | `DOWN 100 500` | |
| **FORWARD** | Move Forward (Pitch +) | `FORWARD 150 2000` | |
| **BACK** | Move Backward (Pitch -) | `BACK 150 1000` | |
| **LEFT** | Move Left (Roll -) | `LEFT 120 1000` | |
| **RIGHT** | Move Right (Roll +) | `RIGHT 120 1000` | |
| **CW** | Rotate Clockwise (Yaw +) | `CW 150 1000` | Turn Right |
| **CCW** | Rotate Counter-Clockwise (Yaw -)| `CCW 150 1000` | Turn Left |

### 3. Advanced & Safety
*   **EMERGENCY**: Immediately stops motors.
*   **MOVE t r p y**: Direct 4-channel control (Range: 0-255).
    *   Example: `MOVE 127 127 127 127` (Neutral)
*   **Protocol Commands**: `HEADLESS`, `SPEED`, `FLIP` (Sent directly to drone, no visual update).

---

## [KO] 한국어

### 1. 기본 문법
명령어는 대소문자를 구분하지 않습니다.
```text
명령어 [강도] [시간(ms)]
```
*   **명령어**: 동작을 지시하는 키워드 (예: `UP`, `FORWARD`)
*   **강도 (Power)**: 0 ~ 255 (생략 가능). 속도나 기울기 정도.
*   **시간 (Duration)**: 동작 유지 시간 (밀리초 단위, 생략 가능).
    *   기본값: **500ms** (0.5초).
    *   `TAKEOFF`, `LAND` 기본값: **3000ms** (3.0초).

### 2. 비행 제어 명령어
| 명령어 | 설명 | 사용 예시 | 비고 |
| :--- | :--- | :--- | :--- |
| **START** | 시동 (Arming) | `START` | 비행 전 필수 |
| **TAKEOFF** | 자동 이륙 | `TAKEOFF` | 약 1m 고도 유지 |
| **LAND** | 자동 착륙 | `LAND` | 착륙 후 모터 정지 |
| **UP** | 상승 (Throttle +) | `UP 150 1000` | |
| **DOWN** | 하강 (Throttle -) | `DOWN 100 500` | |
| **FORWARD** | 전진 (Pitch +) | `FORWARD 150 2000` | |
| **BACK** | 후진 (Pitch -) | `BACK 150 1000` | |
| **LEFT** | 좌측 이동 (Roll -) | `LEFT 120 1000` | |
| **RIGHT** | 우측 이동 (Roll +) | `RIGHT 120 1000` | |
| **CW** | 시계 방향 회전 (Yaw +) | `CW 150 1000` | 우회전 |
| **CCW** | 반시계 방향 회전 (Yaw -)| `CCW 150 1000` | 좌회전 |

### 3. 고급 및 안전
*   **EMERGENCY**: 모터 즉시 정지 (비상).
*   **MOVE t r p y**: 4채널 직접 제어 (범위: 0-255).
    *   예: `MOVE 127 127 127 127` (중립)
*   **프로토콜 명령어**: `HEADLESS`, `SPEED`, `FLIP` 등 (드론으로 전송되나 화면 게이지에는 반영 안 됨).

---

## [JA] 日本語

### 1. 基本構文
大文字・小文字は区別されません。
```text
コマンド [強度] [時間(ms)]
```
*   **コマンド**: 動作を指定するキーワード (例: `UP`, `FORWARD`)
*   **強度 (Power)**: 0 ~ 255 (省略可)。速度や傾きの強さ。
*   **時間 (Duration)**: 動作を維持する時間 (ミリ秒単位, 省略可)。
    *   デフォルト: **500ms** (0.5秒)。
    *   `TAKEOFF`, `LAND` デフォルト: **3000ms** (3.0秒)。

### 2. 飛行制御コマンド
| コマンド | 説明 | 使用例 | 備考 |
| :--- | :--- | :--- | :--- |
| **START** | 始動 (Arming) | `START` | 飛行前に必須 |
| **TAKEOFF** | 自動離陸 | `TAKEOFF` | 高度約1mでホバリング |
| **LAND** | 自動着陸 | `LAND` | 着陸後にモーター停止 |
| **UP** | 上昇 (Throttle +) | `UP 150 1000` | |
| **DOWN** | 下降 (Throttle -) | `DOWN 100 500` | |
| **FORWARD** | 前進 (Pitch +) | `FORWARD 150 2000` | |
| **BACK** | 後退 (Pitch -) | `BACK 150 1000` | |
| **LEFT** | 左移動 (Roll -) | `LEFT 120 1000` | |
| **RIGHT** | 右移動 (Roll +) | `RIGHT 120 1000` | |
| **CW** | 右回転 (Yaw +) | `CW 150 1000` | 時計回り |
| **CCW** | 左回転 (Yaw -)| `CCW 150 1000` | 反時計回り |

### 3. 高度な制御・安全
*   **EMERGENCY**: 緊急停止 (モーター即時停止)。
*   **MOVE t r p y**: 4チャンネル直接制御 (範囲: 0-255)。
    *   例: `MOVE 127 127 127 127` (ニュートラル)
*   **プロトコルコマンド**: `HEADLESS`, `SPEED`, `FLIP` など (ドローンには送信されますが、画面ゲージには反映されません)。