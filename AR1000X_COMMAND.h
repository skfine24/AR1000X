#ifndef AR1000X_COMMAND_H
#define AR1000X_COMMAND_H

// KO: 시리얼 통신 명령어 정의
// EN: Serial communication command definitions

// 1. 제어/안전 (Control/Safety)
#define CMD_ARM         "ARM"
#define CMD_EMERGENCY   "EMERGENCY"
#define CMD_STOP        "STOP"
#define CMD_HOVER       "HOVER"

// 2. 자동 비행 (Auto Flight)
#define CMD_TAKEOFF     "TAKEOFF"
#define CMD_START       "START"
#define CMD_LAND        "LAND"
#define CMD_LANDING     "LANDING" // KO: 사용자 추가 요청 / EN: User requested alias

// 3. 모드/설정 (Mode/Settings)
#define CMD_AMC         "AMC"
#define CMD_FLOW        "FLOW"
#define CMD_HEADLESS    "HEADLESS"
#define CMD_SPEED       "SPEED"
#define CMD_JOY         "JOY"
#define CMD_SRC         "SRC"

// 4. 바인딩 (Binding)
#define CMD_BIND        "BIND"
#define CMD_PAIR        "PAIR"

// 5. 상태/정보 (Telemetry/Info)
#define CMD_BATTERY     "BATTERY"
#define CMD_BAT         "BAT"
#define CMD_DBUG        "DBUG"
#define CMD_ID          "ID"
#define CMD_TXID        "TXID"
#define CMD_ADDR        "ADDR"

// 6. 이동 명령 (Movement - dd3 style)
#define CMD_UP          "UP"
#define CMD_DOWN        "DOWN"
#define CMD_FORWARD     "FORWARD"
#define CMD_BACK        "BACK"
#define CMD_LEFT        "LEFT"
#define CMD_RIGHT       "RIGHT"
#define CMD_CW          "CW"
#define CMD_CCW         "CCW"
#define CMD_CC          "CC"  // KO: CCW 약어 추가 / EN: Add CCW alias
#define CMD_FLIP        "FLIP"

// 7. 호환성/기타 (Compatibility/Misc)
#define CMD_GYRORESET   "GYRORESET"
#define CMD_GYRO_RESET  "GYRO_RESET"
#define CMD_FUNLED      "FUNLED"
#define CMD_FUN_LED     "FUN_LED"

// 8. 직접 제어 (Direct Control - 0~255)
#define CMD_SET_THROTTLE "SET_THROTTLE"
#define CMD_SET_ROLL     "SET_ROLL"
#define CMD_SET_PITCH    "SET_PITCH"
#define CMD_SET_YAW      "SET_YAW"
#define CMD_MOVE         "MOVE"

#endif