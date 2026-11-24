/*
  Reflex Arcade - Wi-Fi bridge for the ESP8266 on the Mega + ESP8266 board

  - Joins your Wi-Fi (credentials in secrets.h).
  - Serves the game page on http://<ip>/ and http://reflex.local/
  - Runs a WebSocket server on port 81 and relays lines both ways:
      browser -> ESP8266 -> Mega   (commands such as "START multi 2 same 60")
      Mega -> ESP8266 -> every browser   (JSON state and events)
  - Remembers the last state, so a reconnecting page is up to date at once.

  Robustness:
  - Wi-Fi power saving is off and auto-reconnect is on.
  - WebSocket heartbeat drops dead clients; the page reconnects by itself.
  - The page sends "ping" every few seconds and gets "pong" back.

  Requires the "WebSockets" library by Markus Sattler.
*/

#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>
#include <WebSocketsServer.h>

#include "secrets.h"   // copy secrets.example.h to secrets.h and fill it in
#include "web_page.h"

const uint32_t BAUD = 115200;   // must match the Mega
const char *HOSTNAME = "reflex";

ESP8266WebServer http(80);
WebSocketsServer ws(81);

String rxLine;
String lastState;
String lastPins;
bool wifiUp = false;
bool mdnsUp = false;
uint32_t lastIpReport = 0;

void toMega(const String &line) {
  Serial.print(line);
  Serial.print('\n');
}

void reportIp() {
  toMega("WIFI 3");
  toMega("IP " + WiFi.localIP().toString());
  lastIpReport = millis();
}

void onWsEvent(uint8_t num, WStype_t type, uint8_t *payload, size_t length) {
  switch (type) {
    case WStype_CONNECTED:
      if (lastPins.length()) ws.sendTXT(num, lastPins);
      if (lastState.length()) ws.sendTXT(num, lastState);
      toMega("STATE?");
      break;

    case WStype_TEXT: {
      if (length == 4 && memcmp(payload, "ping", 4) == 0) {
        ws.sendTXT(num, "pong");
        break;
      }
      // Forward one printable command line to the Mega.
      String cmd;
      cmd.reserve(length);
      for (size_t i = 0; i < length && i < 80; i++) {
        char c = (char)payload[i];
        if (c >= 32 && c < 127) cmd += c;
      }
      if (cmd.length()) toMega(cmd);
      break;
    }

    default:
      break;
  }
}

void pollMega() {
  while (Serial.available()) {
    char c = (char)Serial.read();
    if (c == '\n' || c == '\r') {
      if (rxLine.length() && rxLine[0] == '{') {
        if (rxLine.startsWith("{\"t\":\"state\"")) lastState = rxLine;
        else if (rxLine.startsWith("{\"t\":\"pins\"")) lastPins = rxLine;
        ws.broadcastTXT(rxLine);
      }
      rxLine = "";
    } else if (rxLine.length() < 700) {
      rxLine += c;
    } else {
      rxLine = "";  // runaway line, drop it
    }
  }
}

void checkWifi() {
  bool up = WiFi.status() == WL_CONNECTED;
  if (up && !wifiUp) {
    reportIp();
    if (!mdnsUp) {
      mdnsUp = MDNS.begin(HOSTNAME);
      if (mdnsUp) MDNS.addService("http", "tcp", 80);
    }
  }
  wifiUp = up;
  // Re-send the IP now and then in case the Mega was reset.
  if (wifiUp && millis() - lastIpReport > 10000) reportIp();
  // While not connected, tell the Mega why (shown on its USB port and the page):
  // 1 = network not found, 4 = wrong password, 6/7 = still connecting.
  if (!wifiUp && millis() - lastIpReport > 3000) {
    toMega("WIFI " + String((int)WiFi.status()));
    lastIpReport = millis();
  }
}

void setup() {
  Serial.setRxBufferSize(2048);
  Serial.begin(BAUD);

  WiFi.persistent(false);
  WiFi.mode(WIFI_STA);
  WiFi.hostname(HOSTNAME);
  WiFi.setSleepMode(WIFI_NONE_SLEEP);
  WiFi.setAutoReconnect(true);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  http.on("/", []() {
    http.sendHeader("Cache-Control", "no-cache");
    http.send_P(200, "text/html", INDEX_HTML);
  });
  http.onNotFound([]() {
    http.sendHeader("Location", "/");
    http.send(302, "text/plain", "");
  });
  http.begin();

  ws.begin();
  ws.onEvent(onWsEvent);
  ws.enableHeartbeat(10000, 4000, 2);  // ping every 10 s, drop after 2 missed pongs
}

void loop() {
  pollMega();
  ws.loop();
  http.handleClient();
  checkWifi();
  if (mdnsUp) MDNS.update();
}
