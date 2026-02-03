/*
KO: AR1000X 조종기 펌웨어 (RP2040)
EN: AR1000X transmitter firmware (RP2040)

KO: 핀/LED/버튼/PC 명령은 AR1000X_Controller_Operation.md 참고
EN: See AR1000X_Controller_Operation.md for pins/LEDs/buttons/PC commands
*/

#if defined(USE_TINYUSB)
// KO: TinyUSBDevice(Adafruit 스택) 사용 시 아래 include 주석 해제
// EN: For TinyUSBDevice (Adafruit stack). Uncomment include below if needed
#else
#  include <USB.h>              // KO: USB 클래스(Pico SDK 스택) / EN: USB class (Pico SDK stack)
#endif
#include <Arduino.h>
#include <cstdio>
#include <SPI.h>
#include <RF24.h>
#include <EEPROM.h>

// ============================================================================
// KO: RF 핀
// EN: RF pins
// ============================================================================
#define RF_CE      6
#define RF_CSN     5

// ============================================================================
// KO: MUX 핀
// EN: MUX pins
// ============================================================================
#define MUX_S0     22
#define MUX_S1     21
#define MUX_S2     20
#define MUX_ADC    29

// ============================================================================
// KO: ADC 핀
// EN: ADC pins
// ============================================================================
#define ADC_THROTTLE  26
#define ADC_YAW       27
#define ADC_PITCH     28

// ============================================================================
// KO: 버튼(LOW 활성)
// EN: Buttons (active LOW)
// ============================================================================
#define BTN_PAIR_SPEED 15
#define BTN_AUTO       11
#define BTN_TRIM_MODE  14
#define BTN_HEADLESS   13
#define BTN_FLOW_TOGGLE 10

// ============================================================================
// KO: LED 핀(사용자 지정)
// EN: LED pins (user specified)
// ============================================================================
#define LED1_POWER    25  // KO: LED1 / EN: LED1
#define LED2_HEADLESS 23  // KO: LED2 / EN: LED2
#define LED3_TRIM     24  // KO: LED3 / EN: LED3

// ============================================================================
// KO: RF 파이프 주소(매크로 충돌 방지 위해 이름 변경)
// EN: RF pipe addresses (renamed to avoid macro collision)
// ============================================================================
static uint8_t PIPE_ADDR_TX[5]   = {0xB1,0xB2,0xB3,0xB4,0x01};
static const uint8_t PIPE_ADDR_PAIR[5] = {0xE8,0xE8,0xF0,0xF0,0xE1};

static const uint8_t PAIR_CHANNEL = 76;

// KO: FHSS 기본 홉 테이블(부팅 스캔으로 대체)
// EN: FHSS default hop table (replaced by boot scan)
static const uint8_t HOP_MAX = 12;
static uint8_t hopTable[HOP_MAX] = {63,64,65,66,67,68,69,70,71,72,73,74};
static uint8_t hopLen = HOP_MAX;
static uint8_t nextHopTable[HOP_MAX] = {0};
static uint8_t nextHopLen = 0;
static bool nextHopValid = false;
static bool pendingHopSave = false;
static bool linkReady = false;
static uint32_t lastLinkTxMs = 0;
static const uint32_t HOP_SLOT_MS = 20;

// KO: 드론 쪽 구조체 레이아웃과 정확히 일치해야 함 / EN: Must match drone-side struct layout exactly
struct Signal {
  uint8_t throttle, roll, pitch, yaw, aux1, aux2;
  uint8_t speed;
  uint8_t hop; // KO: FHSS 홉 인덱스 / EN: FHSS hop index
};

struct LinkPayload {
  char name[8];
  uint8_t addr[5];
  uint8_t hopLen;
  uint8_t hopTable[HOP_MAX];
  uint8_t flags;
  uint8_t crc;
};

// KO: 스캔 설정(부팅/페어링 전용)
// EN: Scan settings (boot/pairing only)
static const uint8_t SCAN_MIN_CH = 5;  // KO: 2405 MHz / EN: 2405 MHz
static const uint8_t SCAN_MAX_CH = 80; // KO: 2480 MHz / EN: 2480 MHz
static const uint8_t SCAN_SAMPLES = 4;

static const char CTRL_NAME[8] = "AF1000X"; // KO: 8바이트, 널 문자 필요 없음 / EN: 8 bytes, no null required
static const char CTRL_VERSION[4] = "00a";
static const uint8_t LINK_FLAG_FACTORY_RESET = 0x01;
static bool factoryResetReq = false;
static const uint8_t AUX2_HEADLESS = 0x01;
static const uint8_t AUX2_FLOW     = 0x02;

static const uint32_t EEPROM_MAGIC = 0xA0F1A0F1;
static const int EEPROM_SIZE = 64;
static const uint32_t EEPROM_ADDR_MAGIC = 0xA0F1B0B1;
static const int EEPROM_ADDR_OFFSET = 32;


RF24 radio(RF_CE, RF_CSN);

static uint8_t hopSeed = 1;
static uint32_t hopStartMs = 0;
static uint8_t hopIdx = 0;
static uint8_t hopIdxLast = 0xFF;

