/*
  Reflex Arcade - game engine for the Arduino Mega 2560 (Mega + ESP8266 board)

  The Mega owns everything time-critical: it reads 16 buttons, drives the
  lights, runs the game clock and measures reaction times. The ESP8266 only
  relays text lines between the Mega and the web page, so a Wi-Fi hiccup never
  freezes or skews a game.

  Talks the same line protocol on two ports:
    Serial  (USB, 115200)   - for debugging from a computer
    Serial3 (ESP8266, 115200) - for the web page, via the ESP8266

  Commands (one per line):
    START single|multi <targets 1-4> same|ind <seconds>
    STOP                       back to idle
    STATE?                     send the pin map and the state now
    DIAG                       enter diagnostic mode
    LIGHT <player 0-1> <button 0-7> <0|1|t>   set a light (enters DIAG)
    ALL <0|1>                  all lights off/on (enters DIAG)
    RESET                      clear diagnostic press counters
    IP <address>               the ESP8266 reports its IP address
    WIFI <status>              the ESP8266 reports its Wi-Fi status (3 = connected)
    NET...                     (USB only) Wi-Fi settings, passed on to the ESP8266

  Hold player 1 button #1 for 5 s while no game runs: the ESP8266 switches on
  its "ArcadeGame" hotspot (all lights blink to confirm).

  Messages sent (one JSON object per line):
    {"t":"pins","shared":0,"btn":[[22,...],[30,...]],"led":[[38,...],[46,...]]}
    {"t":"state",...}  full state, on every change and every 500 ms
    {"t":"hit","p":0,"b":3,"ms":412}
    {"t":"miss","p":1,"b":2}
    {"t":"over","win":0}       win: 0 or 1, -1 for a draw or single player
*/

#include "game_types.h"

// ======================================================================
// Wiring - change pins here
// ======================================================================

// Each button: one leg to the pin, the other to GND (internal pull-ups used).
const uint8_t BTN_PINS[PLAYERS][KEYS] = {
  {22, 23, 24, 25, 26, 27, 28, 29},  // player 1
  {30, 31, 32, 33, 34, 35, 36, 37},  // player 2
};

// Each light: pin -> 220 ohm resistor -> LED -> GND (or a driver input).
const uint8_t LED_PINS[PLAYERS][KEYS] = {
  {38, 39, 40, 41, 42, 43, 44, 45},  // player 1
  {46, 47, 48, 49, 50, 51, 52, 53},  // player 2
};

// 1 = only 8 lights in total (player 1 pins), shared by both players.
//     Light N then means "button N" for both players, and the
//     "different targets per player" option is turned off.
#define SHARED_LIGHTS 0

// 0 if your light driver (e.g. a relay board) switches on with LOW.
#define LED_ACTIVE_HIGH 1

#define ESP_SERIAL Serial3

// ======================================================================
// Tuning
// ======================================================================

const uint32_t BAUD = 115200;
const uint8_t DEBOUNCE_MS = 15;
const uint16_t RACE_GRACE_MS = 150;   // same-target mode: a late press on a just-taken target is not a miss
const uint16_t COUNTDOWN_MS = 3000;
const uint16_t RESULT_BLINK_MS = 3000;
const uint16_t STATE_EVERY_MS = 500;
const uint8_t STATE_MIN_GAP_MS = 80;
const uint8_t MAX_TARGETS = 4;
const uint16_t HOTSPOT_HOLD_MS = 5000;

// ======================================================================
// Non-blocking serial output (keeps button timing exact while sending)
// ======================================================================

struct TxQueue {
  static const uint16_t SIZE = 900;
  char buf[SIZE];
  uint16_t head = 0, tail = 0, count = 0;

