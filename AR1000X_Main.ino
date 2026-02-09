/*
KO: AR1000X 조종기 펌웨어 (RP2040)
EN: AR1000X transmitter firmware (RP2040)

KO: 핀/LED/버튼/PC 명령은 AR1000X_Controller_Operation.md 참고
EN: See AR1000X_Controller_Operation.md for pins/LEDs/buttons/PC commands
*/

// KO/EN: Forward declarations to satisfy Arduino's auto-prototypes
struct Signal;
struct LinkPayload;

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
#include <Joystick.h> // KO: MHeironimus/ArduinoJoystick 라이브러리 필요 / EN: Requires MHeironimus/ArduinoJoystick library
#include "AR1000X_COMMAND.h" // KO: 명령어 정의 헤더 포함 / EN: Include command definitions

// KO: 조이스틱 설정 (8버튼, X/Y/Z/Rz 축)
// EN: Joystick setup (8 buttons, X/Y/Z/Rz axes)

// KO: USB 연결 여부(호스트가 시리얼을 열었을 때 true)
// EN: USB session active (true when host opens serial)
static inline bool usbSessionActive(){
  return (bool)Serial;
}

// KO/EN: Serial 출력은 PC 연결 시에만
static inline void logPrintln(){
  if(usbSessionActive()) Serial.println();
}
static inline void logPrint(const char* v){ if(usbSessionActive()) Serial.print(v); }
static inline void logPrint(const String& v){ if(usbSessionActive()) Serial.print(v); }
static inline void logPrint(char v){ if(usbSessionActive()) Serial.print(v); }
static inline void logPrint(int v){ if(usbSessionActive()) Serial.print(v); }
static inline void logPrint(unsigned int v){ if(usbSessionActive()) Serial.print(v); }
static inline void logPrint(long v){ if(usbSessionActive()) Serial.print(v); }
static inline void logPrint(unsigned long v){ if(usbSessionActive()) Serial.print(v); }
static inline void logPrint(float v){ if(usbSessionActive()) Serial.print(v); }
static inline void logPrint(double v){ if(usbSessionActive()) Serial.print(v); }
static inline void logPrint(int v, int fmt){ if(usbSessionActive()) Serial.print(v, fmt); }
static inline void logPrint(unsigned int v, int fmt){ if(usbSessionActive()) Serial.print(v, fmt); }
static inline void logPrint(long v, int fmt){ if(usbSessionActive()) Serial.print(v, fmt); }
static inline void logPrint(unsigned long v, int fmt){ if(usbSessionActive()) Serial.print(v, fmt); }
static inline void logPrint(float v, int fmt){ if(usbSessionActive()) Serial.print(v, fmt); }
static inline void logPrint(double v, int fmt){ if(usbSessionActive()) Serial.print(v, fmt); }
static inline void logPrintln(const char* v){ if(usbSessionActive()) Serial.println(v); }
static inline void logPrintln(const String& v){ if(usbSessionActive()) Serial.println(v); }
static inline void logPrintln(char v){ if(usbSessionActive()) Serial.println(v); }
static inline void logPrintln(int v){ if(usbSessionActive()) Serial.println(v); }
static inline void logPrintln(unsigned int v){ if(usbSessionActive()) Serial.println(v); }
static inline void logPrintln(long v){ if(usbSessionActive()) Serial.println(v); }
static inline void logPrintln(unsigned long v){ if(usbSessionActive()) Serial.println(v); }
static inline void logPrintln(float v){ if(usbSessionActive()) Serial.println(v); }
static inline void logPrintln(double v){ if(usbSessionActive()) Serial.println(v); }
static inline void logPrintln(int v, int fmt){ if(usbSessionActive()) Serial.println(v, fmt); }
static inline void logPrintln(unsigned int v, int fmt){ if(usbSessionActive()) Serial.println(v, fmt); }
static inline void logPrintln(long v, int fmt){ if(usbSessionActive()) Serial.println(v, fmt); }
static inline void logPrintln(unsigned long v, int fmt){ if(usbSessionActive()) Serial.println(v, fmt); }
static inline void logPrintln(float v, int fmt){ if(usbSessionActive()) Serial.println(v, fmt); }
static inline void logPrintln(double v, int fmt){ if(usbSessionActive()) Serial.println(v, fmt); }

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
// KO: S1/S2는 보드에서 GND에 고정 (미사용)
// EN: S1/S2 are tied to GND on board (unused)
#define MUX_S1     -1
#define MUX_S2     -1
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
#define BTN_LED_SERVO  12

// ============================================================================
// KO: LED 핀(사용자 지정)
// EN: LED pins (user specified)
// ============================================================================
#define LED1_POWER    25  // KO: LED1 / EN: LED1
#define LED2_HEADLESS 23  // KO: LED2 / EN: LED2
#define LED3_TRIM     24  // KO: LED3 / EN: LED3

// KO: 부팅 LED 시퀀스(LED1->LED2->LED3, 0.5s씩)
// EN: Boot LED sequence (LED1->LED2->LED3, 0.5s each)
static uint32_t bootAnimStartMs = 0;
static bool bootAnimDone = false;

// ============================================================================
// KO: RF 파이프 주소(매크로 충돌 방지 위해 이름 변경)
// EN: RF pipe addresses (renamed to avoid macro collision)
// ============================================================================
static uint8_t PIPE_ADDR_TX[5]   = {0xB1,0xB2,0xB3,0xB4,0x01};
static const uint8_t PIPE_ADDR_PAIR[5] = {0xE8,0xE8,0xF0,0xF0,0xE1};

static const uint8_t PAIR_CHANNEL = 76;

// KO: 다중 드론 프로파일 저장 (D1~D4)
// EN: Multi-drone profile storage (D1..D4)
static const uint8_t PROFILE_COUNT = 4;
static const int PROFILE_HOP_BASE = 0;
static const int PROFILE_HOP_STRIDE = 32;
static const int PROFILE_ADDR_BASE = 128;
static const int PROFILE_ADDR_STRIDE = 16;
static const int LEGACY_ADDR_OFFSET = 32;

// KO: FHSS 기본 홉 테이블(부팅 스캔으로 대체)
// EN: FHSS default hop table (replaced by boot scan)
static const uint8_t HOP_MAX = 12;
static uint8_t hopTable[HOP_MAX] = {63,64,65,66,67,68,69,70,71,72,73,74};
static uint8_t hopLen = HOP_MAX;
static uint8_t nextHopTable[HOP_MAX] = {0};
static uint8_t nextHopLen = 0;
static bool nextHopValid = false;
static bool pendingHopSave = false;
static bool profilePendingHopSave[PROFILE_COUNT] = {false,false,false,false};
static bool linkReady = false;
static bool boundOK = false;
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

// KO: 드론 프로파일(D1~D4)
// EN: Drone profiles (D1..D4)
struct ProfileData {
  uint8_t addr[5];
  uint8_t hopLen;
  uint8_t hopTable[HOP_MAX];
  uint8_t crc;
};

struct AmcState {
  Signal sig;
  bool holdActive;
  uint32_t holdUntil;
  uint8_t holdNeutralThrottle;
  uint32_t aux1Until;
};

static ProfileData profiles[PROFILE_COUNT] = {};
static bool profileValid[PROFILE_COUNT] = {false,false,false,false};
static bool profileLinkReady[PROFILE_COUNT] = {false,false,false,false};
static bool profileBoundOK[PROFILE_COUNT] = {false,false,false,false};
static uint32_t profileLastLinkTxMs[PROFILE_COUNT] = {0,0,0,0};
static uint32_t profileHopStartMs[PROFILE_COUNT] = {0,0,0,0};
static uint8_t profileHopIdxLast[PROFILE_COUNT] = {0xFF,0xFF,0xFF,0xFF};
static uint8_t profileHopSeed[PROFILE_COUNT] = {1,1,1,1};
static AmcState amcStates[PROFILE_COUNT] = {};

static uint8_t currentProfile = 0; // KO: 기본 D1 / EN: default D1
static bool amcMode = false;
static bool emergencyAll = false;
static const uint32_t HEADLESS_LONG_MS = 2000;
static const uint32_t FLOW_LONG_MS = 2000;
static const uint32_t FLIP_READY_TIMEOUT_MS = 3000;
static const uint8_t  FLIP_DIR_DEADZONE = 40;

static void printDebugLine();
static bool runPairing();

// KO: 스캔 설정(부팅/페어링 전용)
// EN: Scan settings (boot/pairing only)
static const uint8_t SCAN_MIN_CH = 5;  // KO: 2405 MHz / EN: 2405 MHz
static const uint8_t SCAN_MAX_CH = 80; // KO: 2480 MHz / EN: 2480 MHz
static const uint8_t SCAN_SAMPLES = 4;

static const char CTRL_NAME[8] = "AF1000X"; // KO: 8바이트, 널 문자 필요 없음 / EN: 8 bytes, no null required
static const char CTRL_VERSION[4] = "00b";
static const uint8_t LINK_FLAG_FACTORY_RESET = 0x01;
static bool factoryResetReq = false;
static const uint8_t AUX2_HEADLESS = 0x01;
static const uint8_t AUX2_FLOW     = 0x02;
static const uint8_t AUX2_FLIP_READY = 0x04;
static const uint8_t AUX1_TAKEOFF = 1;
static const uint8_t AUX1_GYRO_RESET = 2;
static const uint8_t AUX1_SERVO_TOGGLE = 3;
static const uint8_t AUX1_LED_STEP = 4;
static const uint8_t AUX1_FLIP_ROLL_POS  = 5;
static const uint8_t AUX1_FLIP_ROLL_NEG  = 6;
static const uint8_t AUX1_FLIP_PITCH_POS = 7;
static const uint8_t AUX1_FLIP_PITCH_NEG = 8;