static inline uint8_t hopIndexForMs(uint32_t now){
  if(HOP_SLOT_MS == 0 || hopLen == 0) return 0;
  uint32_t slots = (now - hopStartMs) / HOP_SLOT_MS;
  return (uint8_t)((slots + hopSeed) % hopLen);
}

static inline void fhssTxUpdate(uint32_t now){
  if(hopLen == 0){
    radio.setChannel(PAIR_CHANNEL);
    hopIdx = 0;
    hopIdxLast = 0xFF;
    return;
  }
  uint8_t idx = hopIndexForMs(now);
  if(idx != hopIdxLast){
    hopIdxLast = idx;
    radio.setChannel(hopTable[idx]);
  }
  hopIdx = idx;
}

struct HopStore {
  uint32_t magic;
  uint8_t len;
  uint8_t table[HOP_MAX];
  uint8_t crc;
};

struct AddrStore {
  uint32_t magic;
  uint8_t addr[5];
  uint8_t crc;
};

static bool eepromReady = false;

static inline uint8_t hopCrc(const uint8_t* t, uint8_t len){
  uint8_t c = 0x5A;
  for(uint8_t i = 0; i < len; i++){
    c ^= (uint8_t)(t[i] + (uint8_t)(i * 17));
  }
  return c;
}

static bool eepromInit(){
  if(eepromReady) return true;
  EEPROM.begin(EEPROM_SIZE);
  eepromReady = true;
  return true;
}

static bool loadHopTableFromEeprom(){
  if(!eepromInit()) return false;
  HopStore s = {};
  EEPROM.get(0, s);
  if(s.magic != EEPROM_MAGIC) return false;
  if(s.len < 1 || s.len > HOP_MAX) return false;
  for(uint8_t i = 0; i < s.len; i++){
    if(s.table[i] < SCAN_MIN_CH || s.table[i] > SCAN_MAX_CH) return false;
  }
  if(s.crc != hopCrc(s.table, s.len)) return false;
  hopLen = s.len;
  for(uint8_t i = 0; i < hopLen; i++) hopTable[i] = s.table[i];
  return true;
}

static void saveHopTableToEeprom(){
  if(!eepromInit()) return;
  HopStore s = {};
  s.magic = EEPROM_MAGIC;
  s.len = hopLen;
  for(uint8_t i = 0; i < HOP_MAX; i++) s.table[i] = (i < hopLen) ? hopTable[i] : 0;
  s.crc = hopCrc(s.table, s.len);
  EEPROM.put(0, s);
  EEPROM.commit();
}

static inline uint8_t addrCrc(const uint8_t* a){
  uint8_t c = 0x3D;
  for(uint8_t i = 0; i < 5; i++){
    c ^= (uint8_t)(a[i] + (i * 11));
  }
  return c;
}

static bool loadTxAddrFromEeprom(){
  if(!eepromInit()) return false;
  AddrStore s = {};
  EEPROM.get(EEPROM_ADDR_OFFSET, s);
  if(s.magic != EEPROM_ADDR_MAGIC) return false;
  if(s.crc != addrCrc(s.addr)) return false;
  uint8_t orv = s.addr[0] | s.addr[1] | s.addr[2] | s.addr[3] | s.addr[4];
  uint8_t andv = s.addr[0] & s.addr[1] & s.addr[2] & s.addr[3] & s.addr[4];
  if(orv == 0x00 || andv == 0xFF) return false;
  for(uint8_t i = 0; i < 5; i++) PIPE_ADDR_TX[i] = s.addr[i];
  return true;
}

static void saveTxAddrToEeprom(){
  if(!eepromInit()) return;
  AddrStore s = {};
  s.magic = EEPROM_ADDR_MAGIC;
  for(uint8_t i = 0; i < 5; i++) s.addr[i] = PIPE_ADDR_TX[i];
  s.crc = addrCrc(s.addr);
  EEPROM.put(EEPROM_ADDR_OFFSET, s);
  EEPROM.commit();
}

static void printTxAddr(){
  Serial.print("ID ");
  for(uint8_t i = 0; i < 5; i++){
    if(i) Serial.print("-");
    if(PIPE_ADDR_TX[i] < 16) Serial.print("0");
    Serial.print(PIPE_ADDR_TX[i], HEX);
  }
  Serial.println();
}

static void printDebugLine(){
  Serial.print("DBG ");
  Serial.print("T:"); Serial.print((int)stickSig.throttle);
  Serial.print(" R:"); Serial.print((int)stickSig.roll);
  Serial.print(" P:"); Serial.print((int)stickSig.pitch);
  Serial.print(" Y:"); Serial.print((int)stickSig.yaw);
  Serial.print(" SPD:"); Serial.print((int)stickSig.speed);
  Serial.print(" H:"); Serial.print(headlessOn ? 1 : 0);
  Serial.print(" F:"); Serial.print(flowOn ? 1 : 0);
  Serial.print(" VBAT:"); Serial.print(vbat, 2); Serial.print("V");
  Serial.print(" MODE:"); Serial.print(controlMode == MODE1 ? 1 : 2);
  Serial.print(" LINK:"); Serial.print(linkReady ? 1 : 0);
  Serial.print(" PC:"); Serial.print(pcOverride ? 1 : 0);
  Serial.println();
}