  bool push(const char *s, uint16_t len) {
    if (SIZE - count < len + 1) return false;  // drop whole line, state is resent anyway
    for (uint16_t i = 0; i < len; i++) put(s[i]);
    put('\n');
    return true;
  }
  void put(char c) {
    buf[head] = c;
    head = (head + 1) % SIZE;
    count++;
  }
  void pump(HardwareSerial &port) {
    int room = port.availableForWrite();
    while (room-- > 0 && count) {
      port.write(buf[tail]);
      tail = (tail + 1) % SIZE;
      count--;
    }
  }
};

TxQueue usbTx, espTx;

void sendLine(const char *s) {
  uint16_t len = strlen(s);
  usbTx.push(s, len);
  espTx.push(s, len);
}

// ======================================================================
// State
// ======================================================================

const char *const PHASE_NAMES[] = {"idle", "countdown", "play", "over", "diag"};

Key keys[PLAYERS][KEYS];
Player pl[PLAYERS];
Config cfg = {false, 1, true, 60};

Phase phase = IDLE;
uint32_t phaseAt = 0;
uint32_t gameEndAt = 0;
int8_t winner = -1;

uint8_t sharedLit = 0;          // targets (same-target mode)
uint32_t sharedLitAt[KEYS];
uint32_t takenAt[KEYS];

uint8_t diagLit[PLAYERS] = {0, 0};
uint16_t diagCount[PLAYERS][KEYS];

uint8_t shown[PLAYERS] = {0, 0};      // lights the player should see (logical)
uint8_t written[PLAYERS] = {0xFF, 0xFF};
char ipAddr[16] = "";
int8_t wifiStatus = -1;       // from the ESP8266: 3 connected, 1 network not found, 4 wrong password, -1 nothing heard
uint32_t espBytes = 0;        // bytes received from the ESP8266, to check the link

bool dirty = true;
uint32_t lastStateAt = 0;

bool hotspotReqSent = false;
uint32_t hotspotFlashUntil = 0;

// ======================================================================
// Helpers
// ======================================================================

inline uint8_t bitOf(uint8_t i) { return (uint8_t)(1u << i); }

bool playerActive(uint8_t p) { return p == 0 || cfg.multi; }
bool sameTargets() { return cfg.multi && (cfg.same || SHARED_LIGHTS); }

int16_t scoreOf(uint8_t p) { return (int16_t)pl[p].hits - (int16_t)pl[p].misses; }

uint8_t popcount8(uint8_t v) {
  uint8_t c = 0;
  while (v) { c += v & 1; v >>= 1; }
  return c;
}

// Lights a new random target that is not already lit and not `exclude`.
void addTarget(uint8_t &mask, uint32_t *litAt, int8_t exclude, uint32_t now) {
  uint8_t free[KEYS];
  uint8_t n = 0;
  for (uint8_t i = 0; i < KEYS; i++) {
    if (!(mask & bitOf(i)) && i != exclude) free[n++] = i;
  }
  if (!n) return;
  uint8_t pick = free[random(n)];
  mask |= bitOf(pick);
  litAt[pick] = now;
}

void fillTargets(uint8_t &mask, uint32_t *litAt, int8_t exclude, uint32_t now) {
  while (popcount8(mask) < cfg.targets) {
    uint8_t before = mask;
    addTarget(mask, litAt, exclude, now);
    if (mask == before) break;
  }
}

void setPhase(Phase p, uint32_t now) {
  phase = p;
  phaseAt = now;
  dirty = true;
}

// ======================================================================
// Game flow
// ======================================================================

void clearTargets() {
  sharedLit = 0;
  for (uint8_t p = 0; p < PLAYERS; p++) pl[p].lit = 0;
}

void startGame(const Config &c, uint32_t now) {
  cfg = c;
  randomSeed(micros() ^ analogRead(A0) ^ ((uint32_t)analogRead(A1) << 10));
  clearTargets();
  for (uint8_t p = 0; p < PLAYERS; p++) {
    pl[p].hits = pl[p].misses = 0;
    pl[p].reactSum = 0;
    pl[p].best = 0xFFFF;
  }
  for (uint8_t i = 0; i < KEYS; i++) takenAt[i] = 0;
  winner = -1;
  setPhase(COUNTDOWN, now);
}

