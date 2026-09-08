#include <WiFi.h>
#include <WebServer.h>
#include <WebSocketsServer.h>
#include <BleKeyboard.h>

const char* ssid = "GlobeAtHome_CC5E7_2.4";
const char* password = "2B922CE7";

WebServer server(80);
WebSocketsServer webSocket = WebSocketsServer(81);
BleKeyboard bleKeyboard("Keyboard", "ESP32", 100);

bool keepAliveEnabled = false;
unsigned long lastKeepAlive = 0;

unsigned long keepAliveInterval = 30000;

void keepAwake() {
  if (!bleKeyboard.isConnected()) return;

  bleKeyboard.press(KEY_LEFT_ARROW);
  delay(20);
  bleKeyboard.release(KEY_LEFT_ARROW);

  Serial.print("KEEP ALIVE SENT: ");
  Serial.println(keepAliveInterval);
}

// ---------- KEY MAP ----------
uint8_t getKey(String key) {
  key.toUpperCase();

  // Modifiers
  if (key == "CTRL" || key == "LCTRL") return KEY_LEFT_CTRL;
  if (key == "RCTRL") return KEY_RIGHT_CTRL;

  if (key == "SHIFT" || key == "LSHIFT") return KEY_LEFT_SHIFT;
  if (key == "RSHIFT") return KEY_RIGHT_SHIFT;

  if (key == "ALT" || key == "LALT") return KEY_LEFT_ALT;
  if (key == "RALT") return KEY_RIGHT_ALT;

  if (key == "WIN" || key == "LWIN") return KEY_LEFT_GUI;
  if (key == "RWIN") return KEY_RIGHT_GUI;

  // Common keys
  if (key == "ENTER") return KEY_RETURN;
  if (key == "ESC") return KEY_ESC;
  if (key == "TAB") return KEY_TAB;
  if (key == "BACKSPACE") return KEY_BACKSPACE;
  if (key == "SPACE") return ' ';

  // Navigation
  if (key == "UP") return KEY_UP_ARROW;
  if (key == "DOWN") return KEY_DOWN_ARROW;
  if (key == "LEFT") return KEY_LEFT_ARROW;
  if (key == "RIGHT") return KEY_RIGHT_ARROW;

  if (key == "DELETE") return KEY_DELETE;
  if (key == "INSERT") return KEY_INSERT;
  if (key == "HOME") return KEY_HOME;
  if (key == "END") return KEY_END;
  if (key == "PAGEUP") return KEY_PAGE_UP;
  if (key == "PAGEDOWN") return KEY_PAGE_DOWN;

  // Function keys
  if (key == "F1") return KEY_F1;
  if (key == "F2") return KEY_F2;
  if (key == "F3") return KEY_F3;
  if (key == "F4") return KEY_F4;
  if (key == "F5") return KEY_F5;
  if (key == "F6") return KEY_F6;
  if (key == "F7") return KEY_F7;
  if (key == "F8") return KEY_F8;
  if (key == "F9") return KEY_F9;
  if (key == "F10") return KEY_F10;
  if (key == "F11") return KEY_F11;
  if (key == "F12") return KEY_F12;

  // Caps Lock
  if (key == "CAPSLOCK") return KEY_CAPS_LOCK;

  // Single character
  if (key.length() == 1) return key[0];

  return 0;
}

// Toggle ON/OFF
void handleKeepAliveToggle() {
  keepAliveEnabled = !keepAliveEnabled;

  Serial.print("KeepAlive: ");
  Serial.println(keepAliveEnabled ? "ON" : "OFF");

  server.sendHeader("Access-Control-Allow-Origin", "*");
  server.send(200, "application/json",
              String("{\"enabled\":") + (keepAliveEnabled ? "true" : "false") + "}");
}

// Get status
void handleKeepAliveStatus() {
  server.sendHeader("Access-Control-Allow-Origin", "*");
  server.send(200, "application/json",
              String("{\"enabled\":") + (keepAliveEnabled ? "true" : "false") + "}");
}