static void printBanner(){
  Serial.println("SYUBEA Co., LTD");
  Serial.println("www.1510.co.kr");
  Serial.print("Model : AR1000X Controller - Ver.");
  Serial.println(CTRL_VERSION);
  Serial.println();
  printTxAddr();
}

static void generateTxAddr(){
#if defined(ARDUINO_ARCH_RP2040)
  uint64_t chip = rp2040.getChipID();
  for(uint8_t i = 0; i < 5; i++){
    PIPE_ADDR_TX[i] = (uint8_t)(chip >> (i * 8));
  }
#else
  uint32_t seed = micros();
  seed ^= (uint32_t)analogRead(ADC_THROTTLE) << 16;
  seed ^= (uint32_t)analogRead(ADC_YAW) << 8;
  seed ^= (uint32_t)analogRead(ADC_PITCH);
  randomSeed(seed);
  for(uint8_t i = 0; i < 5; i++){
    PIPE_ADDR_TX[i] = (uint8_t)random(1, 255);
  }
#endif
  uint8_t orv = PIPE_ADDR_TX[0] | PIPE_ADDR_TX[1] | PIPE_ADDR_TX[2] | PIPE_ADDR_TX[3] | PIPE_ADDR_TX[4];
  uint8_t andv = PIPE_ADDR_TX[0] & PIPE_ADDR_TX[1] & PIPE_ADDR_TX[2] & PIPE_ADDR_TX[3] & PIPE_ADDR_TX[4];
  if(orv == 0x00 || andv == 0xFF){
    PIPE_ADDR_TX[0] ^= 0xA5;
  }
}

static void initTxAddr(){
  if(loadTxAddrFromEeprom()) return;
  generateTxAddr();
  saveTxAddrToEeprom();
}

static inline uint8_t linkCrc(const LinkPayload &p){
  uint8_t c = 0x6A;
  for(uint8_t i = 0; i < 8; i++) c ^= (uint8_t)(p.name[i] + (i * 13));
  for(uint8_t i = 0; i < 5; i++) c ^= (uint8_t)(p.addr[i] + (i * 7));
  c ^= p.hopLen;
  for(uint8_t i = 0; i < p.hopLen && i < HOP_MAX; i++) c ^= (uint8_t)(p.hopTable[i] + (i * 11));
  c ^= p.flags;
  return c;
}

static inline void buildLinkPayload(LinkPayload &p, const uint8_t* table, uint8_t len){
  for(uint8_t i = 0; i < 8; i++) p.name[i] = CTRL_NAME[i];
  for(uint8_t i = 0; i < 5; i++) p.addr[i] = PIPE_ADDR_TX[i];
  p.hopLen = len;
  for(uint8_t i = 0; i < HOP_MAX; i++) p.hopTable[i] = (i < len) ? table[i] : 0;
  p.flags = factoryResetReq ? LINK_FLAG_FACTORY_RESET : 0;
  p.crc = linkCrc(p);
}

static void scanAndBuildHopTable(uint8_t* outTable, uint8_t &outLen){
  // KO: 전체 채널 RPD 간단 스캔 후 간섭이 적은 채널 선택
  // EN: Simple RPD scan across all channels; pick lowest-energy channels
  uint8_t scores[126] = {0};

  radio.stopListening();
  radio.startListening();

  for(uint8_t ch = SCAN_MIN_CH; ch <= SCAN_MAX_CH; ch++){
    uint8_t hits = 0;
    for(uint8_t i = 0; i < SCAN_SAMPLES; i++){
      radio.setChannel(ch);
      delayMicroseconds(200);
      if(radio.testRPD()) hits++;
    }
    scores[ch] = hits;
  }

  radio.stopListening();

  bool used[126] = {false};
  outLen = HOP_MAX;
  for(uint8_t i = 0; i < outLen; i++){
    uint8_t bestCh = SCAN_MIN_CH;
    uint8_t bestScore = 255;
    for(uint8_t ch = SCAN_MIN_CH; ch <= SCAN_MAX_CH; ch++){
      if(!used[ch] && scores[ch] < bestScore){
        bestScore = scores[ch];
        bestCh = ch;
      }
    }
    outTable[i] = bestCh;
    used[bestCh] = true;
  }
}

static void applyNextHopIfReady(uint32_t now){
  if(!nextHopValid) return;
  hopLen = nextHopLen;
  for(uint8_t i = 0; i < hopLen; i++) hopTable[i] = nextHopTable[i];
  for(uint8_t i = hopLen; i < HOP_MAX; i++) hopTable[i] = 0;
  nextHopValid = false;
  pendingHopSave = true;
  hopStartMs = now;
  hopIdxLast = 0xFF;
}

static bool sendLinkSetup(uint32_t now){
  if(now - lastLinkTxMs < 500) return false; // KO: 스로틀(백그라운드) / EN: throttle (background)
  lastLinkTxMs = now;

  radio.stopListening();
  radio.setChannel(PAIR_CHANNEL);
  radio.openWritingPipe(PIPE_ADDR_TX);

  LinkPayload payload = {};
  const uint8_t* table = nextHopValid ? nextHopTable : hopTable;
  uint8_t len = nextHopValid ? nextHopLen : hopLen;
  buildLinkPayload(payload, table, len);

  bool ok = radio.write(&payload, sizeof(payload));
  if(ok){
    if(nextHopValid){
      applyNextHopIfReady(now);
    }
    if(factoryResetReq){
      factoryResetReq = false;
    }
    linkReady = true;
  }
  return ok;
}