void beginPlay(uint32_t now) {
  gameEndAt = now + (uint32_t)cfg.seconds * 1000UL;
  if (sameTargets()) {
    fillTargets(sharedLit, sharedLitAt, -1, now);
  } else {
    for (uint8_t p = 0; p < PLAYERS; p++) {
      if (playerActive(p)) fillTargets(pl[p].lit, pl[p].litAt, -1, now);
    }
  }
  setPhase(PLAY, now);
}

void endGame(uint32_t now) {
  clearTargets();
  winner = -1;
  if (cfg.multi) {
    int16_t a = scoreOf(0), b = scoreOf(1);
    if (a > b) winner = 0;
    else if (b > a) winner = 1;
  }
  char b[32];
  snprintf(b, sizeof(b), "{\"t\":\"over\",\"win\":%d}", winner);
  sendLine(b);
  setPhase(OVER, now);
}

void registerHit(uint8_t p, uint8_t i, uint32_t reaction) {
  Player &P = pl[p];
  P.hits++;
  P.reactSum += reaction;
  if (reaction < P.best) P.best = reaction;
  char b[56];
  snprintf(b, sizeof(b), "{\"t\":\"hit\",\"p\":%u,\"b\":%u,\"ms\":%lu}", p, i, reaction);
  sendLine(b);
  dirty = true;
}

void registerMiss(uint8_t p, uint8_t i) {
  pl[p].misses++;
  char b[40];
  snprintf(b, sizeof(b), "{\"t\":\"miss\",\"p\":%u,\"b\":%u}", p, i);
  sendLine(b);
  dirty = true;
}

// `at` is when the contact first closed, so debouncing doesn't add to reaction time.
void onPress(uint8_t p, uint8_t i, uint32_t at) {
  if (phase == DIAG) {
    diagCount[p][i]++;
    return;
  }
  if (phase != PLAY || !playerActive(p)) return;

  uint8_t m = bitOf(i);
  if (sameTargets()) {
    if (sharedLit & m) {
      sharedLit &= ~m;
      takenAt[i] = at;
      uint32_t r = at > sharedLitAt[i] ? at - sharedLitAt[i] : 0;
      fillTargets(sharedLit, sharedLitAt, i, millis());
      registerHit(p, i, r);
    } else if (at - takenAt[i] > RACE_GRACE_MS) {
      registerMiss(p, i);
    }
  } else {
    Player &P = pl[p];
    if (P.lit & m) {
      P.lit &= ~m;
      uint32_t r = at > P.litAt[i] ? at - P.litAt[i] : 0;
      fillTargets(P.lit, P.litAt, i, millis());
      registerHit(p, i, r);
    } else {
      registerMiss(p, i);
    }
  }
}

// ======================================================================
// Inputs and outputs
// ======================================================================

void scanKeys(uint32_t now) {
  for (uint8_t p = 0; p < PLAYERS; p++) {
    for (uint8_t i = 0; i < KEYS; i++) {
      Key &k = keys[p][i];
      bool raw = digitalRead(BTN_PINS[p][i]) == LOW;
      if (raw != k.raw) {
        k.raw = raw;
        k.changedAt = now;
      } else if (raw != k.down && now - k.changedAt >= DEBOUNCE_MS) {
        k.down = raw;
        if (phase == DIAG) dirty = true;
        if (raw) onPress(p, i, k.changedAt);
      }
    }
  }
}

