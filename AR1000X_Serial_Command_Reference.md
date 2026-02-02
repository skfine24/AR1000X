# 📡 AF1000X Serial Command Reference

**Device:** SYUBEA AF1000X Controller  
**Mode:** PC Hub Mode (Serial → RF)  
**Baudrate:** `115200`  
**Line Ending:** `\r` or `\n`  
**Case Sensitivity:** Not case-sensitive  

---

## 🔐 1. Control / Safety Commands

| Command | Description |
|------|------------|
| `ARM 1` | Enable PC hub control (**required**) |
| `ARM 0` | Disable PC control, return to physical sticks |
| `SRC PC` | Force PC input (debug/test) |
| `SRC STICK` | Force stick input |
| `EMERGENCY` | 🚨 Immediate throttle cut + RF transmit |
| `STOP` | Same as `EMERGENCY` |

**Notes**
- All movement commands are ignored unless `ARM 1` is active
- If PC commands stop for >200ms, controller automatically falls back to sticks (fail-safe)

---

## 🎮 2. Raw Joystick Control (Direct Channel Control)

### Format
```
JOY throttle roll pitch yaw aux1 aux2 speed
```

### Range
| Field | Range |
|----|----|
| throttle | 0–255 |
| roll / pitch / yaw | 0–255 (128 = neutral) |
| aux1 / aux2 | 0 or 1 |
| speed | 1–3 |

### Example
```
JOY 30 128 140 128 0 0 1
```

**Use Case**
- Real-time control
- Gamepad / joystick streaming from PC

---

## 🧭 3. dd3-Style Relative Motion Commands

### Format
```
<command> power time_ms
```

- `power`: `0–255`
- `time_ms`: duration in milliseconds

| Command | Action |
|------|-------|
| `FORWARD p t` | Move forward |
| `BACK p t` | Move backward |
| `LEFT p t` | Move left |
| `RIGHT p t` | Move right |
| `UP p t` | Ascend |
| `DOWN p t` | Descend |
| `CW p t` | Rotate clockwise |
| `CCW p t` | Rotate counter-clockwise |

### Example
```
FORWARD 120 500
CCW 80 300
UP 150 400
```

**Behavior**
- Motion is held for `time_ms`
- Automatically returns to neutral after completion
- Ideal for scripted flight sequences

---

## ⚙ 4. Speed / Flight Mode

| Command | Description |
|------|------------|
| `SPEED 1` | Low speed |
| `SPEED 2` | Medium speed |
| `SPEED 3` | High speed |
| `HEADLESS` | Toggle headless mode (AUX2) |
| `HOVER` | Exit PC control, return to sticks |

---

## 🚀 5. Automatic Functions (AUX1 Pulse)

| Command | Description |
|------|------------|
| `TAKEOFF` | Auto takeoff |
| `LAND` | Auto landing |
| `START` | Same behavior as TAKEOFF |

**Implementation**
- AUX1 pulse (~180 ms)

---

## 🔋 6. Telemetry / Status Query

| Command | Response |
|------|---------|
| `BATTERY?` | `BAT x.xx` (volts) |
| `BAT?` | Same as above |

### Example
```
BATTERY?
→ BAT 3.87
```

---

## 🧪 7. Compatibility / UI Support Commands

(Reserved for dd3 UI compatibility – acknowledged only)

| Command | Response |
|------|---------|
| `GYRORESET` | `OK` |
| `GYRO_RESET` | `OK` |
| `LED` | `OK` |

---

## 📤 8. Response Codes

| Response | Meaning |
|-------|--------|
| `OK` | Command accepted |
| `IGN` | Ignored (ARM not enabled) |
| `ERR` | Invalid or malformed command |

---

## ✅ Recommended Minimal Control Flow

```
ARM 1
SPEED 2
TAKEOFF
FORWARD 120 500
CCW 80 300
LAND
ARM 0
```

---

## 🧠 Design Philosophy

- **PC** acts as high-level controller / automation hub  
- **AF1000X Controller** acts as safety gate + RF transmitter  
- **Drone firmware remains unchanged**  
- Emergency command always has highest priority

---

© SYUBEA · AF1000X Controller