static const uint32_t EEPROM_MAGIC = 0xA0F1A0F1;
static const uint32_t EEPROM_ADDR_MAGIC = 0xA0F1B0B1;
static const int EEPROM_SIZE = 256;


RF24 radio(RF_CE, RF_CSN);

static uint8_t hopSeed = 1;
static uint32_t hopStartMs = 0;
static uint8_t hopIdx = 0;
static uint8_t hopIdxLast = 0xFF;

// KO: 현재 시간에 해당하는 홉 인덱스 계산
// EN: Calculate hop index for current time
static inline uint8_t hopIndexForMs(uint32_t now){
  if(HOP_SLOT_MS == 0 || hopLen == 0) return 0;
  uint32_t slots = (now - hopStartMs) / HOP_SLOT_MS;
  return (uint8_t)((slots + hopSeed) % hopLen);
}

// KO: FHSS 채널 업데이트 (시간 기반)
// EN: Update FHSS channel (time-based)
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

// KO: 홉 테이블 CRC 계산
// EN: Calculate hop table CRC
static inline uint8_t hopCrc(const uint8_t* t, uint8_t len){
  uint8_t c = 0x5A;
  for(uint8_t i = 0; i < len; i++){
    c ^= (uint8_t)(t[i] + (uint8_t)(i * 17));
  }
  return c;
}

// KO: EEPROM 초기화 (최초 1회)
// EN: Initialize EEPROM (once)
static bool eepromInit(){
  if(eepromReady) return true;
  EEPROM.begin(EEPROM_SIZE);
  eepromReady = true;
  return true;
}

// KO: 프로파일별 홉 테이블 EEPROM 오프셋
// EN: Hop table EEPROM offset per profile
static inline int hopOffsetForProfile(uint8_t idx){
  return PROFILE_HOP_BASE + (int)idx * PROFILE_HOP_STRIDE;
}

// KO: 프로파일별 주소 EEPROM 오프셋
// EN: Address EEPROM offset per profile
static inline int addrOffsetForProfile(uint8_t idx){
  return PROFILE_ADDR_BASE + (int)idx * PROFILE_ADDR_STRIDE;
}

// KO: EEPROM에서 특정 프로파일의 홉 테이블 로드
// EN: Load hop table for specific profile from EEPROM
static bool loadHopTableFromEepromProfile(uint8_t idx, uint8_t* outTable, uint8_t &outLen){
  if(!eepromInit()) return false;
  HopStore s = {};
  EEPROM.get(hopOffsetForProfile(idx), s);
  if(s.magic != EEPROM_MAGIC) return false;
  if(s.len < 1 || s.len > HOP_MAX) return false;
  for(uint8_t i = 0; i < s.len; i++){
    if(s.table[i] < SCAN_MIN_CH || s.table[i] > SCAN_MAX_CH) return false;
  }
  if(s.crc != hopCrc(s.table, s.len)) return false;
  outLen = s.len;
  for(uint8_t i = 0; i < outLen; i++) outTable[i] = s.table[i];
  return true;
}

// KO: EEPROM에 특정 프로파일의 홉 테이블 저장
// EN: Save hop table for specific profile to EEPROM
static void saveHopTableToEepromProfile(uint8_t idx, const uint8_t* table, uint8_t len){
  if(!eepromInit()) return;
  HopStore s = {};
  s.magic = EEPROM_MAGIC;
  s.len = len;
  for(uint8_t i = 0; i < HOP_MAX; i++) s.table[i] = (i < len) ? table[i] : 0;
  s.crc = hopCrc(s.table, s.len);
  EEPROM.put(hopOffsetForProfile(idx), s);
  EEPROM.commit();
}

// KO: 주소 CRC 계산
// EN: Calculate address CRC
static inline uint8_t addrCrc(const uint8_t* a){
  uint8_t c = 0x3D;
  for(uint8_t i = 0; i < 5; i++){
    c ^= (uint8_t)(a[i] + (i * 11));
  }
  return c;
}

// KO: EEPROM에서 특정 프로파일의 TX 주소 로드
// EN: Load TX address for specific profile from EEPROM
static bool loadTxAddrFromEepromProfile(uint8_t idx, uint8_t* outAddr){
  if(!eepromInit()) return false;
  AddrStore s = {};
  EEPROM.get(addrOffsetForProfile(idx), s);
  if(s.magic != EEPROM_ADDR_MAGIC) return false;
  if(s.crc != addrCrc(s.addr)) return false;
  uint8_t orv = s.addr[0] | s.addr[1] | s.addr[2] | s.addr[3] | s.addr[4];
  uint8_t andv = s.addr[0] & s.addr[1] & s.addr[2] & s.addr[3] & s.addr[4];
  if(orv == 0x00 || andv == 0xFF) return false;
  for(uint8_t i = 0; i < 5; i++) outAddr[i] = s.addr[i];
  return true;
}

// KO: EEPROM에 특정 프로파일의 TX 주소 저장
// EN: Save TX address for specific profile to EEPROM
static void saveTxAddrToEepromProfile(uint8_t idx, const uint8_t* addr){
  if(!eepromInit()) return;
  AddrStore s = {};
  s.magic = EEPROM_ADDR_MAGIC;
  for(uint8_t i = 0; i < 5; i++) s.addr[i] = addr[i];
  s.crc = addrCrc(s.addr);
  EEPROM.put(addrOffsetForProfile(idx), s);
  EEPROM.commit();
}

// KO: 레거시(D1) 주소 호환 로드
// EN: Legacy (D1) address load for backward compatibility
static bool loadTxAddrFromEepromLegacy(uint8_t* outAddr){
  if(!eepromInit()) return false;
  AddrStore s = {};
  EEPROM.get(LEGACY_ADDR_OFFSET, s);
  if(s.magic != EEPROM_ADDR_MAGIC) return false;
  if(s.crc != addrCrc(s.addr)) return false;
  uint8_t orv = s.addr[0] | s.addr[1] | s.addr[2] | s.addr[3] | s.addr[4];
  uint8_t andv = s.addr[0] & s.addr[1] & s.addr[2] & s.addr[3] & s.addr[4];
  if(orv == 0x00 || andv == 0xFF) return false;
  for(uint8_t i = 0; i < 5; i++) outAddr[i] = s.addr[i];
  return true;
}

// KO: 기존 함수 호환(프로파일 D1)
// EN: Backward-compatible wrappers (profile D1)
static bool loadHopTableFromEeprom(){
  return loadHopTableFromEepromProfile(0, hopTable, hopLen);
}

static void saveHopTableToEeprom(){
  saveHopTableToEepromProfile(0, hopTable, hopLen);
}

static bool loadTxAddrFromEeprom(){
  return loadTxAddrFromEepromProfile(0, PIPE_ADDR_TX);
}

static void saveTxAddrToEeprom(){
  saveTxAddrToEepromProfile(0, PIPE_ADDR_TX);
}

// KO: 현재 TX 주소 시리얼 출력
// EN: Print current TX address to serial
static void printTxAddr(){
  logPrint("ID ");
  for(uint8_t i = 0; i < 5; i++){
    if(i) logPrint("-");
    if(PIPE_ADDR_TX[i] < 16) logPrint("0");
    logPrint(PIPE_ADDR_TX[i], HEX);
  }
  logPrintln();
}

// KO: 부팅 배너 출력
// EN: Print boot banner
static void printBanner(){
  logPrintln("SYUBEA Co., LTD");
  logPrintln("www.1510.co.kr");
  logPrint("Model : AR1000X Controller - Ver.");
  logPrintln(CTRL_VERSION);
  logPrintln();
  printTxAddr();
}

// KO: 칩 ID 문자열을 64비트 정수로 변환
// EN: Convert chip ID string to uint64
static inline uint64_t chipIdToU64(uint64_t v){ return v; }
static inline uint64_t chipIdToU64(const char* s){
  uint64_t v = 0;
  if(!s) return 0;
  while(*s){
    char c = *s++;
    uint8_t n;
    if(c >= '0' && c <= '9') n = (uint8_t)(c - '0');
    else if(c >= 'a' && c <= 'f') n = (uint8_t)(c - 'a' + 10);
    else if(c >= 'A' && c <= 'F') n = (uint8_t)(c - 'A' + 10);
    else continue;
    v = (v << 4) | n;
  }
  return v;
}

// KO: 고유 ID 기반 기본 주소 생성
// EN: Generate base address from unique ID
static void generateBaseAddr(uint8_t* outAddr){
#if defined(ARDUINO_ARCH_RP2040)
  uint64_t chip = chipIdToU64(rp2040.getChipID());
  for(uint8_t i = 0; i < 5; i++){
    outAddr[i] = (uint8_t)(chip >> (i * 8));
  }
#else
  uint32_t seed = micros();
  seed ^= (uint32_t)analogRead(ADC_THROTTLE) << 16;
  seed ^= (uint32_t)analogRead(ADC_YAW) << 8;
  seed ^= (uint32_t)analogRead(ADC_PITCH);
  randomSeed(seed);
  for(uint8_t i = 0; i < 5; i++){
    outAddr[i] = (uint8_t)random(1, 255);
  }
#endif
  uint8_t orv = outAddr[0] | outAddr[1] | outAddr[2] | outAddr[3] | outAddr[4];
  uint8_t andv = outAddr[0] & outAddr[1] & outAddr[2] & outAddr[3] & outAddr[4];
  if(orv == 0x00 || andv == 0xFF){
    outAddr[0] ^= 0xA5;
  }
}