// Holding player 1 button #1 for 5 s while no game runs asks the ESP8266 to
// switch on its "ArcadeGame" hotspot. All lights blink to confirm.
void checkHotspotHold(uint32_t now) {
  const Key &k = keys[0][0];
  if (!k.down) {
    hotspotReqSent = false;
    return;
  }
  if (hotspotReqSent || (phase != IDLE && phase != OVER)) return;
  if (now - k.changedAt >= HOTSPOT_HOLD_MS) {
    hotspotReqSent = true;
    sendLine("{\"t\":\"apreq\"}");
    hotspotFlashUntil = now + 1500;
  }
}

uint8_t pressedMask(uint8_t p) {
  uint8_t m = 0;
  for (uint8_t i = 0; i < KEYS; i++) if (keys[p][i].down) m |= bitOf(i);
  return m;
}

void updateLights(uint32_t now) {
  uint8_t o[PLAYERS] = {0, 0};
  uint32_t t = now - phaseAt;

  switch (phase) {
    case IDLE:
      if ((int32_t)(hotspotFlashUntil - now) > 0 && (now / 150) % 2 == 0) o[0] = o[1] = 0xFF;
      break;
    case COUNTDOWN: {
      // Blink once per second, and light fewer lamps each second: 8, 5, 2.
      bool on = (t % 1000) < 450;
      uint8_t step = t / 1000 > 2 ? 2 : t / 1000;
      uint8_t n = 8 - 3 * step;
      uint8_t mask = on ? (uint8_t)((1u << n) - 1) : 0;
      for (uint8_t p = 0; p < PLAYERS; p++) if (playerActive(p)) o[p] = mask;
      break;
    }
    case PLAY:
      for (uint8_t p = 0; p < PLAYERS; p++) {
        if (!playerActive(p)) continue;
        o[p] = sameTargets() ? sharedLit : pl[p].lit;
      }
      break;
    case OVER:
      if (t < RESULT_BLINK_MS && (t / 200) % 2 == 0) {
        for (uint8_t p = 0; p < PLAYERS; p++) {
          if (playerActive(p) && (winner < 0 || winner == p)) o[p] = 0xFF;
        }
      }
      break;
    case DIAG:
      o[0] = diagLit[0];
      o[1] = diagLit[1];
      break;
  }

  for (uint8_t p = 0; p < PLAYERS; p++) shown[p] = o[p];

#if SHARED_LIGHTS
  o[0] |= o[1];
  o[1] = 0;
#endif

  for (uint8_t p = 0; p < PLAYERS; p++) {
    if (o[p] == written[p]) continue;
#if SHARED_LIGHTS
    if (p == 1) { written[1] = 0; continue; }
#endif
    for (uint8_t i = 0; i < KEYS; i++) {
      bool on = o[p] & bitOf(i);
      digitalWrite(LED_PINS[p][i], (on == (bool)LED_ACTIVE_HIGH) ? HIGH : LOW);
    }
    written[p] = o[p];
  }
}

// ======================================================================
// State message
// ======================================================================

void sendState(uint32_t now) {
  char b[560];
  uint32_t leftMs = 0;
  if (phase == PLAY && gameEndAt > now) leftMs = gameEndAt - now;
  if (phase == COUNTDOWN && COUNTDOWN_MS > now - phaseAt) leftMs = COUNTDOWN_MS - (now - phaseAt);

  int n = snprintf(b, sizeof(b),
                   "{\"t\":\"state\",\"ph\":\"%s\",\"multi\":%d,\"n\":%u,\"same\":%d,\"dur\":%u,"
                   "\"left\":%lu,\"win\":%d,\"shared\":%d,\"ip\":\"%s\",\"wifi\":%d,\"espRx\":%lu,\"p\":[",
                   PHASE_NAMES[phase], cfg.multi, cfg.targets, sameTargets() ? 1 : (cfg.same ? 1 : 0),
                   cfg.seconds, leftMs, winner, SHARED_LIGHTS, ipAddr, wifiStatus, espBytes);

  for (uint8_t p = 0; p < PLAYERS && n > 0 && n < (int)sizeof(b); p++) {
    const Player &P = pl[p];
    uint16_t avg = P.hits ? (uint16_t)(P.reactSum / P.hits) : 0;
    uint16_t best = P.best == 0xFFFF ? 0 : P.best;
    const uint16_t *c = diagCount[p];
    n += snprintf(b + n, sizeof(b) - n,
                  "%s{\"h\":%u,\"m\":%u,\"s\":%d,\"avg\":%u,\"best\":%u,\"lit\":%u,\"btn\":%u,"
                  "\"cnt\":[%u,%u,%u,%u,%u,%u,%u,%u]}",
                  p ? "," : "", P.hits, P.misses, scoreOf(p), avg, best, shown[p], pressedMask(p),
                  c[0], c[1], c[2], c[3], c[4], c[5], c[6], c[7]);
  }
  if (n > 0 && n < (int)sizeof(b) - 3) {
    strcat(b, "]}");
    sendLine(b);
  }
  lastStateAt = now;
  dirty = false;
}

