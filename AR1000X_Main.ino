/*
AR1000X Transmitter Firmware (RP2040) - FIXED (TX_ADDR macro conflict)
=====================================================================
WHY IT FAILED
-------------
RF24 library's nRF24L01.h defines a macro named TX_ADDR (a register address).
Your sketch also used TX_ADDR as a variable name, causing a macro collision.

FIX
---
Rename the 5-byte pipe address variables:
  TX_ADDR   -> PIPE_ADDR_TX
  PAIR_ADDR -> PIPE_ADDR_PAIR

LED BEHAVIOR (as requested)
---------------------------
LED1 (GPIO25):
 - Power on (initial, not bound yet): blink every 1s
 - Binding completed / normal operation: solid ON
 - Low battery: blink fast every 0.5s

LED2 (GPIO23):
 - Headless mode ON: blink every 1s
 - Headless OFF: OFF

LED3 (GPIO24):
 - Trim adjust mode active: blink every 0.5s
 - Normal: OFF

PC HUB MODE (added)
-------------------
When connected to PC over USB, this firmware can accept serial commands and forward them to the drone via nRF24.

- Serial: 115200 bps, newline/CR terminated.
- Safety: PC control is ignored unless you ARM it.
- Failsafe: if JOY packets stop for >200ms, controller falls back to physical sticks.

Commands:
  ARM 1            -> enable PC override (required before JOY is accepted)
  ARM 0            -> disable PC override
  JOY t r p y a1 a2 spd   (all integers)
     t/r/p/y/a1/a2: 0..255, spd: 1..3
  EMERGENCY        -> immediately disables PC override and sends throttle=0 once

Optional:
  SRC PC           -> force PC override (same as ARM 1)
  SRC STICK        -> force stick control (same as ARM 0)

BOOT MODE
---------
GPIO14 held LOW during power-on -> MODE 1
(default is MODE 2)

BUTTON SUMMARY
--------------
GPIO15 : boot hold = pairing / run = speed cycle
GPIO11 : short = auto takeoff / land (AUX1 pulse)
GPIO13 : headless mode toggle (AUX2 level)
GPIO14 : trim mode toggle (run), boot-hold = MODE 1 select

-------------------------------------------------------------
FULL SOURCE CODE
-------------------------------------------------------------
*/
#if defined(USE_TINYUSB)
#  include <Adafruit_TinyUSB.h>  // for TinyUSBDevice (Adafruit stack)
#else
#  include <USB.h>              // for USB class (Pico SDK stack)
#endif
#include <Arduino.h>
#include <cstdio>
#include <SPI.h>
#include <RF24.h>

// ============ RF Pins ============
#define RF_CE      6
#define RF_CSN     5

// ============ MUX ============
#define MUX_S0     22
#define MUX_S1     21
#define MUX_S2     20
#define MUX_ADC    29

// ============ ADC ============
#define ADC_THROTTLE  26
#define ADC_YAW       27
#define ADC_PITCH     28

// ============ Buttons (active LOW) ============
#define BTN_PAIR_SPEED 15
#define BTN_AUTO       11
#define BTN_TRIM_MODE  14
#define BTN_HEADLESS   13

// ============ LEDs (user specified) ============
#define LED1_POWER    25  // LED1
#define LED2_HEADLESS 23  // LED2
#define LED3_TRIM     24  // LED3

// ============ RF Pipe Addresses (RENAMED to avoid macro collision) ============
static const uint8_t PIPE_ADDR_TX[5]   = {0xB1,0xB2,0xB3,0xB4,0x01};
static const uint8_t PIPE_ADDR_PAIR[5] = {0xE8,0xE8,0xF0,0xF0,0xE1};

RF24 radio(RF_CE, RF_CSN);

// Must match drone side struct layout exactly
struct Signal {
  uint8_t throttle, roll, pitch, yaw, aux1, aux2;
  uint8_t speed;
};

Signal tx;

// ===== PC Hub (Serial -> RF) =====
// ===== Global status (must be declared before use) =====
uint8_t speedMode = 1;      // 1..3
bool headlessOn = false;    // AUX2 flag
float vbat = 4.2f;          // updated in loop via readBattery()


static bool pcArmed = false;
static bool pcOverride = false;
static uint32_t lastPcMs = 0;
static const uint32_t PC_TIMEOUT_MS = 200;