// KO: 메인 TX 주소 생성
// EN: Generate main TX address
static void generateTxAddr(){
  generateBaseAddr(PIPE_ADDR_TX);
}

// KO: 프로파일별 고유 주소 생성 (믹싱)
// EN: Generate unique address per profile (mixing)
static void generateProfileAddr(uint8_t idx, uint8_t* outAddr){
  generateBaseAddr(outAddr);
  uint8_t mix = (uint8_t)((idx + 1) * 37);
  for(uint8_t i = 0; i < 5; i++){
    outAddr[i] ^= (uint8_t)(mix + (i * 11));
    if(outAddr[i] == 0x00) outAddr[i] = 0xA5 ^ (uint8_t)i;
    if(outAddr[i] == 0xFF) outAddr[i] = 0x5A ^ (uint8_t)i;
  }
  uint8_t orv = outAddr[0] | outAddr[1] | outAddr[2] | outAddr[3] | outAddr[4];
  uint8_t andv = outAddr[0] & outAddr[1] & outAddr[2] & outAddr[3] & outAddr[4];
  if(orv == 0x00 || andv == 0xFF){
    outAddr[0] ^= 0xA5;
  }
}

// KO: TX 주소 초기화 (로드 실패 시 생성)
// EN: Initialize TX address (generate if load fails)
static void initTxAddr(){
  if(loadTxAddrFromEeprom()) return;
  generateTxAddr();
  saveTxAddrToEeprom();
}

// KO: 모든 프로파일(D1~D4) 초기화 및 로드
// EN: Initialize and load all profiles (D1..D4)
static void initProfiles(bool regenAll){
  // KO: 주소 로드/생성
  // EN: load or generate addresses
  for(uint8_t i = 0; i < PROFILE_COUNT; i++){
    bool ok = false;
    if(!regenAll){
      ok = loadTxAddrFromEepromProfile(i, profiles[i].addr);
      if(!ok && i == 0){
        // KO: 레거시 D1 주소 로드
        // EN: legacy D1 address load
        ok = loadTxAddrFromEepromLegacy(profiles[i].addr);
      }
    }
    if(!ok){
      generateProfileAddr(i, profiles[i].addr);
      saveTxAddrToEepromProfile(i, profiles[i].addr);
    }
    profileHopSeed[i] = profiles[i].addr[4] ? profiles[i].addr[4] : 1;
    profileValid[i] = true;
  }

  // KO: 홉 테이블 로드/생성
  // EN: load or build hop tables
  if(!loadHopTableFromEepromProfile(0, profiles[0].hopTable, profiles[0].hopLen)){
    scanAndBuildHopTable(profiles[0].hopTable, profiles[0].hopLen);
    saveHopTableToEepromProfile(0, profiles[0].hopTable, profiles[0].hopLen);
  }
  for(uint8_t i = 1; i < PROFILE_COUNT; i++){
    if(!loadHopTableFromEepromProfile(i, profiles[i].hopTable, profiles[i].hopLen)){
      profiles[i].hopLen = profiles[0].hopLen;
      for(uint8_t j = 0; j < HOP_MAX; j++){
        profiles[i].hopTable[j] = profiles[0].hopTable[j];
      }
      saveHopTableToEepromProfile(i, profiles[i].hopTable, profiles[i].hopLen);
    }
  }
}

// KO: 런타임에 특정 프로파일 활성화 (RF 설정)
// EN: Activate specific profile at runtime (RF setup)
static void loadProfileRuntime(uint8_t idx, uint32_t now){
  for(uint8_t i = 0; i < 5; i++) PIPE_ADDR_TX[i] = profiles[idx].addr[i];
  hopLen = profiles[idx].hopLen;
  for(uint8_t i = 0; i < HOP_MAX; i++) hopTable[i] = profiles[idx].hopTable[i];
  hopSeed = profileHopSeed[idx] ? profileHopSeed[idx] : 1;
  hopStartMs = profileHopStartMs[idx];
  if(hopStartMs == 0){
    hopStartMs = now;
    profileHopStartMs[idx] = now;
  }
  hopIdxLast = profileHopIdxLast[idx];
  radio.openWritingPipe(PIPE_ADDR_TX);
  linkReady = profileLinkReady[idx];
  boundOK = profileBoundOK[idx];
  lastLinkTxMs = profileLastLinkTxMs[idx];
  pendingHopSave = profilePendingHopSave[idx];
}

// KO: 현재 런타임 상태를 프로파일에 저장
// EN: Save current runtime state to profile
static void saveProfileRuntime(uint8_t idx){
  profileHopSeed[idx] = hopSeed;
  profileHopStartMs[idx] = hopStartMs;
  profileHopIdxLast[idx] = hopIdxLast;
  profileLinkReady[idx] = linkReady;
  profileBoundOK[idx] = boundOK;
  profileLastLinkTxMs[idx] = lastLinkTxMs;
  profilePendingHopSave[idx] = pendingHopSave;
}

// KO: 링크 페이로드 CRC 계산
// EN: Calculate link payload CRC
static inline uint8_t linkCrc(const LinkPayload &p){
  uint8_t c = 0x6A;
  for(uint8_t i = 0; i < 8; i++) c ^= (uint8_t)(p.name[i] + (i * 13));
  for(uint8_t i = 0; i < 5; i++) c ^= (uint8_t)(p.addr[i] + (i * 7));
  c ^= p.hopLen;
  for(uint8_t i = 0; i < p.hopLen && i < HOP_MAX; i++) c ^= (uint8_t)(p.hopTable[i] + (i * 11));
  c ^= p.flags;
  return c;
}

// KO: 링크 페이로드 구성 (이름, 주소, 홉테이블)
// EN: Build link payload (name, addr, hop table)
static inline void buildLinkPayload(LinkPayload &p, const uint8_t* table, uint8_t len){
  for(uint8_t i = 0; i < 8; i++) p.name[i] = CTRL_NAME[i];
  for(uint8_t i = 0; i < 5; i++) p.addr[i] = PIPE_ADDR_TX[i];
  p.hopLen = len;
  for(uint8_t i = 0; i < HOP_MAX; i++) p.hopTable[i] = (i < len) ? table[i] : 0;
  p.flags = factoryResetReq ? LINK_FLAG_FACTORY_RESET : 0;
  p.crc = linkCrc(p);
}

// KO: 주파수 스캔 및 최적 홉 테이블 생성
// EN: Scan frequencies and build optimal hop table
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

// KO: 대기 중인 새 홉 테이블 적용
// EN: Apply pending new hop table
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

// KO: 링크 설정 패킷 전송 (페어링 채널)
// EN: Send link setup packet (pairing channel)
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

// KO: 특정 프로파일로 제어 신호 전송
// EN: Send control signal to specific profile
static bool sendSignalForProfile(uint8_t idx, const Signal &sig, uint32_t now){
  loadProfileRuntime(idx, now);
  if(!linkReady){
    sendLinkSetup(now);
  }
  Signal txSig = sig;
  fhssTxUpdate(now);
  txSig.hop = hopIdx;
  bool ok = radio.write(&txSig, sizeof(txSig));
  if(ok){
    boundOK = true;
    if(pendingHopSave){
      saveHopTableToEepromProfile(idx, hopTable, hopLen);
      profiles[idx].hopLen = hopLen;
      for(uint8_t i = 0; i < HOP_MAX; i++){
        profiles[idx].hopTable[i] = hopTable[i];
      }
      pendingHopSave = false;
    }
  }
  saveProfileRuntime(idx);
  return ok;
}

// KO: 모든 프로파일에 비상 정지 신호 전송
// EN: Send emergency signal to all profiles
static void sendEmergencyAll(uint32_t now){
  uint8_t savedProfile = currentProfile;
  Signal e = makeEmergencySignal();
  for(uint8_t i = 0; i < PROFILE_COUNT; i++){
    sendSignalForProfile(i, e, now);
  }
  loadProfileRuntime(savedProfile, now);
}

Signal tx;

// ============================================================================
// KO: PC 허브(Serial → RF)
// EN: PC hub (Serial -> RF)
// ============================================================================
uint8_t speedMode = 1;      // KO: 1~3 / EN: 1..3
bool headlessOn = false;    // KO: AUX2 플래그 / EN: AUX2 flag
static bool flipReady = false;
static uint32_t flipReadyStartMs = 0;
float vbat = 4.2f;          // KO: 루프에서 readBattery()로 갱신 / EN: updated in loop via readBattery()
bool stickActive = false;   // KO: 물리 스틱이 활성일 때 true / EN: true when physical sticks are active


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
bool emergencyPulse = false;
static bool killHoldActive = false;
static bool killTriggered = false;
static uint32_t killHoldStart = 0;
static bool flowOn = true;
static bool debugEnabled = false;
static uint32_t debugNextMs = 0;
static bool vlrEnabled = false;

// KO: 트림 오프셋(RAM만, 전원 사이클 시 리셋)
// EN: Trim offsets (RAM only, reset on power cycle)
int trimRoll = 0;
int trimPitch = 0;

// KO: AUX1 펄스(이륙/착륙 트리거)
// EN: AUX1 pulse (takeoff/land trigger)
uint32_t aux1Until = 0;
uint32_t aux1UserUntil = 0;
uint8_t aux1UserValue = 0;
const uint32_t AUX_PULSE = 200;
const uint32_t AUTO_LONG_MS = 2000;
const uint32_t GYRO_RESET_PULSE_MS = 300;
const uint32_t USER_LONG_MS = 1500;

