/*
  Reflex Arcade - Wi-Fi bridge for the ESP8266 on the Mega + ESP8266 board

  Networking (works anywhere):
  - Joins the saved Wi-Fi network. The first saved network comes from secrets.h;
    after that you change it from the page ("Wi-Fi" tab) and it is kept in flash.
  - If that network can't be reached within 15 s, it starts its own hotspot
    "ArcadeGame" (password in secrets.h). While nobody is on the hotspot it
    retries the saved network every 30 s.
  - The hotspot can also be switched on by hand: from the page (kept on), or by
    holding player 1 button #1 for 5 s while no game runs (until next restart).
  - Friendly address: http://arcadegame.local on your Wi-Fi, and on the hotspot
    any address works (http://arcadegame, http://192.168.4.1); phones usually
    open the page by themselves when they join the hotspot.

  Game link:
  - Serves the page on port 80 and a WebSocket on the same port at /ws.
  - Relays lines both ways: browser -> Mega (commands), Mega -> every browser (JSON).
  - "NET..." commands are handled here, whether they come from a browser or
    from the Mega's USB port (the Mega forwards them).

  Requires the "WebSockets" library by Markus Sattler (2.4 or newer).
*/

#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>
#include <DNSServer.h>
#include <EEPROM.h>
#include <WebSockets4WebServer.h>

#include "secrets.h"   // copy secrets.example.h to secrets.h and fill it in
#include "web_page.h"

#ifndef AP_PASSWORD
#define AP_PASSWORD "arcadegame"
#endif

const uint32_t BAUD = 115200;                 // must match the Mega
const char *HOSTNAME = "arcadegame";
const char *AP_SSID = "ArcadeGame";
const IPAddress AP_IP(192, 168, 4, 1);
const uint32_t STA_TIMEOUT_MS = 15000;
const uint32_t STA_RETRY_MS = 30000;
const uint32_t NET_REPORT_MS = 10000;
const uint32_t CONFIG_MAGIC = 0xA4C0DE01;

struct NetConfig {
  uint32_t magic;
  char ssid[33];
  char pass[65];
  uint8_t apAlways;
};

enum StaState : uint8_t { STA_OFF, STA_CONNECTING, STA_UP, STA_FAILED };
const char *const STA_NAMES[] = {"off", "connecting", "up", "failed"};

ESP8266WebServer http(80);
WebSockets4WebServer ws;
DNSServer dns;
NetConfig cfg;

StaState sta = STA_OFF;
bool apOn = false;
bool apForced = false;      // switched on by hand until the next restart
bool mdnsUp = false;
uint32_t staStartedAt = 0;
uint32_t lastRetry = 0;
uint32_t lastNetReport = 0;
uint32_t lastMegaReport = 0;
uint32_t reconnectAt = 0;   // deferred reconnect after a settings change
bool netDirty = true;

String rxLine;
String lastState;
String lastPins;

// ======================================================================
// Helpers
// ======================================================================

void toMega(const String &line) {
  Serial.print(line);
  Serial.print('\n');
}

String jsonEscape(const char *s) {
  String out;
  for (; *s; s++) {
    char c = *s;
    if (c == '"' || c == '\\') { out += '\\'; out += c; }
    else if ((uint8_t)c < 0x20) out += ' ';
    else out += c;
  }
  return out;
}

void saveConfig() {
  EEPROM.put(0, cfg);
  EEPROM.commit();
}

void loadConfig() {
  EEPROM.begin(sizeof(NetConfig));
  EEPROM.get(0, cfg);
  if (cfg.magic != CONFIG_MAGIC) {
    memset(&cfg, 0, sizeof(cfg));
    cfg.magic = CONFIG_MAGIC;
    strncpy(cfg.ssid, WIFI_SSID, sizeof(cfg.ssid) - 1);
    strncpy(cfg.pass, WIFI_PASSWORD, sizeof(cfg.pass) - 1);
    cfg.apAlways = 0;
    saveConfig();
  }
  cfg.ssid[sizeof(cfg.ssid) - 1] = 0;
  cfg.pass[sizeof(cfg.pass) - 1] = 0;
}