// Pin map, so the page can label every button with its Arduino pin.
void sendPins() {
  char b[260];
  int n = snprintf(b, sizeof(b), "{\"t\":\"pins\",\"shared\":%d,\"btn\":[", SHARED_LIGHTS);
  for (uint8_t p = 0; p < PLAYERS; p++) {
    n += snprintf(b + n, sizeof(b) - n, "%s[", p ? "," : "");
    for (uint8_t i = 0; i < KEYS; i++) n += snprintf(b + n, sizeof(b) - n, "%s%u", i ? "," : "", BTN_PINS[p][i]);
    n += snprintf(b + n, sizeof(b) - n, "]");
  }
  n += snprintf(b + n, sizeof(b) - n, "],\"led\":[");
  for (uint8_t p = 0; p < PLAYERS; p++) {
    n += snprintf(b + n, sizeof(b) - n, "%s[", p ? "," : "");
    for (uint8_t i = 0; i < KEYS; i++) {
      uint8_t pin = SHARED_LIGHTS ? LED_PINS[0][i] : LED_PINS[p][i];
      n += snprintf(b + n, sizeof(b) - n, "%s%u", i ? "," : "", pin);
    }
    n += snprintf(b + n, sizeof(b) - n, "]");
  }
  if (n > 0 && n < (int)sizeof(b) - 3) {
    strcat(b, "]}");
    sendLine(b);
  }
}

// ======================================================================
// Commands
// ======================================================================

void enterDiag(uint32_t now) {
  if (phase != DIAG) {
    clearTargets();
    diagLit[0] = diagLit[1] = 0;
    setPhase(DIAG, now);
  }
}