const uint32_t STICK_KILL_HOLD_MS = 2000;
const uint8_t  STICK_KILL_LOW = 30;
const uint8_t  STICK_KILL_HIGH = 225;
uint32_t gyroResetUntil = 0;

// KO: dd3 스타일 문자열 명령 홀드 상태
// EN: dd3-style string command hold state
static bool pcHoldActive = false;
static uint32_t pcHoldUntil = 0;
static uint8_t pcHoldNeutralThrottle = 0;
static uint16_t pcLastPower = 0;
static uint16_t pcLastTimeMs = 0;


// KO: 비상 정지 상태 적용 (플래그 설정)
// EN: Apply emergency state (set flags)
static void applyEmergency(){
  flipReady = false;
  triggerEmergencyAll(millis());
}

// KO: AUX2 비트필드 구성 (헤드리스, 플로우 등)
// EN: Build AUX2 bitfield (headless, flow, etc.)
static inline uint8_t buildAux2(){
  return (headlessOn ? AUX2_HEADLESS : 0) |
         (flowOn ? AUX2_FLOW : 0) |
         (flipReady ? AUX2_FLIP_READY : 0);
}

// KO: 모든 프로파일의 AUX2 상태 동기화
// EN: Sync AUX2 state across all profiles
static inline void syncAux2All(){
  uint8_t aux2 = buildAux2();
  pcSig.aux2 = aux2;
  for(uint8_t i = 0; i < PROFILE_COUNT; i++){
    amcStates[i].sig.aux2 = aux2;
  }
}


// KO: 파워 값을 델타 값으로 변환
// EN: Convert power value to delta
static int powerToDelta(int p){
  // KO: dd3 power 범위 0..255(직접) / EN: dd3 power range 0..255 (direct)
  if(p < 0) p = -p;
  // KO: 0..255를 0..120으로 매핑(중앙 128 기준 델타)
  // EN: map 0..255 -> 0..120 (delta around center 128)
  int d = map(constrain(p, 0, 255), 0, 255, 0, 120);
  return constrain(d, 0, 127);
}

// KO: 신호를 중립 상태로 설정
// EN: Set signal to neutral state
static void setNeutral(Signal &s, uint8_t thr){
  s.throttle = thr;
  s.roll = 127;
  s.pitch = 127;
  s.yaw = 127;
  // KO: aux1/aux2는 명령이 바꾸지 않으면 유지 / EN: aux1/aux2 keep as-is unless command changes them
}

// KO: 비상 정지용 신호 생성
// EN: Create emergency signal
static Signal makeEmergencySignal(){
  Signal s = {};
  s.throttle = 0;
  s.roll = 127;
  s.pitch = 127;
  s.yaw = 127;
  s.aux1 = 0;
  s.aux2 = 0;
  s.speed = 1;
  return s;
}

// KO: AMC 상태 초기화
// EN: Reset AMC state
static void resetAmcState(uint8_t idx, uint8_t baseThr){
  setNeutral(amcStates[idx].sig, baseThr);
  amcStates[idx].sig.speed = speedMode;
  amcStates[idx].sig.aux1 = 0;
  amcStates[idx].sig.aux2 = buildAux2();
  amcStates[idx].holdActive = false;
  amcStates[idx].holdUntil = 0;
  amcStates[idx].holdNeutralThrottle = baseThr;
  amcStates[idx].aux1Until = 0;
}

// KO: 모든 AMC 상태 초기화
// EN: Initialize all AMC states
static void initAmcStates(uint8_t baseThr){
  for(uint8_t i = 0; i < PROFILE_COUNT; i++){
    resetAmcState(i, baseThr);
  }
}

// KO: 전체 비상 정지 트리거
// EN: Trigger global emergency
static void triggerEmergencyAll(uint32_t now){
  (void)now;
  emergencyAll = true;
  emergencyPulse = true;
  pcOverride = false;
  pcHoldActive = false;
  pcSig = makeEmergencySignal();
  for(uint8_t i = 0; i < PROFILE_COUNT; i++){
    resetAmcState(i, 0);
    amcStates[i].sig = makeEmergencySignal();
  }
}

// KO: PC 제어 홀드 시작 (단일)
// EN: Start PC control hold (single)
static void startHold(uint32_t now, uint32_t durMs){
  pcHoldActive = true;
  pcHoldUntil = now + durMs;
  lastPcMs = now;       // KO: 유지(keep alive) / EN: keep alive
  pcOverride = true;
}

// KO: AMC 제어 홀드 시작 (프로파일별)
// EN: Start AMC control hold (per profile)
static void amcStartHold(uint8_t idx, uint32_t now, uint32_t durMs){
  amcStates[idx].holdActive = true;
  amcStates[idx].holdUntil = now + durMs;
}

// KO: AMC 신호 생성 (홀드/펄스 처리)
// EN: Build AMC signal (handle hold/pulse)
static Signal buildAmcSignal(uint8_t idx, uint32_t now){
  AmcState &st = amcStates[idx];
  if(st.holdActive && (int32_t)(now - st.holdUntil) >= 0){
    st.holdActive = false;
    setNeutral(st.sig, st.holdNeutralThrottle);
  }
  Signal s = st.sig;
  if(now < st.aux1Until){
    s.aux1 = AUX1_TAKEOFF;
  }
  return s;
}