// ---------- COMBO ----------
void sendCombo(String combo) {
  if (!bleKeyboard.isConnected()) return;

  combo.toUpperCase();

  // If it's just a single key (no +), use write()
  if (combo.indexOf('+') == -1) {
    bleKeyboard.write(combo[0]);
    return;
  }

  // Otherwise handle combo
  bleKeyboard.releaseAll();
  delay(5);

  int start = 0;
  int idx;

  while ((idx = combo.indexOf('+', start)) != -1) {
    String part = combo.substring(start, idx);
    uint8_t k = getKey(part);
    if (k) bleKeyboard.press(k);
    start = idx + 1;
  }

  String last = combo.substring(start);
  uint8_t k = getKey(last);
  if (k) bleKeyboard.press(k);

  delay(50);
  bleKeyboard.releaseAll();
}

// ---------- HTTP HANDLER ----------
void handleMacro() {
  Serial.println("HTTP HIT");

  if (!bleKeyboard.isConnected()) {
    Serial.println("BLE NOT CONNECTED");
  } else {
    Serial.println("BLE CONNECTED");
  }

  if (server.hasArg("keys")) {
    Serial.println(server.arg("keys"));
    sendCombo(server.arg("keys"));
  }

  server.sendHeader("Access-Control-Allow-Origin", "*");
  server.send(200, "text/plain", "OK");
}

// ---------- WS MESSAGE ----------
void handleWS(String msg) {
  if (!bleKeyboard.isConnected()) return;

  // 🔥 RELEASE ALL
  if (msg == "RA") {
    bleKeyboard.releaseAll();
    Serial.println("RELEASE ALL");
    return;
  }

  if (msg.startsWith("KD:")) {
    String key = msg.substring(3);
    uint8_t k = getKey(key);
    if (k) bleKeyboard.press(k);
  }

  else if (msg.startsWith("KU:")) {
    String key = msg.substring(3);
    uint8_t k = getKey(key);
    if (k) bleKeyboard.release(k);
  }
}

// ---------- WS EVENT ----------
void onWS(uint8_t num, WStype_t type, uint8_t * payload, size_t length) {

  if (type == WStype_CONNECTED) {
    Serial.println("WS CONNECTED");
  }

  else if (type == WStype_DISCONNECTED) {
    Serial.println("WS DISCONNECTED");
    // 1. Release all BLE keys to prevent stuck keys when the connection drops
            if (bleKeyboard.isConnected()) {
                bleKeyboard.releaseAll();
            }
            
  }

  else if (type == WStype_TEXT) {
    String msg = String((char*)payload, length);  // ✅ FIXED

    Serial.println("WS MSG: " + msg);

    handleWS(msg);
  }
}

// ---------- SETUP ----------
void setup() {
  Serial.begin(115200);

  WiFi.begin(ssid, password);
  Serial.print("Connecting");

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("\nIP:");
  Serial.println(WiFi.localIP());

  bleKeyboard.begin();

  // HTTP
  server.on("/macro", handleMacro);
  server.on("/keepalive/toggle", handleKeepAliveToggle);
  server.on("/keepalive/status", handleKeepAliveStatus);
  server.on("/releaseAll", handleKeepAliveStatus);
  server.on("/ping", HTTP_GET, []() {
    Serial.println("ESP32 Received a Ping request");
    server.sendHeader("Access-Control-Allow-Origin", "*");
    server.send(200, "text/plain", "MACROPAD_OK");
  });
  server.begin();

  // WS
  Serial.println("Starting WebSocket...");
  webSocket.begin();
  webSocket.onEvent(onWS);
  Serial.println("WebSocket started on port 81");
}

// ---------- LOOP ----------
void loop() {
  server.handleClient();
  webSocket.loop();

  if (keepAliveEnabled && millis() - lastKeepAlive > keepAliveInterval) {
    keepAwake();
    lastKeepAlive = millis();
    keepAliveInterval = random(100000, 120000);
  }
}