void handleLine(char *line) {
  uint32_t now = millis();
  char *cmd = strtok(line, " ");
  if (!cmd) return;

  if (!strcmp(cmd, "START")) {
    char *mode = strtok(NULL, " ");
    char *nStr = strtok(NULL, " ");
    char *pat = strtok(NULL, " ");
    char *sec = strtok(NULL, " ");
    if (!mode || !nStr || !pat || !sec) return;
    Config c;
    c.multi = !strcmp(mode, "multi");
    c.targets = constrain(atoi(nStr), 1, MAX_TARGETS);
    c.same = strcmp(pat, "ind") != 0;
    c.seconds = constrain(atol(sec), 5, 3600);
    startGame(c, now);
  } else if (!strcmp(cmd, "STOP")) {
    clearTargets();
    diagLit[0] = diagLit[1] = 0;
    setPhase(IDLE, now);
  } else if (!strcmp(cmd, "STATE?")) {
    sendPins();
    dirty = true;
  } else if (!strcmp(cmd, "DIAG")) {
    enterDiag(now);
  } else if (!strcmp(cmd, "LIGHT")) {
    char *pS = strtok(NULL, " ");
    char *iS = strtok(NULL, " ");
    char *vS = strtok(NULL, " ");
    if (!pS || !iS || !vS) return;
    int p = atoi(pS), i = atoi(iS);
    if (p < 0 || p >= PLAYERS || i < 0 || i >= KEYS) return;
    enterDiag(now);
    uint8_t m = bitOf(i);
    if (vS[0] == 't') diagLit[p] ^= m;
    else if (atoi(vS)) diagLit[p] |= m;
    else diagLit[p] &= ~m;
    dirty = true;
  } else if (!strcmp(cmd, "ALL")) {
    char *vS = strtok(NULL, " ");
    if (!vS) return;
    enterDiag(now);
    diagLit[0] = diagLit[1] = atoi(vS) ? 0xFF : 0;
    dirty = true;
  } else if (!strcmp(cmd, "RESET")) {
    memset(diagCount, 0, sizeof(diagCount));
    dirty = true;
  } else if (!strcmp(cmd, "WIFI")) {
    char *s = strtok(NULL, " ");
    if (!s) return;
    wifiStatus = atoi(s);
    dirty = true;
  } else if (!strcmp(cmd, "IP")) {
    char *a = strtok(NULL, " ");
    if (!a) return;
    if (a[0] == '-') a[0] = 0;   // no address yet
    strncpy(ipAddr, a, sizeof(ipAddr) - 1);
    ipAddr[sizeof(ipAddr) - 1] = 0;
    dirty = true;
  }
  // Anything else (e.g. ESP8266 boot noise) is ignored.
}

// Reads lines from one port.
//  - USB port: lines starting with NET are passed on to the ESP8266 (Wi-Fi settings).
//  - ESP port: JSON lines from the ESP8266 (network status) are passed on to USB.
template <uint16_t SIZE>
struct LineReader {
  char buf[SIZE];
  uint16_t len = 0;
  bool overflow = false;
  uint32_t bytes = 0;

  void poll(HardwareSerial &port, bool fromUsb) {
    while (port.available()) {
      char c = port.read();
      bytes++;
      if (c == '\n' || c == '\r') {
        if (len && !overflow) {
          buf[len] = 0;
          if (fromUsb && !strncmp(buf, "NET", 3)) espTx.push(buf, len);
          else if (!fromUsb && buf[0] == '{') usbTx.push(buf, len);
          else handleLine(buf);
        }
        len = 0;
        overflow = false;
      } else if (len < SIZE - 1) {
        buf[len++] = c;
      } else {
        overflow = true;
      }
    }
  }
};

LineReader<160> usbRx;
LineReader<420> espRx;

// ======================================================================
// Arduino entry points
// ======================================================================

void setup() {
  Serial.begin(BAUD);
  ESP_SERIAL.begin(BAUD);
  for (uint8_t p = 0; p < PLAYERS; p++) {
    for (uint8_t i = 0; i < KEYS; i++) {
      pinMode(BTN_PINS[p][i], INPUT_PULLUP);
      pinMode(LED_PINS[p][i], OUTPUT);
      keys[p][i] = {false, false, 0};
    }
  }
  randomSeed(analogRead(A0));
  updateLights(millis());
  sendLine("{\"t\":\"boot\"}");
  sendPins();
}

void loop() {
  uint32_t now = millis();

  usbRx.poll(Serial, true);
  espRx.poll(ESP_SERIAL, false);
  espBytes = espRx.bytes;
  scanKeys(now);
  checkHotspotHold(now);

  if (phase == COUNTDOWN && now - phaseAt >= COUNTDOWN_MS) beginPlay(now);
  if (phase == PLAY && (int32_t)(now - gameEndAt) >= 0) endGame(now);

  updateLights(now);

  if ((dirty && now - lastStateAt >= STATE_MIN_GAP_MS) || now - lastStateAt >= STATE_EVERY_MS) {
    sendState(now);
  }

  usbTx.pump(Serial);
  espTx.pump(ESP_SERIAL);
}