static Signal pcSig = {0};
static Signal stickSig = {0};
static Signal outSig = {0};
static String pcLine;

// dd3-style string command hold state
static bool pcHoldActive = false;
static uint32_t pcHoldUntil = 0;
static uint8_t pcHoldNeutralThrottle = 0;
static uint16_t pcLastPower = 0;
static uint16_t pcLastTimeMs = 0;


static void applyEmergency(){
  pcArmed = false;
  pcOverride = false;
  pcSig = {};
  pcSig.throttle = 0;
  pcSig.speed = 1;
}


static int powerToDelta(int p){
  // dd3 power range: 0..255 (direct)
  if(p < 0) p = -p;
  // map 0..255 -> 0..120 (delta added/subtracted around center 128)
  int d = map(constrain(p, 0, 255), 0, 255, 0, 120);
  return constrain(d, 0, 127);
}

static void setNeutral(Signal &s, uint8_t thr){
  s.throttle = thr;
  s.roll = 127;
  s.pitch = 127;
  s.yaw = 127;
  // aux1/aux2 keep as-is unless command changes them
}

static void startHold(uint32_t now, uint32_t durMs){
  pcHoldActive = true;
  pcHoldUntil = now + durMs;
  lastPcMs = now;       // keep alive
  pcOverride = true;
}