// ======================================================================
// Wi-Fi: station (your network) and hotspot (ArcadeGame)
// ======================================================================

void startAp() {
  if (apOn) return;
  WiFi.mode(WIFI_AP_STA);
  WiFi.softAPConfig(AP_IP, AP_IP, IPAddress(255, 255, 255, 0));
  WiFi.softAP(AP_SSID, AP_PASSWORD);
  dns.setErrorReplyCode(DNSReplyCode::NoError);
  dns.start(53, "*", AP_IP);     // every name points to the board: captive portal
  apOn = true;
  if (mdnsUp) MDNS.notifyAPChange();
  netDirty = true;
}

void stopAp() {
  if (!apOn) return;
  dns.stop();
  WiFi.softAPdisconnect(true);
  WiFi.mode(WIFI_STA);
  apOn = false;
  if (mdnsUp) MDNS.notifyAPChange();
  netDirty = true;
}

bool wantAp() { return cfg.apAlways || apForced; }

void startSta() {
  if (!cfg.ssid[0]) {
    sta = STA_OFF;
    startAp();
    return;
  }
  WiFi.setAutoReconnect(true);
  WiFi.begin(cfg.ssid, cfg.pass);
  sta = STA_CONNECTING;
  staStartedAt = millis();
  netDirty = true;
}

void netTick() {
  uint32_t now = millis();
  bool connected = WiFi.status() == WL_CONNECTED;

  if (reconnectAt && (int32_t)(now - reconnectAt) >= 0) {
    reconnectAt = 0;
    WiFi.disconnect(false);
    startSta();
  }

  if (connected && sta != STA_UP) {
    sta = STA_UP;
    netDirty = true;
  } else if (!connected && sta == STA_UP) {
    sta = STA_CONNECTING;          // lost it, auto-reconnect is trying
    staStartedAt = now;
    netDirty = true;
  }

  if (sta == STA_CONNECTING && now - staStartedAt > STA_TIMEOUT_MS) {
    // Known network not reachable: fall back to the hotspot and stop scanning,
    // so the hotspot stays stable for the players.
    sta = STA_FAILED;
    WiFi.setAutoReconnect(false);
    WiFi.disconnect(false);
    startAp();
    lastRetry = now;
    netDirty = true;
  }

  // Retry the saved network now and then, but only while nobody uses the hotspot.
  if (sta == STA_FAILED && cfg.ssid[0] && now - lastRetry > STA_RETRY_MS &&
      WiFi.softAPgetStationNum() == 0) {
    lastRetry = now;
    startSta();
  }

  // Hotspot no longer needed once we are back on the network and it is empty.
  if (apOn && sta == STA_UP && !wantAp() && WiFi.softAPgetStationNum() == 0) stopAp();
  // Start a requested hotspot only once the network attempt is settled, so the
  // station connects on a quiet radio and the hotspot then shares its channel.
  if (!apOn && wantAp() && sta != STA_CONNECTING) startAp();

  if (apOn) dns.processNextRequest();
}

// ======================================================================
// Reporting
// ======================================================================

String netJson() {
  String j;
  j.reserve(320);
  j = F("{\"t\":\"net\",\"host\":\"");
  j += HOSTNAME;
  j += F("\",\"sta\":{\"ssid\":\"");
  j += jsonEscape(cfg.ssid);
  j += F("\",\"state\":\"");
  j += STA_NAMES[sta];
  j += F("\",\"ip\":\"");
  j += sta == STA_UP ? WiFi.localIP().toString() : String();
  j += F("\",\"rssi\":");
  j += sta == STA_UP ? WiFi.RSSI() : 0;
  j += F("},\"ap\":{\"on\":");
  j += apOn ? 1 : 0;
  j += F(",\"ssid\":\"");
  j += AP_SSID;
  j += F("\",\"ip\":\"");
  j += AP_IP.toString();
  j += F("\",\"clients\":");
  j += apOn ? WiFi.softAPgetStationNum() : 0;
  j += F(",\"always\":");
  j += cfg.apAlways ? 1 : 0;
  j += F(",\"forced\":");
  j += apForced ? 1 : 0;
  j += F("}}");
  return j;
}