Signal tx;

// ============================================================================
// KO: PC 허브(Serial → RF)
// EN: PC hub (Serial -> RF)
// ============================================================================
uint8_t speedMode = 1;      // KO: 1~3 / EN: 1..3
bool headlessOn = false;    // KO: AUX2 플래그 / EN: AUX2 flag
float vbat = 4.2f;          // KO: 루프에서 readBattery()로 갱신 / EN: updated in loop via readBattery()
bool stickActive = false;   // KO: 물리 스틱이 활성일 때 true / EN: true when physical sticks are active


static bool pcArmed = false;
static bool pcOverride = false;
static uint32_t lastPcMs = 0;
static const uint32_t PC_TIMEOUT_MS = 200;

static Signal pcSig = {0};
static Signal stickSig = {0};
static Signal outSig = {0};
static String pcLine;

// ============================================================================
// KO: 상태 / EN: State
// ============================================================================
bool trimMode = false;
bool boundOK = false;
bool emergencyPulse = false;
static bool killHoldActive = false;
static bool killTriggered = false;
static uint32_t killHoldStart = 0;
static bool flowOn = true;
static bool debugEnabled = false;
static uint32_t debugNextMs = 0;

// KO: dd3 스타일 문자열 명령 홀드 상태
// EN: dd3-style string command hold state
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
  // KO: dd3 power 범위 0..255(직접) / EN: dd3 power range 0..255 (direct)
  if(p < 0) p = -p;
  // KO: 0..255를 0..120으로 매핑(중앙 128 기준 델타)
  // EN: map 0..255 -> 0..120 (delta around center 128)
  int d = map(constrain(p, 0, 255), 0, 255, 0, 120);
  return constrain(d, 0, 127);
}

static void setNeutral(Signal &s, uint8_t thr){
  s.throttle = thr;
  s.roll = 127;
  s.pitch = 127;
  s.yaw = 127;
  // KO: aux1/aux2는 명령이 바꾸지 않으면 유지 / EN: aux1/aux2 keep as-is unless command changes them
}

static void startHold(uint32_t now, uint32_t durMs){
  pcHoldActive = true;
  pcHoldUntil = now + durMs;
  lastPcMs = now;       // KO: 유지(keep alive) / EN: keep alive
  pcOverride = true;
}