static void handlePcLine(String s){
  s.trim();
  if(!s.length()) return;

  // dd3 sometimes sends lowercase like "emergency"
  String u = s;
  u.trim();
  u.toUpperCase();

  // ===== Immediate safety =====
  if(u == "EMERGENCY"){
    applyEmergency();
    pcHoldActive = false;
    // Best-effort immediate packet with throttle 0
    outSig = pcSig;
    radio.write(&outSig, sizeof(outSig));
    Serial.println("OK");
    return;
  }

  // ===== ARM gating for PC control =====
  if(u.startsWith("ARM ")){
    int v = u.substring(4).toInt();
    pcArmed = (v != 0);
    if(!pcArmed){
      pcOverride = false;
      pcHoldActive = false;
    }
    Serial.println("OK");
    return;
  }

  // Force source (optional)
  if(u.startsWith("SRC ")){
    String src = u.substring(4);
    src.trim();
    if(src == "PC"){
      pcArmed = true;
      pcOverride = true;
      lastPcMs = millis();
    } else if(src == "STICK"){
      pcOverride = false;
      pcArmed = false;
      pcHoldActive = false;
    }
    Serial.println("OK");
    return;
  }

  // ===== Telemetry query =====
  if(u == "BATTERY?" || u == "BATTERY" || u == "BAT?" || u == "BAT"){
    Serial.print("BAT ");
    Serial.println(vbat, 2);
    return;
  }

  // ===== dd3: speed N =====
  if(u.startsWith("SPEED ")){
    int sp = u.substring(6).toInt();
    sp = constrain(sp, 1, 3);
    speedMode = (uint8_t)sp;
    pcSig.speed = (uint8_t)sp;
    Serial.println("OK");
    return;
  }

  // ===== dd3: start/stop/takeoff/land/headless/gyroreset/funled =====
  if(u == "TAKEOFF" || u == "LAND" || u == "START"){
    if(!pcArmed){ Serial.println("IGN"); return; }
    // Use AUX1 pulse (same behavior as button auto takeoff/land)
    pcSig.speed = speedMode;
    pcSig.aux2 = headlessOn ? 1 : 0;
    setNeutral(pcSig, stickSig.throttle);
    pcSig.aux1 = 1;
    startHold(millis(), 180); // ~180ms pulse
    Serial.println("OK");
    return;
  }

  if(u == "STOP"){
    // Treat STOP as emergency throttle-cut (safer)
    applyEmergency();
    pcHoldActive = false;
    outSig = pcSig;
    radio.write(&outSig, sizeof(outSig));
    Serial.println("OK");
    return;
  }

  if(u == "HEADLESS"){
    headlessOn = !headlessOn;
    pcSig.aux2 = headlessOn ? 1 : 0;
    Serial.println("OK");
    return;
  }

  if(u == "GYRORESET" || u == "GYRO_RESET"){
    // Not defined in this protocol; acknowledge to keep dd3 UI happy
    Serial.println("OK");
    return;
  }

  if(u == "FUNLED" || u == "FUN_LED"){
    Serial.println("OK");
    return;
  }

  if(u == "HOVER"){
    pcHoldActive = false;
    pcOverride = false;
    Serial.println("OK");
    return;
  }

  // ===== JOY direct (still supported) =====
  if(u.startsWith("JOY ")){
    if(!pcArmed){ Serial.println("IGN"); return; }

    int t,r,p,y,a1,a2,spd;
    int n = sscanf(u.c_str(), "JOY %d %d %d %d %d %d %d", &t,&r,&p,&y,&a1,&a2,&spd);
    if(n == 7){
      pcSig.throttle = (uint8_t)constrain(t,  0, 255);
      pcSig.roll     = (uint8_t)constrain(r,  0, 255);
      pcSig.pitch    = (uint8_t)constrain(p,  0, 255);
      pcSig.yaw      = (uint8_t)constrain(y,  0, 255);
      pcSig.aux1     = (uint8_t)constrain(a1, 0, 255);
      pcSig.aux2     = (uint8_t)constrain(a2, 0, 255);
      int sm = constrain(spd, 1, 3);
      pcSig.speed = (uint8_t)sm;

      lastPcMs = millis();
      pcOverride = true;
      pcHoldActive = false; // JOY should be streamed continuously
      Serial.println("OK");
      return;
    }
    Serial.println("ERR");
    return;
  }

  // ===== dd3: move commands: "<cmd> <power> <time_ms>" =====
  // cmd: UP/DOWN/FORWARD/BACK/LEFT/RIGHT/CW/CCW
  // Example: "forward 180 500" (power 0..255)
  {
    // tokenize quickly
    int sp1 = u.indexOf(' ');
    if(sp1 > 0){
      String cmd = u.substring(0, sp1);
      String rest = u.substring(sp1 + 1);
      rest.trim();
      int sp2 = rest.indexOf(' ');
      if(sp2 > 0){
        int pwr = rest.substring(0, sp2).toInt();
        int tms = rest.substring(sp2 + 1).toInt();
        if(tms < 0) tms = -tms;
        tms = constrain(tms, 20, 15000);

        if(!pcArmed){ Serial.println("IGN"); return; }

        int d = powerToDelta(pwr);
        pcLastPower = (uint16_t)pwr;
        pcLastTimeMs = (uint16_t)tms;

        // base throttle: keep current stick throttle
        uint8_t baseThr = stickSig.throttle;
        pcHoldNeutralThrottle = baseThr;

        pcSig.speed = speedMode;
        pcSig.aux1 = 0;
        pcSig.aux2 = headlessOn ? 1 : 0;

        // start from neutral
        setNeutral(pcSig, baseThr);

        if(cmd == "UP"){
          pcSig.throttle = (uint8_t)constrain((int)baseThr + d, 0, 255);
        } else if(cmd == "DOWN"){
          pcSig.throttle = (uint8_t)constrain((int)baseThr - d, 0, 255);
        } else if(cmd == "FORWARD"){
          pcSig.pitch = (uint8_t)constrain(127 + d, 0, 255);
        } else if(cmd == "BACK"){
          pcSig.pitch = (uint8_t)constrain(127 - d, 0, 255);
        } else if(cmd == "RIGHT"){
          pcSig.roll = (uint8_t)constrain(127 + d, 0, 255);
        } else if(cmd == "LEFT"){
          pcSig.roll = (uint8_t)constrain(127 - d, 0, 255);
        } else if(cmd == "CW"){
          pcSig.yaw = (uint8_t)constrain(127 + d, 0, 255);
        } else if(cmd == "CCW"){
          pcSig.yaw = (uint8_t)constrain(127 - d, 0, 255);
        } else {
          Serial.println("ERR");
          return;
        }

        startHold(millis(), (uint32_t)tms);
        Serial.println("OK");
        return;
      }
    }
  }

  Serial.println("ERR");
}


static void pollPcSerial(){
  while(Serial.available()){
    char c = (char)Serial.read();
    if(c == '\r' || c == '\n'){
      if(pcLine.length()){
        handlePcLine(pcLine);
        pcLine = "";
      }
    } else {
      if(pcLine.length() < 96) pcLine += c;
    }
  }
}


// ===== Modes =====
enum { MODE2 = 2, MODE1 = 1 };
uint8_t controlMode = MODE2;

// ===== State =====
bool trimMode = false;
bool boundOK = false;

// battery (TX side)
uint32_t lastBatMs = 0;
const float VBAT_LOW = 3.5f;   // adjust if needed