// KO: PC 시리얼 명령 처리
// EN: Handle PC serial command
static void handlePcLine(String s){
  s.trim();
  if(!s.length()) return;

  // KO: dd3가 "emergency"처럼 소문자로 보낼 때가 있음
  // EN: dd3 sometimes sends lowercase like "emergency"
  String u = s;
  u.trim();
  u.toUpperCase();
  uint32_t now = millis();

  // KO: D1~D4 타겟 프리픽스 파싱
  // EN: Parse D1..D4 target prefix
  uint8_t targetProfile = currentProfile;
  if(u.length() >= 2 && u.charAt(0) == 'D'){
    char d = u.charAt(1);
    if(d >= '1' && d <= ('0' + PROFILE_COUNT)){
      targetProfile = (uint8_t)(d - '1');
      u = u.substring(2);
      u.trim();
      if(u.length() && (u.charAt(0) == ':' || u.charAt(0) == ',')){
        u = u.substring(1);
        u.trim();
      }
    }
  }

  // KO: 도움말 출력
  // EN: Print help
  if(u == "HELP" || u == "?"){
    logPrintln("=== AR1000X COMMANDS ===");
    logPrintln(" [FLIGHT] START, TAKEOFF, LAND, STOP, EMERGENCY, HOVER");
    logPrintln(" [MOVE] UP, DOWN, FORWARD, BACK, LEFT, RIGHT, CW, CCW <val> <ms>");
    logPrintln(" [MODE] HEADLESS, FLOW ON/OFF, SPEED 1/2/3, AMC ON/OFF");
    logPrintln(" [SETUP] BIND, GYRORESET, FLIP F/B/L/R");
    logPrintln(" [INFO] BAT?, ID?, DBUG, VLR, VLROFF");
    logPrintln(" [MULTI] Prefix D1~D4 (e.g. D1 TAKEOFF)");
    return;
  }

  // KO: AMC 멀티제어 모드
  // EN: AMC multi-control mode
  if(u == CMD_AMC || u == CMD_AMC " ON" || u == CMD_AMC " 1"){
    amcMode = true;
    initAmcStates(stickSig.throttle);
    pcOverride = false;
    pcHoldActive = false;
    logPrintln("OK");
    return;
  }
  if(u == CMD_AMC " OFF" || u == CMD_AMC " 0"){
    amcMode = false;
    pcOverride = false;
    pcHoldActive = false;
    logPrintln("OK");
    return;
  }

  // KO: FLOW 토글(PC 시리얼)
  // EN: FLOW toggle (PC serial)
  if(u == CMD_FLOW " ON" || u == CMD_FLOW " 1"){
    flowOn = true;
    logPrintln("OK");
    return;
  }
  if(u == CMD_FLOW " OFF" || u == CMD_FLOW " 0"){
    flowOn = false;
    logPrintln("OK");
    return;
  }

  // KO: 수동 바인딩(스캔 + 페어링 페이로드)
  // EN: Manual binding (scan + pairing payload)
  if(u.startsWith(CMD_BIND) || u.startsWith(CMD_PAIR)){
    uint8_t bindTarget = targetProfile;
    int sp = u.indexOf(' ');
    if(sp > 0){
      String arg = u.substring(sp + 1);
      arg.trim();
      if(arg.length() >= 2 && arg.charAt(0) == 'D'){
        char d = arg.charAt(1);
        if(d >= '1' && d <= ('0' + PROFILE_COUNT)){
          bindTarget = (uint8_t)(d - '1');
        }
      }
    }
    uint8_t savedProfile = currentProfile;
    currentProfile = bindTarget;
    loadProfileRuntime(bindTarget, now);
    scanAndBuildHopTable(hopTable, hopLen);
    profiles[bindTarget].hopLen = hopLen;
    for(uint8_t i = 0; i < HOP_MAX; i++){
      profiles[bindTarget].hopTable[i] = hopTable[i];
    }
    nextHopValid = false;
    bool pairedOk = runPairing();
    boundOK = false;
    linkReady = pairedOk;
    pendingHopSave = true;
    saveProfileRuntime(bindTarget);
    currentProfile = savedProfile;
    loadProfileRuntime(savedProfile, now);
    logPrintln("OK");
    return;
  }

  // KO: 디버그 출력 토글(1초마다 출력)
  // EN: Debug output toggle (prints every 1s)
  if(u == CMD_DBUG){
    debugEnabled = !debugEnabled;
    debugNextMs = millis() + 1000;
    logPrintln(debugEnabled ? "DBUG ON" : "DBUG OFF");
    if(debugEnabled){
      printDebugLine();
    }
    return;
  }

  // KO: 레버 값 출력 토글 (VLR / VLROFF)
  // EN: Toggle lever value output
  if(u == "VLR" || u == "VLR ON"){
    vlrEnabled = true;
    logPrintln("OK");
    return;
  }
  if(u == "VLROFF" || u == "VLR OFF"){
    vlrEnabled = false;
    logPrintln("OK");
    return;
  }

  // KO: 블록 코딩 연결 확인용
  // EN: Block coding connection check
  if(u == "SYUBEA1004"){
    logPrintln("OK");
    return;
  }

  // KO: 조종기 ID 출력
  // EN: Print controller ID
  if(u == CMD_ID "?" || u == CMD_TXID "?" || u == CMD_ADDR "?"){
    for(uint8_t i = 0; i < PROFILE_COUNT; i++){
      logPrint("D"); logPrint(i + 1); logPrint(" ID: ");
      for(uint8_t j = 0; j < 5; j++){
        if(j) logPrint("-");
        if(profiles[i].addr[j] < 16) logPrint("0");
        logPrint(profiles[i].addr[j], HEX);
      }
      logPrintln();
    }
    return;
  }

  // ==========================================================================
  // KO: 즉시 안전 처리
  // EN: Immediate safety
  // ==========================================================================
  if(u == CMD_EMERGENCY){
    applyEmergency();
    pcHoldActive = false;
    // KO: 모든 드론에 즉시 비상정지 전송
    // EN: Send emergency to all drones immediately
    sendEmergencyAll(now);
    logPrintln("OK");
    return;
  }

  if(stickActive && !amcMode){
    logPrintln("IGN");
    return;
  }

  // KO: 입력 소스 강제(옵션)
  // EN: Force source (optional)
  if(u.startsWith(CMD_SRC " ")){
    String src = u.substring(4);
    src.trim();
    if(src == "PC"){
      pcOverride = true;
      lastPcMs = millis();
    } else if(src == "STICK"){
      pcOverride = false;
      pcHoldActive = false;
    }
    logPrintln("OK");
    return;
  }

  // ==========================================================================
  // KO: 텔레메트리 조회
  // EN: Telemetry query
  // ==========================================================================
  if(u == CMD_BATTERY "?" || u == CMD_BATTERY || u == CMD_BAT "?" || u == CMD_BAT){
    logPrint("BAT ");
    logPrintln(vbat, 2);
    return;
  }

  // ==========================================================================
  // KO: dd3 속도 N
  // EN: dd3 speed N
  // ==========================================================================
  if(u.startsWith(CMD_SPEED " ")){
    int sp = u.substring(6).toInt();
    sp = constrain(sp, 1, 3);
    speedMode = (uint8_t)sp;
    pcSig.speed = (uint8_t)sp;
    for(uint8_t i = 0; i < PROFILE_COUNT; i++){
      amcStates[i].sig.speed = (uint8_t)sp;
    }
    logPrintln("OK");
    return;
  }

  // KO: START 명령 (시동/ARM 활성화)
  // EN: START command (Arming)
  if(u == CMD_START){
    logPrintln("OK");
    return;
  }

  // ==========================================================================
  // KO: dd3 stop/takeoff/land/headless/gyroreset/funled
  // EN: dd3 stop/takeoff/land/headless/gyroreset/funled
  // ==========================================================================
  if(u == CMD_TAKEOFF || u == CMD_LAND || u == CMD_LANDING){
    if(amcMode){
      // KO: AMC는 타겟 프로파일에 AUX1 펄스
      // EN: AMC uses AUX1 pulse on target profile
      AmcState &st = amcStates[targetProfile];
      uint8_t baseThr = st.sig.throttle;
      st.holdNeutralThrottle = baseThr;
      st.sig.speed = speedMode;
      st.sig.aux2 = buildAux2();
      setNeutral(st.sig, baseThr);
      st.aux1Until = now + 180;
      st.holdActive = false;
    } else {
      // KO: AUX1 펄스 사용(버튼 자동 이륙/착륙과 동일)
      // EN: Use AUX1 pulse (same behavior as button auto takeoff/land)
      pcSig.speed = speedMode;
      pcSig.aux2 = buildAux2();
      setNeutral(pcSig, stickSig.throttle);
      pcSig.aux1 = 1;
      startHold(millis(), 180); // KO: ~180ms 펄스 / EN: ~180ms pulse
    }
    logPrintln("OK");
    return;
  }

  if(u == CMD_STOP){
    // KO: STOP을 비상 스로틀 컷으로 처리(더 안전)
    // EN: Treat STOP as emergency throttle-cut (safer)
    applyEmergency();
    pcHoldActive = false;
    sendEmergencyAll(now);
    logPrintln("OK");
    return;
  }

  if(u == CMD_HEADLESS){
    headlessOn = !headlessOn;
    flipReady = false;
    syncAux2All();
    logPrintln("OK");
    return;
  }

  if(u == CMD_GYRORESET || u == CMD_GYRO_RESET){
    // KO: 자이로 초기화 트리거 / EN: Trigger gyro reset
    gyroResetUntil = now + GYRO_RESET_PULSE_MS;
    logPrintln("OK");
    return;
  }

  if(u == CMD_FUNLED || u == CMD_FUN_LED){
    logPrintln("OK");
    return;
  }

  if(u == CMD_HOVER){
    if(amcMode){
      amcStates[targetProfile].holdActive = false;
      setNeutral(amcStates[targetProfile].sig, amcStates[targetProfile].holdNeutralThrottle);
    } else {
      pcHoldActive = false;
      pcOverride = false;
    }
    logPrintln("OK");
    return;
  }

  // KO: FLIP 명령 (FLIP F/B/L/R)
  // EN: FLIP command
  if(u.startsWith(CMD_FLIP " ")){
    String dirStr = u.substring(5);
    dirStr.trim();
    char d = dirStr.length() > 0 ? dirStr.charAt(0) : ' ';
    
    uint8_t flipCmd = 0;
    if(d == 'F') flipCmd = AUX1_FLIP_PITCH_POS;      // 앞 (Forward)
    else if(d == 'B') flipCmd = AUX1_FLIP_PITCH_NEG; // 뒤 (Back)
    else if(d == 'L') flipCmd = AUX1_FLIP_ROLL_NEG;  // 좌 (Left)
    else if(d == 'R') flipCmd = AUX1_FLIP_ROLL_POS;  // 우 (Right)
    
    if(flipCmd != 0){
      if(amcMode){
        // AMC 모드에서는 현재 지원하지 않음 (필요 시 추가 가능)
      } else {
        aux1UserValue = flipCmd;
        aux1UserUntil = now + AUX_PULSE;
      }
      logPrintln("OK");
    } else {
      logPrintln("ERR");
    }
    return;
  }

  // KO: 다중 축 이동 명령 (MOVE t r p y time)
  // EN: Multi-axis move command
  if(u.startsWith(CMD_MOVE " ")){
    int t, r, p, y, tms;
    // Format: MOVE throttle roll pitch yaw time_ms
    int n = sscanf(u.c_str(), "MOVE %d %d %d %d %d", &t, &r, &p, &y, &tms);
    if(n == 5){
      tms = constrain(tms, 20, 15000);
      
      if(amcMode){
        AmcState &st = amcStates[targetProfile];
        st.sig.throttle = (uint8_t)constrain(t, 0, 255);
        st.sig.roll     = (uint8_t)constrain(r, 0, 255);
        st.sig.pitch    = (uint8_t)constrain(p, 0, 255);
        st.sig.yaw      = (uint8_t)constrain(y, 0, 255);
        st.sig.speed    = speedMode;
        st.sig.aux1     = 0;
        st.sig.aux2     = buildAux2();
        st.aux1Until    = 0;
        st.holdNeutralThrottle = stickSig.throttle;
        
        amcStartHold(targetProfile, now, (uint32_t)tms);
      } else {
        pcSig.throttle = (uint8_t)constrain(t, 0, 255);
        pcSig.roll     = (uint8_t)constrain(r, 0, 255);
        pcSig.pitch    = (uint8_t)constrain(p, 0, 255);
        pcSig.yaw      = (uint8_t)constrain(y, 0, 255);
        pcSig.speed    = speedMode;
        pcSig.aux1     = 0;
        pcSig.aux2     = buildAux2();
        
        pcHoldNeutralThrottle = stickSig.throttle;
        startHold(millis(), (uint32_t)tms);
      }
      logPrintln("OK");
    } else {
      logPrintln("ERR");
    }
    return;
  }

  // ==========================================================================
  // KO: JOY 직접 입력(계속 지원)
  // EN: JOY direct (still supported)
  // ==========================================================================
  if(u.startsWith(CMD_JOY " ")){
    int t,r,p,y,a1,a2,spd;
    int n = sscanf(u.c_str(), "JOY %d %d %d %d %d %d %d", &t,&r,&p,&y,&a1,&a2,&spd);
    if(n == 7){
      int sm = constrain(spd, 1, 3);
      if(amcMode){
        AmcState &st = amcStates[targetProfile];
        st.sig.throttle = (uint8_t)constrain(t,  0, 255);
        st.sig.roll     = (uint8_t)constrain(r,  0, 255);
        st.sig.pitch    = (uint8_t)constrain(p,  0, 255);
        st.sig.yaw      = (uint8_t)constrain(y,  0, 255);
        st.sig.aux1     = (uint8_t)constrain(a1, 0, 255);
        st.sig.aux2     = (uint8_t)constrain(a2, 0, 255);
        st.sig.speed    = (uint8_t)sm;
        st.holdActive = false;
        st.aux1Until = 0;
        st.holdNeutralThrottle = st.sig.throttle;
      } else {
        pcSig.throttle = (uint8_t)constrain(t,  0, 255);
        pcSig.roll     = (uint8_t)constrain(r,  0, 255);
        pcSig.pitch    = (uint8_t)constrain(p,  0, 255);
        pcSig.yaw      = (uint8_t)constrain(y,  0, 255);
        pcSig.aux1     = (uint8_t)constrain(a1, 0, 255);
        pcSig.aux2     = (uint8_t)constrain(a2, 0, 255);
        pcSig.speed = (uint8_t)sm;

        lastPcMs = millis();
        pcOverride = true;
        pcHoldActive = false; // KO: JOY는 연속 스트림 전송 필요 / EN: JOY should be streamed continuously
      }
      logPrintln("OK");
      return;
    }
    logPrintln("ERR");
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
        String pwrStr = rest.substring(0, sp2);
        pwrStr.trim();
        int pwr = 0;
        if(pwrStr.endsWith("%")){
          int pct = pwrStr.substring(0, pwrStr.length() - 1).toInt();
          pwr = map(constrain(pct, 0, 100), 0, 100, 0, 255);
        } else {
          pwr = pwrStr.toInt();
        }
        int tms = rest.substring(sp2 + 1).toInt();
        if(tms < 0) tms = -tms;
        tms = constrain(tms, 20, 15000);

        int d = powerToDelta(pwr);
        pcLastPower = (uint16_t)pwr;
        pcLastTimeMs = (uint16_t)tms;

        if(amcMode){
          // KO: AMC는 타겟 프로파일에 적용
          // EN: AMC applies to target profile
          AmcState &st = amcStates[targetProfile];
          uint8_t baseThr = st.sig.throttle;
          st.holdNeutralThrottle = baseThr;
          st.sig.speed = speedMode;
          st.sig.aux1 = 0;
          st.sig.aux2 = buildAux2();
          st.aux1Until = 0;

          // KO: 중립에서 시작 / EN: start from neutral
          setNeutral(st.sig, baseThr);

          if(cmd == CMD_UP){
            st.sig.throttle = (uint8_t)constrain((int)baseThr + d, 0, 255);
          } else if(cmd == CMD_DOWN){
            st.sig.throttle = (uint8_t)constrain((int)baseThr - d, 0, 255);
          } else if(cmd == CMD_FORWARD){
            st.sig.pitch = (uint8_t)constrain(127 + d, 0, 255);
          } else if(cmd == CMD_BACK){
            st.sig.pitch = (uint8_t)constrain(127 - d, 0, 255);
          } else if(cmd == CMD_RIGHT){
            st.sig.roll = (uint8_t)constrain(127 + d, 0, 255);
          } else if(cmd == CMD_LEFT){
            st.sig.roll = (uint8_t)constrain(127 - d, 0, 255);
          } else if(cmd == CMD_CW){
            st.sig.yaw = (uint8_t)constrain(127 + d, 0, 255);
          } else if(cmd == CMD_CCW || cmd == CMD_CC){
            st.sig.yaw = (uint8_t)constrain(127 - d, 0, 255);
          } else if(cmd == CMD_SET_THROTTLE){
            st.sig.throttle = (uint8_t)constrain(pwr, 0, 255);
          } else if(cmd == CMD_SET_ROLL){
            st.sig.roll = (uint8_t)constrain(pwr, 0, 255);
          } else if(cmd == CMD_SET_PITCH){
            st.sig.pitch = (uint8_t)constrain(pwr, 0, 255);
          } else if(cmd == CMD_SET_YAW){
            st.sig.yaw = (uint8_t)constrain(pwr, 0, 255);
          } else {
            logPrintln("ERR");
            return;
          }

          amcStartHold(targetProfile, now, (uint32_t)tms);
        } else {
          // KO: 기준 스로틀 = 현재 스틱 스로틀 / EN: base throttle = current stick throttle
          uint8_t baseThr = stickSig.throttle;
          pcHoldNeutralThrottle = baseThr;

          pcSig.speed = speedMode;
          pcSig.aux1 = 0;
          pcSig.aux2 = buildAux2();

          // KO: 중립에서 시작 / EN: start from neutral
          setNeutral(pcSig, baseThr);

          if(cmd == CMD_UP){
            pcSig.throttle = (uint8_t)constrain((int)baseThr + d, 0, 255);
          } else if(cmd == CMD_DOWN){
            pcSig.throttle = (uint8_t)constrain((int)baseThr - d, 0, 255);
          } else if(cmd == CMD_FORWARD){
            pcSig.pitch = (uint8_t)constrain(127 + d, 0, 255);
          } else if(cmd == CMD_BACK){
            pcSig.pitch = (uint8_t)constrain(127 - d, 0, 255);
          } else if(cmd == CMD_RIGHT){
            pcSig.roll = (uint8_t)constrain(127 + d, 0, 255);
          } else if(cmd == CMD_LEFT){
            pcSig.roll = (uint8_t)constrain(127 - d, 0, 255);
          } else if(cmd == CMD_CW){
            pcSig.yaw = (uint8_t)constrain(127 + d, 0, 255);
          } else if(cmd == CMD_CCW || cmd == CMD_CC){
            pcSig.yaw = (uint8_t)constrain(127 - d, 0, 255);
          } else if(cmd == CMD_SET_THROTTLE){
            pcSig.throttle = (uint8_t)constrain(pwr, 0, 255);
          } else if(cmd == CMD_SET_ROLL){
            pcSig.roll = (uint8_t)constrain(pwr, 0, 255);
          } else if(cmd == CMD_SET_PITCH){
            pcSig.pitch = (uint8_t)constrain(pwr, 0, 255);
          } else if(cmd == CMD_SET_YAW){
            pcSig.yaw = (uint8_t)constrain(pwr, 0, 255);
          } else {
            logPrintln("ERR");
            return;
          }

          startHold(millis(), (uint32_t)tms);
        }
        logPrintln("OK");
        return;
      }
    }
  }

  logPrintln("ERR");
}