static void handlePcLine(String s){
  s.trim();
  if(!s.length()) return;

  // KO: dd3가 "emergency"처럼 소문자로 보낼 때가 있음
  // EN: dd3 sometimes sends lowercase like "emergency"
  String u = s;
  u.trim();
  u.toUpperCase();

  // KO: FLOW 토글(PC 시리얼)
  // EN: FLOW toggle (PC serial)
  if(u == "FLOW ON" || u == "FLOW 1"){
    flowOn = true;
    Serial.println("OK");
    return;
  }
  if(u == "FLOW OFF" || u == "FLOW 0"){
    flowOn = false;
    Serial.println("OK");
    return;
  }

  // KO: 수동 바인딩(스캔 + 페어링 페이로드)
  // EN: Manual binding (scan + pairing payload)
  if(u == "BIND" || u == "PAIR"){
    scanAndBuildHopTable(hopTable, hopLen);
    nextHopValid = false;
    bool pairedOk = runPairing();
    boundOK = false;
    linkReady = pairedOk;
    pendingHopSave = true;
    Serial.println("OK");
    return;
  }

  // KO: 디버그 출력 토글(1초마다 출력)
  // EN: Debug output toggle (prints every 1s)
  if(u == "DBUG"){
    debugEnabled = !debugEnabled;
    debugNextMs = millis() + 1000;
    Serial.println(debugEnabled ? "DBUG ON" : "DBUG OFF");
    if(debugEnabled){
      printDebugLine();
    }
    return;
  }

  // KO: 조종기 ID 출력
  // EN: Print controller ID
  if(u == "ID?" || u == "TXID?" || u == "ADDR?"){
    printTxAddr();
    return;
  }

  if(stickActive){
    Serial.println("IGN");
    return;
  }

  // ==========================================================================
  // KO: 즉시 안전 처리
  // EN: Immediate safety
  // ==========================================================================
  if(u == "EMERGENCY"){
    applyEmergency();
    pcHoldActive = false;
    // KO: 스로틀 0 즉시 패킷(가능한 한)
    // EN: Best-effort immediate packet with throttle 0
    outSig = pcSig;
    fhssTxUpdate(millis());
    outSig.hop = hopIdx;
    radio.write(&outSig, sizeof(outSig));
    Serial.println("OK");
    return;
  }

  // ==========================================================================
  // KO: PC 제어 ARM 게이팅 / EN: ARM gating for PC control
  // ==========================================================================
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

  // KO: 입력 소스 강제(옵션)
  // EN: Force source (optional)
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

  // ==========================================================================
  // KO: 텔레메트리 조회
  // EN: Telemetry query
  // ==========================================================================
  if(u == "BATTERY?" || u == "BATTERY" || u == "BAT?" || u == "BAT"){
    Serial.print("BAT ");
    Serial.println(vbat, 2);
    return;
  }

  // ==========================================================================
  // KO: dd3 속도 N
  // EN: dd3 speed N
  // ==========================================================================
  if(u.startsWith("SPEED ")){
    int sp = u.substring(6).toInt();
    sp = constrain(sp, 1, 3);
    speedMode = (uint8_t)sp;
    pcSig.speed = (uint8_t)sp;
    Serial.println("OK");
    return;
  }

  // ==========================================================================
  // KO: dd3 start/stop/takeoff/land/headless/gyroreset/funled
  // EN: dd3 start/stop/takeoff/land/headless/gyroreset/funled
  // ==========================================================================
  if(u == "TAKEOFF" || u == "LAND" || u == "START"){
    if(!pcArmed){ Serial.println("IGN"); return; }
    // KO: AUX1 펄스 사용(버튼 자동 이륙/착륙과 동일)
    // EN: Use AUX1 pulse (same behavior as button auto takeoff/land)
    pcSig.speed = speedMode;
    pcSig.aux2 = headlessOn ? 1 : 0;
    setNeutral(pcSig, stickSig.throttle);
    pcSig.aux1 = 1;
    startHold(millis(), 180); // KO: ~180ms 펄스 / EN: ~180ms pulse
    Serial.println("OK");
    return;
  }

  if(u == "STOP"){
    // KO: STOP을 비상 스로틀 컷으로 처리(더 안전)
    // EN: Treat STOP as emergency throttle-cut (safer)
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
    // KO: 이 프로토콜에 정의되지 않음; dd3 UI 유지를 위해 OK 응답
    // EN: Not defined in this protocol; acknowledge to keep dd3 UI happy
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

  // ==========================================================================
  // KO: JOY 직접 입력(계속 지원)
  // EN: JOY direct (still supported)
  // ==========================================================================
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
      pcHoldActive = false; // KO: JOY는 연속 스트림 전송 필요 / EN: JOY should be streamed continuously
      Serial.println("OK");
      return;
    }
    Serial.println("ERR");
    return;
  }

  // ==========================================================================
  // KO: dd3 이동 명령 "<cmd> <power> <time_ms>"
  // EN: dd3 move commands "<cmd> <power> <time_ms>"
  // KO: cmd=UP/DOWN/FORWARD/BACK/LEFT/RIGHT/CW/CCW
  // EN: cmd=UP/DOWN/FORWARD/BACK/LEFT/RIGHT/CW/CCW
  // KO: 예: "forward 180 500" (power 0..255)
  // EN: Example "forward 180 500" (power 0..255)
  // ==========================================================================
  {
    // KO: 빠르게 토큰 분리 / EN: tokenize quickly
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

        // KO: 기준 스로틀 = 현재 스틱 스로틀 / EN: base throttle = current stick throttle
        uint8_t baseThr = stickSig.throttle;
        pcHoldNeutralThrottle = baseThr;

        pcSig.speed = speedMode;
        pcSig.aux1 = 0;
        pcSig.aux2 = headlessOn ? 1 : 0;

        // KO: 중립에서 시작 / EN: start from neutral
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


// ============================================================================
// KO: 모드
// EN: Modes
// ============================================================================
enum { MODE2 = 2, MODE1 = 1 };
uint8_t controlMode = MODE2;

// KO: 배터리(TX 측)
// EN: Battery (TX side)
uint32_t lastBatMs = 0;
const float VBAT_LOW = 3.5f;   // KO: 필요 시 조정 / EN: adjust if needed
const uint8_t STICK_CENTER = 127;
const uint8_t STICK_THROTTLE_ACTIVE = 15; // KO: 필요 시 조정 / EN: adjust if needed

// KO: 트림 오프셋(RAM만, 전원 사이클 시 리셋)
// EN: Trim offsets (RAM only, reset on power cycle)
int trimRoll = 0;
int trimPitch = 0;

// KO: AUX1 펄스(이륙/착륙 트리거)
// EN: AUX1 pulse (takeoff/land trigger)
uint32_t aux1Until = 0;
const uint32_t AUX_PULSE = 200;
const uint32_t AUTO_LONG_MS = 2000;
const uint32_t GYRO_RESET_PULSE_MS = 300;
const uint32_t STICK_KILL_HOLD_MS = 2000;
const uint8_t  STICK_KILL_LOW = 30;
const uint8_t  STICK_KILL_HIGH = 225;
uint32_t gyroResetUntil = 0;

// ============================================================================
// KO: 헬퍼
// EN: Helpers
// ============================================================================
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
  if(m > 120 && m < 135) return 127; // KO: 중앙 데드존 / EN: deadzone around center
  return (uint8_t)constrain(m, 0, 255);
}

static inline float readBattery(){
  // KO: MUX ch0 배터리 입력(1/2 분압 가정)
  // EN: Battery via mux ch0, divider 1/2 assumed
  int raw = readMux(0);
  return (raw / 4095.0f) * 3.3f * 2.0f;
}