// trim offsets (RAM only -> resets on power cycle)
int trimRoll = 0;
int trimPitch = 0;

// aux1 pulse (takeoff/land trigger)
uint32_t aux1Until = 0;
const uint32_t AUX_PULSE = 200;

// ===== Helpers =====
static inline bool btn(int p){ return digitalRead(p) == LOW; }

static inline int readMux(uint8_t ch){
  digitalWrite(MUX_S0, bitRead(ch,0));
  digitalWrite(MUX_S1, bitRead(ch,1));
  digitalWrite(MUX_S2, bitRead(ch,2));
  delayMicroseconds(5);
  return analogRead(MUX_ADC);
}

static inline uint8_t mapAxis(int v){
  int m = map(v, 0, 4095, 0, 255);
  if(m > 120 && m < 135) return 127; // deadzone around center
  return (uint8_t)constrain(m, 0, 255);
}

static inline float readBattery(){
  // Battery via mux ch0, divider 1/2 assumed
  int raw = readMux(0);
  return (raw / 4095.0f) * 3.3f * 2.0f;
}

// ===== Pairing =====
static void runPairing(){
  radio.stopListening();
  radio.openWritingPipe(PIPE_ADDR_PAIR);

  uint32_t t0 = millis();
  while(millis() - t0 < 2000){
    // Send our TX pipe address (5 bytes) to the drone in pairing window
    radio.write(PIPE_ADDR_TX, 5);
    // during pairing, blink LED1 fast-ish
    digitalWrite(LED1_POWER, (millis()/250)%2);
    delay(10);
  }

  radio.openWritingPipe(PIPE_ADDR_TX);
}

// ===== LED handling =====
static void updateLEDs(){
  uint32_t t = millis();

  // LED1: power / bind / battery
  if(vbat < VBAT_LOW){
    digitalWrite(LED1_POWER, (t/250)%2);   // 0.5s blink (fast)
  } else if(!boundOK){
    digitalWrite(LED1_POWER, (t/500)%2);   // 1s blink
  } else {
    digitalWrite(LED1_POWER, HIGH);        // solid ON
  }

  // LED2: headless
  if(headlessOn){
    digitalWrite(LED2_HEADLESS, (t/500)%2); // 1s blink
  } else {
    digitalWrite(LED2_HEADLESS, LOW);
  }

  // LED3: trim mode
  if(trimMode){
    digitalWrite(LED3_TRIM, (t/250)%2);     // 0.5s blink
  } else {
    digitalWrite(LED3_TRIM, LOW);
  }
}

// ===== Setup =====
void setup(){
  // Serial over USB (CDC)
  Serial.begin(115200);  // Set USB descriptor strings so PC can identify this device.
  // NOTE: API differs depending on Tools->USB Stack.
#if defined(USE_TINYUSB)
  // Adafruit TinyUSB stack
  TinyUSBDevice.setManufacturerDescriptor("SYUBEA");
  TinyUSBDevice.setProductDescriptor("SYUBEA AF1000X Controller");
#else
  // Default Pico SDK USB stack (Arduino-Pico)
  USB.disconnect();
  USB.setManufacturer("SYUBEA");
  USB.setProduct("SYUBEA AF1000X Controller");
  USB.connect();
  delay(50);
#endif
  pinMode(MUX_S0, OUTPUT);
  pinMode(MUX_S1, OUTPUT);
  pinMode(MUX_S2, OUTPUT);

  pinMode(BTN_PAIR_SPEED, INPUT_PULLUP);
  pinMode(BTN_AUTO, INPUT_PULLUP);
  pinMode(BTN_TRIM_MODE, INPUT_PULLUP);
  pinMode(BTN_HEADLESS, INPUT_PULLUP);

  pinMode(LED1_POWER, OUTPUT);
  pinMode(LED2_HEADLESS, OUTPUT);
  pinMode(LED3_TRIM, OUTPUT);

  // Boot mode select (GPIO14 held at power on)
  controlMode = btn(BTN_TRIM_MODE) ? MODE1 : MODE2;

  // RF init
  radio.begin();
  radio.setChannel(76);
  radio.setDataRate(RF24_1MBPS);
  radio.setPALevel(RF24_PA_LOW);
  radio.openWritingPipe(PIPE_ADDR_TX);

  // Pairing on boot (GPIO15 held)
  if(btn(BTN_PAIR_SPEED)){
    runPairing();
    // Pairing itself doesn't guarantee bind, but usually you want to show "trying"
    // boundOK will become true once writes succeed in loop.
  }

  stickSig = {};
  stickSig.speed = speedMode;
  pcSig = {};
  pcSig.speed = 1;

  // show initial LEDs
  updateLEDs();
}

