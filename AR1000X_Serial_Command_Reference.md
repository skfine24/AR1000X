# AR1000X Serial Command Reference (Controller)

KO: SYUBEA AF1000X 조종기 PC Hub(Serial → RF) 명령
EN: SYUBEA AF1000X controller PC hub (Serial → RF) commands

KO: Baudrate = `115200`, Line ending = `\r` or `\n`, 대소문자 구분 없음
EN: Baudrate = `115200`, Line ending = `\r` or `\n`, case-insensitive

---

## KO: 0) 대상 드론 지정 | EN: Target Drone (D1~D4)

KO: 명령 앞에 `D1`~`D4`를 붙이면 해당 드론에만 적용됩니다. 기본값은 `D1`.
EN: Prefix commands with `D1`~`D4` to target a specific drone. Default is `D1`.

**Format**
```
D1 <command>
D2:<command>
D3, <command>
D4 <command>
```

KO: `:` 또는 `,` 는 선택입니다.
EN: `:` or `,` is optional.

---

## KO: 1) 제어/안전 | EN: Control/Safety

| Command | KO | EN |
|---|---|---|
| `ARM 1` | PC 제어 활성 | Enable PC control |
| `ARM 0` | PC 제어 해제(스틱 우선) | Disable PC control (stick priority) |
| `SRC PC` | PC 입력 강제 | Force PC input |
| `SRC STICK` | 스틱 입력 강제 | Force stick input |
| `EMERGENCY` | 즉시 모터 컷 + **D1~D4 전체 전송** | Immediate throttle cut + **broadcast to D1~D4** |
| `STOP` | `EMERGENCY`와 동일 | Same as `EMERGENCY` |

KO: PC 명령이 200ms 이상 없으면 스틱으로 자동 복귀
EN: If PC commands stop for >200ms, fallback to sticks

---

## KO: 1-1) AMC 멀티제어 모드 | EN: AMC Multi-Control Mode

| Command | KO | EN |
|---|---|---|
| `AMC ON` | AMC 모드 진입 | Enter AMC mode |
| `AMC OFF` | AMC 모드 해제 | Exit AMC mode |

KO: AMC 모드에서는 조종기 스틱/버튼 제어는 무시되고, PC 명령으로만 제어합니다.  
KO: `D1~D4`가 붙은 명령은 해당 드론에만 적용됩니다.
EN: In AMC mode, stick/button inputs are ignored and only PC commands are applied.  
EN: Commands with `D1~D4` target the specified drone.

---

## KO: 2) 바인딩 | EN: Binding

| Command | KO | EN |
|---|---|---|
| `BIND` | 스캔 후 페어링 전송 (기본 D1) | Scan then transmit pairing payload (default D1) |
| `PAIR` | `BIND`와 동일 | Same as `BIND` |
| `BIND D2` | D2로 바인딩 | Bind to D2 |
| `D3 BIND` | D3로 바인딩 | Bind to D3 |

---

## KO: 3) 조이스틱 직접 제어 | EN: Raw Joystick

**Format**
```
JOY throttle roll pitch yaw aux1 aux2 speed
```

KO: AMC 모드에서는 `D1~D4`를 붙인 경우 해당 드론에만 적용됩니다.  
EN: In AMC mode, use `D1~D4` prefix to target a specific drone.

**Range**
| Field | KO | EN |
|---|---|---|
| `throttle` | 0~255 | 0~255 |
| `roll/pitch/yaw` | 0~255 (중앙 127) | 0~255 (center 127) |
| `aux1/aux2` | 0~255 | 0~255 |
| `speed` | 1~3 | 1~3 |

---

## KO: 4) dd3 상대 이동 명령 | EN: dd3 Relative Move

**Format**
```
<cmd> <power> <time_ms>
```

KO: AMC 모드에서는 `D1~D4`를 붙인 경우 해당 드론에만 적용됩니다.  
EN: In AMC mode, use `D1~D4` prefix to target a specific drone.