void reportNet() {
  String j = netJson();
  ws.broadcastTXT(j);
  toMega(j);                         // the Mega shows it on its USB port
  // Short form for the Mega's own state message.
  toMega(String(F("WIFI ")) + (sta == STA_UP ? 3 : (int)WiFi.status()));
  String ip = sta == STA_UP ? WiFi.localIP().toString() : (apOn ? AP_IP.toString() : String("-"));
  toMega("IP " + ip);
  lastNetReport = millis();
  netDirty = false;
}

void scanDone(int found) {
  String j;
  j.reserve(600);
  j = F("{\"t\":\"scan\",\"nets\":[");
  int added = 0;
  for (int i = 0; i < found && added < 15; i++) {
    String ssid = WiFi.SSID(i);
    if (!ssid.length()) continue;
    bool dup = false;
    for (int k = 0; k < i; k++) if (WiFi.SSID(k) == ssid) { dup = true; break; }
    if (dup) continue;
    if (added++) j += ',';
    j += F("{\"s\":\"");
    j += jsonEscape(ssid.c_str());
    j += F("\",\"r\":");
    j += WiFi.RSSI(i);
    j += F(",\"e\":");
    j += WiFi.encryptionType(i) == ENC_TYPE_NONE ? 0 : 1;
    j += '}';
  }
  j += F("]}");
  WiFi.scanDelete();
  ws.broadcastTXT(j);
}

// ======================================================================
// NET commands (from a browser or forwarded by the Mega)
//   NET?                        report network status
//   NETSET<TAB>ssid<TAB>pass    save a new network and connect to it
//   NETAP<TAB>1|0               keep the hotspot always on / automatic
//   NETSCAN                     list nearby networks
//   NETRETRY                    try the saved network again now
// ======================================================================

void handleNet(const String &line) {
  if (line.startsWith("NETSET\t")) {
    int a = line.indexOf('\t');
    int b = line.indexOf('\t', a + 1);
    String ssid = b > 0 ? line.substring(a + 1, b) : line.substring(a + 1);
    String pass = b > 0 ? line.substring(b + 1) : String();
    if (!ssid.length() || ssid.length() > 32 || pass.length() > 64) return;
    strncpy(cfg.ssid, ssid.c_str(), sizeof(cfg.ssid) - 1);
    cfg.ssid[sizeof(cfg.ssid) - 1] = 0;
    strncpy(cfg.pass, pass.c_str(), sizeof(cfg.pass) - 1);
    cfg.pass[sizeof(cfg.pass) - 1] = 0;
    saveConfig();
    sta = STA_CONNECTING;
    staStartedAt = millis();
    reconnectAt = millis() + 600;    // let the answer reach the page first
    netDirty = true;
  } else if (line.startsWith("NETAP\t")) {
    cfg.apAlways = line.endsWith("1") ? 1 : 0;
    if (!cfg.apAlways) apForced = false;
    saveConfig();
    netDirty = true;
  } else if (line == "NETSCAN") {
    WiFi.scanNetworksAsync(scanDone);
  } else if (line == "NETRETRY") {
    reconnectAt = millis() + 300;
    netDirty = true;
  } else if (line == "NET?") {
    netDirty = true;
  }
}

// ======================================================================
// WebSocket and serial
// ======================================================================

void onWsEvent(uint8_t num, WStype_t type, uint8_t *payload, size_t length) {
  switch (type) {
    case WStype_CONNECTED: {
      if (lastPins.length()) ws.sendTXT(num, lastPins);
      if (lastState.length()) ws.sendTXT(num, lastState);
      String j = netJson();
      ws.sendTXT(num, j);
      toMega("STATE?");
      break;
    }

    case WStype_TEXT: {
      if (length == 4 && memcmp(payload, "ping", 4) == 0) {
        ws.sendTXT(num, "pong");
        break;
      }
      String msg;
      msg.reserve(length);
      for (size_t i = 0; i < length && i < 200; i++) msg += (char)payload[i];
      if (msg.startsWith("NET")) {
        handleNet(msg);
        break;
      }
      // Forward one printable command line to the Mega.
      String cmd;
      for (size_t i = 0; i < msg.length() && i < 80; i++) {
        char c = msg[i];
        if (c >= 32 && c < 127) cmd += c;
      }
      if (cmd.length()) toMega(cmd);
      break;
    }

    default:
      break;
  }
}

