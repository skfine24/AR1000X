# AR1000X Serial Command Reference (Controller)

KO: SYUBEA AF1000X 조종기 PC Hub(Serial → RF) 명령
EN: SYUBEA AF1000X controller PC hub (Serial → RF) commands

KO: Baudrate = `115200`, Line ending = `\r` or `\n`, 대소문자 구분 없음
EN: Baudrate = `115200`, Line ending = `\r` or `\n`, case-insensitive

---

## KO: 1) 제어/안전 | EN: Control/Safety

| Command | KO | EN |
|---|---|---|
| `ARM 1` | PC 제어 활성 | Enable PC control |
| `ARM 0` | PC 제어 해제(스틱 우선) | Disable PC control (stick priority) |
| `SRC PC` | PC 입력 강제 | Force PC input |
| `SRC STICK` | 스틱 입력 강제 | Force stick input |
| `EMERGENCY` | 즉시 모터 컷 + 전송 | Immediate throttle cut + transmit |
| `STOP` | `EMERGENCY`와 동일 | Same as `EMERGENCY` |

KO: PC 명령이 200ms 이상 없으면 스틱으로 자동 복귀
EN: If PC commands stop for >200ms, fallback to sticks

---

## KO: 2) 바인딩 | EN: Binding

| Command | KO | EN |
|---|---|---|
| `BIND` | 스캔 후 페어링 전송 | Scan then transmit pairing payload |
| `PAIR` | `BIND`와 동일 | Same as `BIND` |

---

## KO: 3) 조이스틱 직접 제어 | EN: Raw Joystick

**Format**
```
JOY throttle roll pitch yaw aux1 aux2 speed
```

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