// ============================================================================
// KO: 페어링 / EN: Pairing
// ============================================================================
static bool runPairing(){
  radio.stopListening();
  radio.setChannel(PAIR_CHANNEL);
  radio.openWritingPipe(PIPE_ADDR_PAIR);

  LinkPayload payload = {};
  const uint8_t* table = nextHopValid ? nextHopTable : hopTable;
  uint8_t len = nextHopValid ? nextHopLen : hopLen;
  buildLinkPayload(payload, table, len);

  bool pairedOk = false;
  uint32_t t0 = millis();
  while(millis() - t0 < 2000){
    // KO: 페어링 페이로드 전송(name + addr + hop table)
    // EN: Send pairing payload (name + addr + hop table)
    if(radio.write(&payload, sizeof(payload))){
      pairedOk = true;
    }
    // KO: 페어링 중 LED1 빠르게 점멸
    // EN: during pairing, blink LED1 fast-ish
    digitalWrite(LED1_POWER, (millis()/250)%2);
    delay(10);
  }

  radio.openWritingPipe(PIPE_ADDR_TX);
  if(pairedOk && nextHopValid){
    applyNextHopIfReady(millis());
  }
  if(pairedOk && factoryResetReq){
    factoryResetReq = false;
  }
  return pairedOk;
}

// ============================================================================
// KO: LED 처리
// EN: LED handling
// ============================================================================
static void updateLEDs(){
  uint32_t t = millis();

  // KO: LED1 = 전원/바인딩/배터리 / EN: LED1 = power/bind/battery
  if(vbat < VBAT_LOW){
    digitalWrite(LED1_POWER, (t/250)%2);   // KO: 0.5초 점멸 / EN: 0.5s blink
  } else if(!boundOK){
    digitalWrite(LED1_POWER, (t/500)%2);   // KO: 1초 점멸 / EN: 1s blink
  } else {
    digitalWrite(LED1_POWER, HIGH);        // KO: 고정 ON / EN: solid ON
  }

  // KO: LED2 = 헤드리스
  // EN: LED2 = headless
  if(headlessOn){
    digitalWrite(LED2_HEADLESS, (t/500)%2); // KO: 1초 점멸 / EN: 1s blink
  } else {
    digitalWrite(LED2_HEADLESS, LOW);
  }

  // KO: LED3 = 트림 모드
  // EN: LED3 = trim mode
  if(trimMode){
    digitalWrite(LED3_TRIM, (t/250)%2);     // KO: 0.5초 점멸 / EN: 0.5s blink
  } else {
    digitalWrite(LED3_TRIM, LOW);
  }
}

// ============================================================================
// KO: 셋업 / EN: Setup
// ============================================================================
void setup(){
  // KO: USB 시리얼(CDC) / EN: Serial over USB (CDC)
  Serial.begin(115200);  // KO: USB 디스크립터 설정 / EN: set USB descriptors
  // KO: Tools->USB Stack 설정에 따라 API가 다름
  // EN: API differs depending on Tools->USB Stack
#if defined(USE_TINYUSB)
  // KO: Adafruit TinyUSB 스택 / EN: Adafruit TinyUSB stack
  TinyUSBDevice.setManufacturerDescriptor("SYUBEA");
  TinyUSBDevice.setProductDescriptor("SYUBEA AF1000X Controller");
#else
  // KO: 기본 Pico SDK USB 스택(Arduino-Pico)
  // EN: Default Pico SDK USB stack (Arduino-Pico)
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
  pinMode(BTN_FLOW_TOGGLE, INPUT_PULLUP);

  pinMode(LED1_POWER, OUTPUT);
  pinMode(LED2_HEADLESS, OUTPUT);
  pinMode(LED3_TRIM, OUTPUT);

  // KO: 부팅 시 공장초기화 요청(GPIO11 누름)
  // EN: Factory reset request on boot (GPIO11 held)
  factoryResetReq = btn(BTN_AUTO);

  // KO: 부트 모드 선택(GPIO14 전원 켤 때 누름)
  // EN: Boot mode select (GPIO14 held at power on)
  controlMode = btn(BTN_TRIM_MODE) ? MODE1 : MODE2;

  // KO: 고유 TX 주소 자동 생성/로드(EEPROM 저장)
  // EN: Auto-generate/load unique TX address (stored in EEPROM)
  // KO: 부팅 시 GPIO10(Flow 버튼) 홀드하면 ID 재생성
  // EN: Hold GPIO10 (Flow button) on boot to regenerate ID
  if(btn(BTN_FLOW_TOGGLE)){
    generateTxAddr();
    saveTxAddrToEeprom();
  } else {
    initTxAddr();
  }
  printBanner();

  // KO: RF 초기화 / EN: RF init
  radio.begin();
  radio.setChannel(PAIR_CHANNEL);
  radio.setDataRate(RF24_250KBPS);
  radio.setPALevel(RF24_PA_LOW);
  radio.openWritingPipe(PIPE_ADDR_TX);

  bool loaded = loadHopTableFromEeprom();
  if(!loaded){
    scanAndBuildHopTable(hopTable, hopLen);
    nextHopValid = false;
  } else {
    scanAndBuildHopTable(nextHopTable, nextHopLen);
    nextHopValid = true;
  }

  hopSeed = PIPE_ADDR_TX[4];
  if(hopSeed == 0) hopSeed = 1;
  hopStartMs = millis();
  hopIdxLast = 0xFF;

  // KO: 부팅 시 페어링(GPIO15 누름)
  // EN: Pairing on boot (GPIO15 held)
  if(btn(BTN_PAIR_SPEED)){
    scanAndBuildHopTable(hopTable, hopLen);
    nextHopValid = false;
    bool pairedOk = runPairing();
    linkReady = pairedOk;
    pendingHopSave = true;
    // KO: 페어링만으로 바인딩 보장 X; 쓰기 성공 후 boundOK
    // EN: Pairing alone doesn't guarantee bind; boundOK after successful writes
  }

  stickSig = {};
  stickSig.speed = speedMode;
  pcSig = {};
  pcSig.speed = 1;

  // KO: 초기 LED 표시 / EN: show initial LEDs
  updateLEDs();
}