// A JSON line from the Mega is only passed on if its braces balance and close
// exactly at the end. This drops lines that lost bytes (e.g. while the flash
// was busy saving settings) instead of sending broken data to the page.
bool looksComplete(const String &s) {
  int depth = 0;
  bool inStr = false;
  for (size_t i = 0; i < s.length(); i++) {
    char c = s[i];
    if (inStr) {
      if (c == '\\') i++;
      else if (c == '"') inStr = false;
      continue;
    }
    if (c == '"') inStr = true;
    else if (c == '{' || c == '[') depth++;
    else if (c == '}' || c == ']') {
      if (--depth < 0) return false;
      if (depth == 0 && i != s.length() - 1) return false;
    }
  }
  return depth == 0 && !inStr;
}

void pollMega() {
  while (Serial.available()) {
    char c = (char)Serial.read();
    if (c == '\n' || c == '\r') {
      if (rxLine.length()) {
        if (rxLine[0] == '{' && !looksComplete(rxLine)) {
          // damaged line: drop it
        } else if (rxLine[0] == '{') {
          if (rxLine.startsWith("{\"t\":\"state\"")) lastState = rxLine;
          else if (rxLine.startsWith("{\"t\":\"pins\"")) lastPins = rxLine;
          else if (rxLine.startsWith("{\"t\":\"apreq\"")) {
            apForced = true;           // player held button #1: hotspot on
            netDirty = true;
          }
          ws.broadcastTXT(rxLine);
        } else if (rxLine.startsWith("NET")) {
          handleNet(rxLine);           // forwarded from the Mega's USB port
        }
      }
      rxLine = "";
    } else if (rxLine.length() < 700) {
      rxLine += c;
    } else {
      rxLine = "";  // runaway line, drop it
    }
  }
}

// ======================================================================
// Web server
// ======================================================================

void sendPage() {
  http.sendHeader("Cache-Control", "no-cache");
  http.send_P(200, "text/html", INDEX_HTML);
}

// Anything unknown goes to the page. On the hotspot this is what makes
// phones show the "sign in to network" page with the game.
void redirectToPage() {
  bool viaHotspot = http.client().localIP() == AP_IP;
  http.sendHeader("Location", viaHotspot ? String("http://192.168.4.1/") : String("/"));
  http.sendHeader("Cache-Control", "no-cache");
  http.send(302, "text/plain", "");
}

// ======================================================================
// Arduino entry points
// ======================================================================

void setup() {
  Serial.setRxBufferSize(2048);
  Serial.begin(BAUD);

  loadConfig();

  WiFi.persistent(false);
  WiFi.setSleepMode(WIFI_NONE_SLEEP);
  WiFi.hostname(HOSTNAME);
  WiFi.mode(WIFI_STA);
  startSta();          // the hotspot, if wanted, follows once this settles

  mdnsUp = MDNS.begin(HOSTNAME);
  if (mdnsUp) MDNS.addService("http", "tcp", 80);

  http.on("/", sendPage);
  http.onNotFound(redirectToPage);
  http.addHook(ws.hookForWebserver("/ws", onWsEvent));
  http.begin();

  ws.enableHeartbeat(10000, 4000, 2);  // ping every 10 s, drop after 2 missed pongs
}

void loop() {
  pollMega();
  netTick();
  http.handleClient();
  ws.loop();
  if (mdnsUp) MDNS.update();

  uint32_t now = millis();
  if (netDirty || now - lastNetReport > NET_REPORT_MS) reportNet();
}