// KO: PC 시리얼 수신 폴링
// EN: Poll PC serial input
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

// KO: 디버그 정보 출력
// EN: Print debug info
static void printDebugLine(){
  logPrint("DBG ");
  logPrint("T:"); logPrint((int)stickSig.throttle);
  logPrint(" R:"); logPrint((int)stickSig.roll);
  logPrint(" P:"); logPrint((int)stickSig.pitch);
  logPrint(" Y:"); logPrint((int)stickSig.yaw);
  logPrint(" SPD:"); logPrint((int)stickSig.speed);
  logPrint(" H:"); logPrint(headlessOn ? 1 : 0);
  logPrint(" F:"); logPrint(flowOn ? 1 : 0);
  logPrint(" VBAT:"); logPrint(vbat, 2); logPrint("V");
  logPrint(" MODE:"); logPrint(controlMode == MODE1 ? 1 : 2);
  logPrint(" LINK:"); logPrint(linkReady ? 1 : 0);
  logPrint(" PC:"); logPrint(pcOverride ? 1 : 0);
  logPrintln();
}

// KO: 배터리(TX 측) - 부팅 시 1회 측정
// EN: Battery (TX side) - measure once at boot
const float VBAT_LOW = 3.2f;   // KO: 필요 시 조정 / EN: adjust if needed
const uint8_t STICK_CENTER = 127;
const uint8_t STICK_THROTTLE_ACTIVE = 15; // KO: 필요 시 조정 / EN: adjust if needed

// ============================================================================
// KO: 헬퍼
// EN: Helpers
// ============================================================================
// KO: 버튼 상태 읽기 (Active LOW)
// EN: Read button state (Active LOW)
static inline bool btn(int p){ return digitalRead(p) == LOW; }

// KO: MUX 채널 읽기
// EN: Read MUX channel
static inline int readMux(uint8_t ch){
  digitalWrite(MUX_S0, bitRead(ch,0));
  if(MUX_S1 >= 0) digitalWrite(MUX_S1, bitRead(ch,1));
  if(MUX_S2 >= 0) digitalWrite(MUX_S2, bitRead(ch,2));
  delayMicroseconds(30);
  return analogRead(MUX_ADC);
}

// KO: ADC 값을 0~255 범위로 매핑 (중앙 데드존 적용)
// EN: Map ADC value to 0~255 (with center deadzone)
static inline uint8_t mapAxis(int v){
  int m = map(v, 0, 4095, 0, 255);
  if(m > 120 && m < 135) return 127; // KO: 중앙 데드존 / EN: deadzone around center
  return (uint8_t)constrain(m, 0, 255);
}

// KO: 배터리 전압 측정
// EN: Measure battery voltage
static inline float readBattery(){
  // KO: MUX ch1 배터리 입력(1/2 분압 가정)
  // EN: Battery via mux ch1, divider 1/2 assumed
  int raw = readMux(1);
  // KO: 배터리 읽은 뒤 기본 채널(레버)로 복귀
  // EN: Return to default channel (lever) after battery read
  readMux(0);
  return (raw / 4095.0f) * 3.3f * 2.0f;
}