| Command | KO | EN |
|---|---|---|
| `FORWARD` | 전진 | Forward |
| `BACK` | 후진 | Back |
| `LEFT` | 좌 | Left |
| `RIGHT` | 우 | Right |
| `UP` | 상승 | Up |
| `DOWN` | 하강 | Down |
| `CW` | 시계 방향 회전 | Rotate CW |
| `CCW` | 반시계 방향 회전 | Rotate CCW |

---

## KO: 5) 속도/모드 | EN: Speed/Mode

| Command | KO | EN |
|---|---|---|
| `SPEED 1/2/3` | 속도 설정 | Set speed |
| `HEADLESS` | 헤드리스 토글 | Toggle headless |
| `HOVER` | PC 제어 종료 | Exit PC control |
| `FLOW ON` / `FLOW 1` | 옵티컬 플로우(Flow) ON | Optical flow ON |
| `FLOW OFF` / `FLOW 0` | 옵티컬 플로우(Flow) OFF | Optical flow OFF |

KO: `HEADLESS`, `FLOW`, `SPEED`는 전체에 적용됩니다.  
EN: `HEADLESS`, `FLOW`, and `SPEED` apply globally.

---

## KO: AUX2 비트 전송 위치 | EN: AUX2 Bit Tx Locations

KO: 조종기에서 AUX2 비트를 생성/전송하는 위치
EN: Where controller builds/transmits AUX2 bits
- `AR1000X_Main.ino` (AUX2 비트 조합/전송)

---

## KO: 6) 자동 이륙/착륙 | EN: Auto Takeoff/Land

| Command | KO | EN |
|---|---|---|
| `TAKEOFF` | 자동 이륙 | Auto takeoff |
| `LAND` | 자동 착륙 | Auto landing |
| `START` | `TAKEOFF`와 동일 | Same as `TAKEOFF` |

KO: 내부적으로 AUX1 펄스를 발생
EN: Internally generates an AUX1 pulse

---

## KO: 7) 상태 조회 | EN: Telemetry/Status

| Command | Response |
|---|---|
| `BATTERY?` | `BAT x.xx` |
| `BAT?` | `BAT x.xx` |
| `BAT` | `BAT x.xx` |
| `ID?` / `TXID?` / `ADDR?` | `ID XX-XX-XX-XX-XX` |

---

## KO: 8) 디버그 | EN: Debug

| Command | KO | EN |
|---|---|---|
| `DBUG` | 1초 간격 상태 출력 토글 | Toggle 1s status print |

**Output Example**
```
DBG T:123 R:127 P:130 Y:120 SPD:2 H:0 F:1 VBAT:4.11V MODE:2 LINK:1 PC:0
```

---

## KO: 9) 호환/더미 명령 | EN: Compatibility/Dummy

| Command | KO | EN |
|---|---|---|
| `GYRORESET` | `OK` 반환 | Returns `OK` |
| `GYRO_RESET` | `OK` 반환 | Returns `OK` |
| `FUNLED` | `OK` 반환 | Returns `OK` |
| `FUN_LED` | `OK` 반환 | Returns `OK` |

---

## KO: 10) 응답 코드 | EN: Response Codes

| Code | KO | EN |
|---|---|---|
| `OK` | 수락 | Accepted |
| `IGN` | 무시 (ARM 미활성) | Ignored (ARM not enabled) |
| `ERR` | 오류 | Error |

---

## KO: 11) 예제 (터미널/파이썬) | EN: Examples (Terminal/Python)

### KO: Python (pyserial)
### EN: Python (pyserial)

```python
import serial, time

ser = serial.Serial("COM5", 115200, timeout=0.2)
time.sleep(0.5)

def send(cmd):
    ser.write((cmd + "\n").encode())
    time.sleep(0.05)
    print(ser.read_all().decode(errors="ignore"))

send("ARM 1")
send("SPEED 2")
send("TAKEOFF")
send("FORWARD 120 500")
send("LAND")
send("ARM 0")
```

### KO: Windows PowerShell (pyserial miniterm)
### EN: Windows PowerShell (pyserial miniterm)

```powershell
python -m serial.tools.miniterm COM5 115200
```

KO: 연결 후 직접 명령어를 입력
EN: After connecting, type commands directly