// ===== Loop =====
void loop(){
  uint32_t now = millis();

  // --- PC Serial poll ---
  pollPcSerial();

  // PC hold active (dd3 move/pulse) keeps PC override alive until pcHoldUntil
  if(pcHoldActive){
    if(now < pcHoldUntil){
      lastPcMs = now; // keep alive
      pcOverride = true;
    } else {
      pcHoldActive = false;
      pcOverride = false;
    }
  }

  // PC timeout -> fall back to sticks (for streamed JOY mode)
  if(pcOverride && (now - lastPcMs > PC_TIMEOUT_MS)){
    pcOverride = false;
  }

  // battery update every 1s
  if(now - lastBatMs > 1000){
    lastBatMs = now;
    vbat = readBattery();
  }

  // -----------------------------
  // Read physical sticks -> stickSig
  // -----------------------------
  int rawThr = analogRead(ADC_THROTTLE);
  int rawYaw = analogRead(ADC_YAW);
  int rawPit = analogRead(ADC_PITCH);
  int rawRol = readMux(1);

  uint8_t aThr = mapAxis(rawThr);
  uint8_t aYaw = mapAxis(rawYaw);
  uint8_t aPit = mapAxis(rawPit);
  uint8_t aRol = mapAxis(rawRol);

  // mode mapping
  if(controlMode == MODE2){
    stickSig.throttle = aThr;
    stickSig.yaw      = aYaw;
    stickSig.pitch    = aPit;
    stickSig.roll     = aRol;
  } else { // MODE1
    stickSig.throttle = aPit;
    stickSig.yaw      = aYaw;
    stickSig.pitch    = aThr;
    stickSig.roll     = aRol;
  }

  // speed cycle (GPIO15 short press) - only affects stick control
  static bool lastSpd = false;
  bool spd = btn(BTN_PAIR_SPEED);
  if(!lastSpd && spd){
    speedMode++;
    if(speedMode > 3) speedMode = 1;
  }
  lastSpd = spd;
  stickSig.speed = speedMode;

  // auto takeoff/land (GPIO11 short press -> AUX1 pulse)
  static bool lastAuto = false;
  bool a = btn(BTN_AUTO);
  if(!lastAuto && a){
    aux1Until = now + AUX_PULSE;
  }
  lastAuto = a;
  stickSig.aux1 = (now < aux1Until) ? 1 : 0;

  // headless toggle (GPIO13) - only affects stick control
  static bool lastHead = false;
  bool h = btn(BTN_HEADLESS);
  if(!lastHead && h){
    headlessOn = !headlessOn;
  }
  lastHead = h;
  stickSig.aux2 = headlessOn ? 1 : 0;

  // trim mode toggle (GPIO14 short press)
  // 1st press: enter trim mode and capture offsets
  // 2nd press: exit trim mode (offsets kept until power off)
  static bool lastTrim = false;
  bool t = btn(BTN_TRIM_MODE);
  if(!lastTrim && t){
    if(!trimMode){
      trimRoll  = 127 - (int)stickSig.roll;
      trimPitch = 127 - (int)stickSig.pitch;
      trimRoll  = constrain(trimRoll,  -30, 30);
      trimPitch = constrain(trimPitch, -30, 30);
      trimMode = true;
    } else {
      trimMode = false;
    }
  }
  lastTrim = t;

  // apply trim offsets only to stick control path
  stickSig.roll  = (uint8_t)constrain((int)stickSig.roll  + trimRoll,  0, 255);
  stickSig.pitch = (uint8_t)constrain((int)stickSig.pitch + trimPitch, 0, 255);

  // -----------------------------
  // Select output (PC override vs stick)
  // -----------------------------
  if(pcOverride){
    outSig = pcSig;
  } else {
    outSig = stickSig;
  }

  // send
  bool ok = radio.write(&outSig, sizeof(outSig));
  if(ok) boundOK = true;

  // keep LEDs reflecting headless/trim state from physical controller
  updateLEDs();
  delay(20);
}