// ============================================================================
// KO: 루프
// EN: Loop
// ============================================================================
void loop(){
  uint32_t now = millis();


  // ==========================================================================
  // KO: 물리 스틱 읽기 → stickSig
  // EN: Read physical sticks -> stickSig
  // ==========================================================================
  int rawThr = analogRead(ADC_THROTTLE);
  int rawYaw = analogRead(ADC_YAW);
  int rawPit = analogRead(ADC_PITCH);
  int rawRol = readMux(1);

  uint8_t aThr = mapAxis(rawThr);
  uint8_t aYaw = mapAxis(rawYaw);
  uint8_t aPit = mapAxis(rawPit);
  uint8_t aRol = mapAxis(rawRol);

  // KO: 비상 모터 컷(스틱 콤보 2초 홀드)
  // EN: Emergency motor cut (stick combo hold 2s)
  bool leftDown = (aThr <= STICK_KILL_LOW);
  bool rightDown = (aPit <= STICK_KILL_LOW);
  bool leftLeft = (aYaw <= STICK_KILL_LOW);
  bool leftRight = (aYaw >= STICK_KILL_HIGH);
  bool rightLeft = (aRol <= STICK_KILL_LOW);
  bool rightRight = (aRol >= STICK_KILL_HIGH);

  bool comboA = leftDown && leftLeft  && rightDown && rightRight; // KO: 왼쪽 7시, 오른쪽 5시 / EN: left 7 o'clock, right 5 o'clock
  bool comboB = leftDown && leftRight && rightDown && rightLeft;  // KO: 왼쪽 5시, 오른쪽 7시 / EN: left 5 o'clock, right 7 o'clock
  bool combo = comboA || comboB;

  if(combo){
    if(!killHoldActive && !killTriggered){
      killHoldActive = true;
      killHoldStart = now;
    }
    if(killHoldActive && !killTriggered && (now - killHoldStart >= STICK_KILL_HOLD_MS)){
      emergencyPulse = true;
      applyEmergency();
      pcHoldActive = false;
      pcOverride = false;
      aux1Until = 0;
      gyroResetUntil = 0;
      killTriggered = true;
      killHoldActive = false;
    }
  } else {
    killHoldActive = false;
    killTriggered = false;
  }

  // KO: 모드 매핑 / EN: mode mapping
  if(controlMode == MODE2){
    stickSig.throttle = aThr;
    stickSig.yaw      = aYaw;
    stickSig.pitch    = aPit;
    stickSig.roll     = aRol;
  } else { // KO: MODE1 / EN: MODE1
    stickSig.throttle = aPit;
    stickSig.yaw      = aYaw;
    stickSig.pitch    = aThr;
    stickSig.roll     = aRol;
  }

  // KO: 속도 순환(GPIO15 짧은 클릭, 스틱 전용)
  // EN: speed cycle (GPIO15 short press, stick only)
  static bool lastSpd = false;
  bool spd = btn(BTN_PAIR_SPEED);
  if(!lastSpd && spd){
    speedMode++;
    if(speedMode > 3) speedMode = 1;
  }
  lastSpd = spd;
  stickSig.speed = speedMode;

  // KO: 자동 이륙/착륙(GPIO11 짧은 클릭 → AUX1 펄스)
  // EN: auto takeoff/land (GPIO11 short press -> AUX1 pulse)
  // KO: 롱프레스(>=2초) → 자이로 초기화 요청
  // EN: long press (>=2s) -> gyro init request
  static bool lastAuto = false;
  static uint32_t autoPressedAt = 0;
  static bool autoLongFired = false;
  bool a = btn(BTN_AUTO) && !factoryResetReq;
  if(a && !lastAuto){
    autoPressedAt = now;
    autoLongFired = false;
  }
  if(a && !autoLongFired && (now - autoPressedAt >= AUTO_LONG_MS)){
    autoLongFired = true;
    gyroResetUntil = now + GYRO_RESET_PULSE_MS;
    aux1Until = 0;
    pcHoldActive = false;
    pcOverride = false;
  }
  if(!a && lastAuto){
    if(!autoLongFired){
      aux1Until = now + AUX_PULSE;
    }
  }
  lastAuto = a;
  if(now < gyroResetUntil){
    stickSig.aux1 = 2; // KO: 자이로 초기화 요청 / EN: gyro init request
  } else {
    stickSig.aux1 = (now < aux1Until) ? 1 : 0;
  }

  // KO: 헤드리스 토글(GPIO13, 스틱 전용)
  // EN: headless toggle (GPIO13, stick only)
  static bool lastHead = false;
  bool h = btn(BTN_HEADLESS);
  if(!lastHead && h){
    headlessOn = !headlessOn;
    pcSig.aux2 = headlessOn ? AUX2_HEADLESS : 0;
  }
  lastHead = h;
  stickSig.aux2 = headlessOn ? AUX2_HEADLESS : 0;

  // KO: 옵티컬 플로우 토글(GPIO10)
  // EN: Optical flow toggle (GPIO10)
  static bool lastFlow = false;
  bool f = btn(BTN_FLOW_TOGGLE);
  if(!lastFlow && f){
    flowOn = !flowOn;
  }
  lastFlow = f;

  // KO: 트림 모드 토글(GPIO14 짧은 클릭)
  // EN: trim mode toggle (GPIO14 short press)
  // KO: 1회=진입+캡처, 2회=종료(전원 끌 때까지 유지)
  // EN: 1st=enter+capture, 2nd=exit (kept until power off)
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

  // KO: 스틱 입력 감지(트림 적용 전)
  // EN: detect stick activity (before trim offsets)
  stickActive = (stickSig.roll != STICK_CENTER) ||
                (stickSig.pitch != STICK_CENTER) ||
                (stickSig.yaw != STICK_CENTER) ||
                (stickSig.throttle > STICK_THROTTLE_ACTIVE);
  if(stickActive){
    pcOverride = false;
    pcHoldActive = false;
  }

  // KO: 스틱 제어 경로에만 트림 오프셋 적용
  // EN: apply trim offsets only to stick control path
  stickSig.roll  = (uint8_t)constrain((int)stickSig.roll  + trimRoll,  0, 255);
  stickSig.pitch = (uint8_t)constrain((int)stickSig.pitch + trimPitch, 0, 255);

  // KO: PC 시리얼 폴링
  // EN: PC serial poll
  pollPcSerial();

  // KO: dd3 홀드가 pcHoldUntil까지 PC 오버라이드 유지
  // EN: dd3 hold keeps PC override alive until pcHoldUntil
  if(pcHoldActive){
    if(now < pcHoldUntil){
      lastPcMs = now; // KO: 유지(keep alive) / EN: keep alive
      pcOverride = true;
    } else {
      pcHoldActive = false;
      pcOverride = false;
    }
  }

  // KO: PC 타임아웃 → 스틱으로 복귀(JOY 스트림용)
  // EN: PC timeout -> fall back to sticks (for streamed JOY mode)
  if(pcOverride && (now - lastPcMs > PC_TIMEOUT_MS)){
    pcOverride = false;
  }

  // KO: 배터리 1초 주기 갱신
  // EN: battery update every 1s
  if(now - lastBatMs > 1000){
    lastBatMs = now;
    vbat = readBattery();
  }

  if(debugEnabled && (int32_t)(now - debugNextMs) >= 0){
    debugNextMs = now + 1000;
    printDebugLine();
  }

  // KO: 링크 설정(제어 전송과 동시에 백그라운드)
  // EN: Link setup (background while still sending control)
  if(!linkReady){
    sendLinkSetup(now);
  }

  // ==========================================================================
  // KO: 출력 선택(PC 오버라이드 vs 스틱)
  // EN: Select output (PC override vs stick)
  // ==========================================================================
  if(pcOverride){
    outSig = pcSig;
  } else {
    outSig = stickSig;
  }
  // KO: AUX2 비트필드(bit0=headless, bit1=flow)
  // EN: AUX2 bitfield (bit0=headless, bit1=flow)
  outSig.aux2 = (outSig.aux2 & AUX2_HEADLESS) | (flowOn ? AUX2_FLOW : 0);
  if(emergencyPulse){
    outSig = {};
    outSig.throttle = 0;
    outSig.roll = 127;
    outSig.pitch = 127;
    outSig.yaw = 127;
    outSig.aux1 = 0;
    outSig.aux2 = 0;
    outSig.speed = 1;
  }
  if(!emergencyPulse && (now < gyroResetUntil)){
    outSig.aux1 = 2; // KO: 자이로 초기화 요청 / EN: gyro init request
  }

  // KO: FHSS 채널 + 홉 인덱스 / EN: FHSS channel + hop index
  fhssTxUpdate(now);
  outSig.hop = hopIdx;

  // KO: 전송 / EN: send
  bool ok = radio.write(&outSig, sizeof(outSig));
  if(ok){
    boundOK = true;
    if(pendingHopSave){
      saveHopTableToEeprom();
      pendingHopSave = false;
    }
  }

  if(emergencyPulse) emergencyPulse = false;

  // KO: LED가 헤드리스/트림 상태 반영 유지
  // EN: keep LEDs reflecting headless/trim state
  updateLEDs();
  delay(20);
}