// ============================================================================
// KO: 페어링 / EN: Pairing
// ============================================================================
// KO: 페어링 프로세스 실행
// EN: Run pairing process
static bool runPairing(){
  radio.stopListening();
  radio.setChannel(PAIR_CHANNEL);
  radio.openWritingPipe(PIPE_ADDR_PAIR);

  LinkPayload payload = {};
  const uint8_t* table = nextHopValid ? nextHopTable : hopTable;
  uint8_t len = nextHopValid ? nextHopLen : hopLen;
  buildLinkPayload(payload, table, len);

  bool pairedOk = false;
  while(!pairedOk){
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
// KO: LED 상태 업데이트
// EN: Update LED states
static void updateLEDs(){
  uint32_t t = millis();

  // KO: 부팅 시퀀스(0.5s 간격으로 LED1->LED2->LED3)
  // EN: boot sequence (0.5s per LED1->LED2->LED3)
  if(!bootAnimDone){
    uint32_t dt = t - bootAnimStartMs;
    if(dt < 1500){
      uint8_t stage = (uint8_t)(dt / 500); // 0,1,2
      digitalWrite(LED1_POWER, stage == 0 ? HIGH : LOW);
      digitalWrite(LED2_HEADLESS, stage == 1 ? HIGH : LOW);
      digitalWrite(LED3_TRIM, stage == 2 ? HIGH : LOW);
      return;
    } else {
      bootAnimDone = true;
    }
  }

  // KO: LED1 = 전원/바인딩/배터리 / EN: LED1 = power/bind/battery
  if(vbat < VBAT_LOW){
    // KO: 배터리 부족 -> 3번 깜박임 (2초 주기)
    // EN: Low battery -> 3 blinks (2s cycle)
    uint32_t cycle = t % 2000;
    bool on = (cycle < 200) || (cycle > 400 && cycle < 600) || (cycle > 800 && cycle < 1000);
    digitalWrite(LED1_POWER, on ? HIGH : LOW);
    digitalWrite(LED2_HEADLESS, LOW);
    digitalWrite(LED3_TRIM, LOW);
    return;
  }
  if(boundOK){
    // KO: 연결 완료 -> LED1 2번 깜박임 (2초 주기)
    // EN: Connected -> LED1 2 blinks (2s cycle)
    uint32_t cycle = t % 2000;
    bool on = (cycle < 200) || (cycle > 400 && cycle < 600);
    digitalWrite(LED1_POWER, on ? HIGH : LOW);
  } else {
    // KO: 미연결 -> LED1 켜짐 (전원 표시)
    // EN: Not bound -> LED1 ON (Power)
    digitalWrite(LED1_POWER, HIGH);
  }

  // KO: LED2 = 플립 준비 > 헤드리스
  // EN: LED2 = flip ready > headless
  if(flipReady){
    digitalWrite(LED2_HEADLESS, (t/500)%2); // KO: 0.5초 점멸 / EN: 0.5s blink
  } else if(headlessOn){
    digitalWrite(LED2_HEADLESS, HIGH); // KO: 헤드리스 ON = 고정 점등 / EN: headless ON = solid ON
  } else {
    digitalWrite(LED2_HEADLESS, LOW);
  }

  // KO: LED3 = AMC 모드(1초 점멸) > 트림 모드(고정 점등)
  // EN: LED3 = AMC mode (1s blink) > trim mode (solid ON)
  if(boundOK){
    if(amcMode){
      digitalWrite(LED3_TRIM, (t/1000)%2); // KO: 1초 ON / 1초 OFF / EN: 1s ON / 1s OFF
    } else if(trimMode){
      digitalWrite(LED3_TRIM, HIGH);     // KO: 트림 모드 ON = 고정 점등 / EN: trim mode ON = solid ON
    } else {
      digitalWrite(LED3_TRIM, LOW);
    }
  } else {
    // KO: 미연결 -> LED3 2초 ON / 1초 OFF
    // EN: Not bound -> LED3 2s ON / 1s OFF
    uint32_t cycle = t % 3000;
    digitalWrite(LED3_TRIM, (cycle < 2000) ? HIGH : LOW);
  }
}

// ============================================================================
// KO: 셋업 / EN: Setup
// ============================================================================
// KO: 초기화 설정
// EN: Setup initialization
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

  // KO: ADC 해상도를 12비트(0~4095)로 설정 (RP2040 기본은 10비트)
  // EN: Set ADC resolution to 12-bit (0~4095)
  analogReadResolution(12);

  // KO: 조이스틱 초기화 (범위 0~255)
  // EN: Initialize Joystick (range 0~255)
  Joystick.begin();

  pinMode(MUX_S0, OUTPUT);
  if(MUX_S1 >= 0) pinMode(MUX_S1, OUTPUT);
  if(MUX_S2 >= 0) pinMode(MUX_S2, OUTPUT);
  // KO: 기본값은 레버 채널(A0)
  // EN: Default to lever channel (A0)
  digitalWrite(MUX_S0, LOW);

  pinMode(BTN_PAIR_SPEED, INPUT_PULLUP);
  pinMode(BTN_AUTO, INPUT_PULLUP);
  pinMode(BTN_TRIM_MODE, INPUT_PULLUP);
  pinMode(BTN_HEADLESS, INPUT_PULLUP);
  pinMode(BTN_FLOW_TOGGLE, INPUT_PULLUP);
  pinMode(BTN_LED_SERVO, INPUT_PULLUP);

  pinMode(LED1_POWER, OUTPUT);
  pinMode(LED2_HEADLESS, OUTPUT);
  pinMode(LED3_TRIM, OUTPUT);

  // KO: 부팅 시 공장초기화 요청(GPIO11 누름)
  // EN: Factory reset request on boot (GPIO11 held)
  factoryResetReq = btn(BTN_AUTO);

  // KO: 부트 모드 선택(GPIO14 전원 켤 때 누름)
  // EN: Boot mode select (GPIO14 held at power on)
  controlMode = btn(BTN_TRIM_MODE) ? MODE1 : MODE2;

  // KO: RF 초기화 / EN: RF init
  radio.begin();
  radio.setChannel(PAIR_CHANNEL);
  radio.setDataRate(RF24_250KBPS);
  radio.setPALevel(RF24_PA_LOW);
  // KO: 부팅 시 GPIO10(Flow 버튼) 홀드하면 D1~D4 ID 재생성
  // EN: Hold GPIO10 (Flow button) on boot to regenerate D1..D4 IDs
  bool regenAll = btn(BTN_FLOW_TOGGLE);
  initProfiles(regenAll);
  currentProfile = 0; // D1
  loadProfileRuntime(currentProfile, millis());
  nextHopValid = false;
  pendingHopSave = false;

  // KO: 부팅 시 페어링(GPIO15 누름)
  // EN: Pairing on boot (GPIO15 held)
  if(btn(BTN_PAIR_SPEED)){
    scanAndBuildHopTable(hopTable, hopLen);
    profiles[currentProfile].hopLen = hopLen;
    for(uint8_t i = 0; i < HOP_MAX; i++) profiles[currentProfile].hopTable[i] = hopTable[i];
    saveHopTableToEepromProfile(currentProfile, profiles[currentProfile].hopTable, profiles[currentProfile].hopLen);
    nextHopValid = false;
    bool pairedOk = runPairing();
    linkReady = pairedOk;
    profileLinkReady[currentProfile] = pairedOk;
    profileBoundOK[currentProfile] = false;
    pendingHopSave = true;
    profilePendingHopSave[currentProfile] = true;
    // KO: 페어링만으로 바인딩 보장 X; 쓰기 성공 후 boundOK
    // EN: Pairing alone doesn't guarantee bind; boundOK after successful writes
  }

  stickSig = {};
  stickSig.speed = speedMode;
  pcSig = {};
  pcSig.speed = 1;

  // KO: 배터리 부팅 시 1회 측정
  // EN: Measure battery once at boot
  if(!usbSessionActive()){
    vbat = readBattery();
  }

  // KO: 초기 LED 표시 / EN: show initial LEDs
  bootAnimStartMs = millis();
  bootAnimDone = false;
  updateLEDs();
}

// ============================================================================
// KO: 루프
// EN: Loop
// ============================================================================
// KO: 메인 루프
// EN: Main loop
void loop(){
  uint32_t now = millis();

  // KO: USB 시리얼이 열릴 때 배너 1회 출력
  // EN: Print banner once when USB serial opens
  static bool lastUsbActive = false;
  bool usbActive = usbSessionActive();
  if(usbActive && !lastUsbActive){
    printBanner();
  }
  lastUsbActive = usbActive;


  // ==========================================================================
  // KO: 물리 스틱 읽기 → stickSig
  // EN: Read physical sticks -> stickSig
  // ==========================================================================
  int rawThr = analogRead(ADC_THROTTLE);
  int rawYaw = analogRead(ADC_YAW);
  int rawPit = analogRead(ADC_PITCH);
  int rawRol = readMux(0);

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
      aux1UserUntil = 0;
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
  pcSig.speed = speedMode;
  for(uint8_t i = 0; i < PROFILE_COUNT; i++){
    amcStates[i].sig.speed = speedMode;
  }

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

  // KO: 사용자 버튼(GPIO12) - 짧게: LED, 길게: 서보
  // EN: User button (GPIO12) - short: LED, long: servo
  static bool lastUserBtn = false;
  static uint32_t userPressedAt = 0;
  static bool userLongFired = false;
  bool ub = btn(BTN_LED_SERVO);
  if(ub && !lastUserBtn){
    userPressedAt = now;
    userLongFired = false;
  }
  if(ub && !userLongFired && (now - userPressedAt >= USER_LONG_MS)){
    userLongFired = true;
    aux1UserValue = AUX1_SERVO_TOGGLE;
    aux1UserUntil = now + AUX_PULSE;
  }
  if(!ub && lastUserBtn){
    if(!userLongFired){
      aux1UserValue = AUX1_LED_STEP;
      aux1UserUntil = now + AUX_PULSE;
    }
  }
  lastUserBtn = ub;

  // KO: 플립/헤드리스 (GPIO13) - 짧게: 플립 준비, 길게: 헤드리스 토글
  // EN: flip/headless (GPIO13) - short: flip ready, long: headless toggle
  static bool lastHead = false;
  static uint32_t headPressedAt = 0;
  static bool headLongFired = false;
  bool h = btn(BTN_HEADLESS);
  if(h && !lastHead){
    headPressedAt = now;
    headLongFired = false;
  }
  if(h && !headLongFired && (now - headPressedAt >= HEADLESS_LONG_MS)){
    headLongFired = true;
    headlessOn = !headlessOn;
    flipReady = false;
    syncAux2All();
  }
  if(!h && lastHead){
    if(!headLongFired){
      if(headlessOn){
        flipReady = false;
      } else if(flipReady){
        flipReady = false;
      } else {
        flipReady = true;
        flipReadyStartMs = now;
      }
      syncAux2All();
    }
  }
  lastHead = h;

  // KO: 옵티컬 플로우/비상정지 (GPIO10) - 짧게: Flow 토글, 길게: 긴급정지
  // EN: flow/emergency (GPIO10) - short: flow toggle, long: emergency
  static bool lastFlow = false;
  static uint32_t flowPressedAt = 0;
  static bool flowLongFired = false;
  bool f = btn(BTN_FLOW_TOGGLE);
  if(f && !lastFlow){
    flowPressedAt = now;
    flowLongFired = false;
  }
  if(f && !flowLongFired && (now - flowPressedAt >= FLOW_LONG_MS)){
    flowLongFired = true;
    applyEmergency();
  }
  if(!f && lastFlow){
    if(!flowLongFired){
      flowOn = !flowOn;
      syncAux2All();
    }
  }
  lastFlow = f;

  // KO: 플립 준비 상태 처리 (3초 타임아웃)
  // EN: flip ready handling (3s timeout)
  if(flipReady){
    if(now - flipReadyStartMs >= FLIP_READY_TIMEOUT_MS){
      flipReady = false;
    } else {
      int dRoll = (int)stickSig.roll - STICK_CENTER;
      int dPitch = (int)stickSig.pitch - STICK_CENTER;
      int absR = abs(dRoll);
      int absP = abs(dPitch);
      if(absR > FLIP_DIR_DEADZONE || absP > FLIP_DIR_DEADZONE){
        if(absR >= absP){
          aux1UserValue = dRoll > 0 ? AUX1_FLIP_ROLL_POS : AUX1_FLIP_ROLL_NEG;
        } else {
          aux1UserValue = dPitch > 0 ? AUX1_FLIP_PITCH_POS : AUX1_FLIP_PITCH_NEG;
        }
        aux1UserUntil = now + AUX_PULSE;
        flipReady = false;
      }
    }
  }

  stickSig.aux2 = buildAux2();
  syncAux2All();

  uint8_t aux1Cmd = 0;
  if(now < gyroResetUntil){
    aux1Cmd = AUX1_GYRO_RESET; // KO: 자이로 초기화 요청 / EN: gyro init request
  } else if(now < aux1UserUntil){
    aux1Cmd = aux1UserValue;
  } else if(now < aux1Until){
    aux1Cmd = AUX1_TAKEOFF;
  }
  stickSig.aux1 = aux1Cmd;

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
  // KO: 물리적 오차를 고려하여 120~130 범위를 중립(비활성)으로 처리
  // EN: Treat 120~130 range as neutral (inactive) considering physical errors
  bool rollActive  = (stickSig.roll < 120 || stickSig.roll > 130);
  bool pitchActive = (stickSig.pitch < 120 || stickSig.pitch > 130);
  bool yawActive   = (stickSig.yaw < 120 || stickSig.yaw > 130);
  
  // KO: 스로틀이 0(최하단)이거나 중립 범위(120~130)이면 비활성으로 간주
  bool throttleActive = (stickSig.throttle > STICK_THROTTLE_ACTIVE) && (stickSig.throttle < 120 || stickSig.throttle > 130);
  
  stickActive = rollActive || pitchActive || yawActive || throttleActive;
  if(stickActive && !amcMode){
    pcOverride = false;
    pcHoldActive = false;
  }

  // KO: 스틱 값 실시간 디버깅 출력 (200ms 간격)
  // EN: Real-time stick value debug output (200ms interval)
  static uint32_t stickDebugMs = 0;
  if(vlrEnabled && (now - stickDebugMs > 200)){
    stickDebugMs = now;
    logPrint("STK T:"); logPrint((int)stickSig.throttle);
    logPrint(" Y:"); logPrint((int)stickSig.yaw);
    logPrint(" P:"); logPrint((int)stickSig.pitch);
    logPrint(" R:"); logPrint((int)stickSig.roll);
    logPrint(" ACT:"); logPrint(stickActive);
    logPrint(" B:");
    logPrint((int)btn(BTN_AUTO)); logPrint(" ");
    logPrint((int)btn(BTN_HEADLESS)); logPrint(" ");
    logPrint((int)btn(BTN_FLOW_TOGGLE)); logPrint(" ");
    logPrint((int)btn(BTN_LED_SERVO)); logPrint(" ");
    logPrint((int)btn(BTN_TRIM_MODE)); logPrint(" ");
    logPrintln((int)btn(BTN_PAIR_SPEED));
  }

  // KO: 스틱 제어 경로에만 트림 오프셋 적용
  // EN: apply trim offsets only to stick control path
  stickSig.roll  = (uint8_t)constrain((int)stickSig.roll  + trimRoll,  0, 255);
  stickSig.pitch = (uint8_t)constrain((int)stickSig.pitch + trimPitch, 0, 255);

  // KO: 조이스틱 상태 업데이트
  // EN: Update Joystick state
  // KO: RP2040 내장 라이브러리는 -127~127 범위를 사용하므로 변환
  Joystick.X(map(stickSig.roll, 0, 255, -127, 127));
  Joystick.Y(map(stickSig.pitch, 0, 255, -127, 127));
  Joystick.Z(map(stickSig.throttle, 0, 255, -127, 127));
  Joystick.Zrotate(map(stickSig.yaw, 0, 255, -127, 127));
  Joystick.button(1, btn(BTN_AUTO));      // RP2040 Lib: Button 1~32
  Joystick.button(2, btn(BTN_HEADLESS));
  Joystick.button(3, btn(BTN_FLOW_TOGGLE));
  Joystick.button(4, btn(BTN_LED_SERVO));
  Joystick.button(5, btn(BTN_TRIM_MODE));
  Joystick.button(6, btn(BTN_PAIR_SPEED));

  // KO: PC 시리얼 폴링
  // EN: PC serial poll
  pollPcSerial();

  // KO: dd3 홀드가 pcHoldUntil까지 PC 오버라이드 유지
  // EN: dd3 hold keeps PC override alive until pcHoldUntil
  if(!amcMode && pcHoldActive){
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
  if(!amcMode && pcOverride && (now - lastPcMs > PC_TIMEOUT_MS)){
    pcOverride = false;
  }

  if(debugEnabled && (int32_t)(now - debugNextMs) >= 0){
    debugNextMs = now + 1000;
    printDebugLine();
  }

  // KO: 전체 비상정지(AMC ON/OFF 모두)
  // EN: global emergency (AMC on/off)
  if(emergencyAll){
    sendEmergencyAll(now);
    emergencyAll = false;
    emergencyPulse = false;
    updateLEDs();
    delay(20);
    return;
  }

  if(amcMode){
    for(uint8_t i = 0; i < PROFILE_COUNT; i++){
      Signal s = buildAmcSignal(i, now);
      // KO: AUX2 비트필드(bit0=headless, bit1=flow, bit2=flip ready)
      // EN: AUX2 bitfield (bit0=headless, bit1=flow, bit2=flip ready)
      s.aux2 = buildAux2();
      if(now < gyroResetUntil){
        s.aux1 = AUX1_GYRO_RESET; // KO: 자이로 초기화 요청 / EN: gyro init request
      }
      sendSignalForProfile(i, s, now);
    }
    loadProfileRuntime(currentProfile, now);
    updateLEDs();
    delay(20);
    return;
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
  // KO: AUX2 비트필드(bit0=headless, bit1=flow, bit2=flip ready)
  // EN: AUX2 bitfield (bit0=headless, bit1=flow, bit2=flip ready)
  outSig.aux2 = buildAux2();
  if(emergencyPulse){
    outSig = makeEmergencySignal();
  }
  if(!emergencyPulse && (now < gyroResetUntil)){
    outSig.aux1 = AUX1_GYRO_RESET; // KO: 자이로 초기화 요청 / EN: gyro init request
  }

  sendSignalForProfile(currentProfile, outSig, now);

  if(emergencyPulse) emergencyPulse = false;

  // KO: LED가 헤드리스/트림 상태 반영 유지
  // EN: keep LEDs reflecting headless/trim state
  updateLEDs();
  delay(20);
}
