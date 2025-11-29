/*
  ESP32 Health Monitor — Component Board (7 Sensors)
  Updated Version — Includes CLOUD UPLOAD SUPPORT
  Sends sensor readings to:
  https://YOUR-RENDER-APP.onrender.com/sensor-data
*/

#include <Wire.h>
#include "MAX30105.h"
#include "spo2_algorithm.h"
#include <OneWire.h>
#include <DallasTemperature.h>
#include <WiFi.h>
#include <HTTPClient.h>

// =====================================================
// WIFI + CLOUD CONFIG
// =====================================================
const char* WIFI_SSID = "YOUR_WIFI_NAME";
const char* WIFI_PASS = "YOUR_WIFI_PASSWORD";

const char* SERVER_URL = "https://YOUR-RENDER-APP.onrender.com/sensor-data";
const char* DEVICE_ID = "esp32-001";
const char* API_KEY = "my-secret-key";

// =====================================================
// WiFi Connect Function
// =====================================================
void connectWiFi() {
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  Serial.print("Connecting to WiFi");
  int tries = 0;

  while (WiFi.status() != WL_CONNECTED && tries < 20) {
    delay(500);
    Serial.print(".");
    tries++;
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\n✅ WiFi Connected");
    Serial.print("IP: ");
    Serial.println(WiFi.localIP());
  } else {
    Serial.println("\n❌ WiFi FAILED");
  }
}

// =====================================================
// CLOUD UPLOAD FUNCTION
// =====================================================
void sendToCloud(String key, String value) {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("❌ No WiFi, cannot send to cloud");
    return;
  }

  HTTPClient http;
  http.begin(SERVER_URL);
  http.addHeader("Content-Type", "application/json");
  http.addHeader("x-api-key", API_KEY);

  String json = "{";
  json += "\"device_id\":\"" + String(DEVICE_ID) + "\",";
  json += "\"timestamp\":\"" + String(millis()) + "\",";
  json += "\"seq\":1,";
  json += "\"" + key + "\":\"" + value + "\"";
  json += "}";

  int code = http.POST(json);
  Serial.print("🌐 Cloud POST Status: ");
  Serial.println(code);

  if (code > 0) {
    Serial.println(http.getString());
  }

  http.end();
}

// =====================================================
// ORIGINAL SENSOR CODE BELOW (UNTOUCHED)
// =====================================================

// ============ I2C & Serial setup ============
TwoWire I2C1 = Wire;
#define I2C_SDA 21
#define I2C_SCL 22
#define DISP_TX 19
#define DISP_RX 18

// ============ Objects ============
MAX30105 maxHR;
#define ONE_WIRE_BUS 4
OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature ds18(&oneWire);

// ============ Touch Buttons ============
#define BTN_HR     32
#define BTN_ECG    14
#define BTN_TEMP   12
#define BTN_GSR    13
#define BTN_SPIRO  5
#define BTN_GLU    15

const int btnPins[6] = {BTN_HR, BTN_ECG, BTN_TEMP, BTN_GSR, BTN_SPIRO, BTN_GLU};
const char* btnKeys[6] = {"HR", "ECG", "TEMP", "GSR", "SPIRO", "GLU"};
bool btnPrev[6] = {LOW, LOW, LOW, LOW, LOW, LOW};
bool sensorOn[6] = {false, false, false, false, false, false};

// ============ ECG ============
#define ECG_PIN 35
#define LO_PLUS 27
#define LO_MINUS 26

// ============ Analog sensors ============
#define MPX_PIN 34
#define GSR_PIN 33

// ============ BP serial ============
#define BP_RX_PIN 16
#define BP_TX_PIN 17
String bpLine = "";

// ============ Parameters ============
const int SAMPLE_FREQ = 100;
const int N_SAMPLES = 100;
uint32_t hr_ir[N_SAMPLES], hr_red[N_SAMPLES];
const uint32_t HR_FINGER_THRESHOLD = 40000UL;

// Send message to display board
void sendDisplay(const String &msg) {
  Serial1.println(msg);
  Serial.print("→ Display: ");
  Serial.println(msg);
}

// =====================================================
// SETUP
// =====================================================
void setup() {
  Serial.begin(115200);
  connectWiFi();   // 🔥 CLOUD SUPPORT ADDED HERE

  Serial1.begin(115200, SERIAL_8N1, DISP_RX, DISP_TX);

  I2C1.begin(I2C_SDA, I2C_SCL, 400000);
  ds18.begin();

  for (int i = 0; i < 6; i++) pinMode(btnPins[i], INPUT);

  if (maxHR.begin(I2C1)) {
    maxHR.setup();
    maxHR.setPulseAmplitudeIR(0x1F);
    maxHR.setPulseAmplitudeRed(0x1F);
    Serial.println("✅ MAX30102 ready");
  } else Serial.println("⚠ MAX30102 not found");

  pinMode(LO_PLUS, INPUT);
  pinMode(LO_MINUS, INPUT);

  Serial2.begin(9600, SERIAL_8N1, BP_RX_PIN, BP_TX_PIN);

  Serial.println("🏥 Component ESP32 Ready");
}

// =====================================================
// MAIN LOOP
// =====================================================
void loop() {
  for (int i = 0; i < 6; i++) {
    bool state = digitalRead(btnPins[i]);
    if (state && !btnPrev[i]) {
      sensorOn[i] = !sensorOn[i];
      String key = btnKeys[i];
      sendDisplay(key + (sensorOn[i] ? "_ON" : "_OFF"));

      if (sensorOn[i]) {
        if (key == "HR") measureHR();
        else if (key == "TEMP") measureTemp();
        else if (key == "ECG") streamECG();
        else if (key == "GSR") measureGSR();
        else if (key == "SPIRO") measureSpiro();
        else if (key == "GLU") sendDisplay("GLU_START");
      }
    }
    btnPrev[i] = state;
  }

  // Listen for BP serial data
  while (Serial2.available()) {
    char c = (char)Serial2.read();
    if (c == '\n') {
      if (bpLine.length()) { parseBP(bpLine); bpLine = ""; }
    } else if (c != '\r') bpLine += c;
  }

  delay(30);
}

// =====================================================
// SENSOR FUNCTIONS (WITH CLOUD UPLOAD ADDED)
// =====================================================

void measureHR() {
  Serial.println("🩸 Measuring HR/SpO2...");
  if (!waitFinger()) { sendDisplay("HR:---"); return; }

  for (int i = 0; i < N_SAMPLES; i++) {
    while (!maxHR.check()) delay(1);
    hr_ir[i] = maxHR.getIR();
    hr_red[i] = maxHR.getRed();
  }

  int32_t spo2, hr; int8_t vspo2, vhr;
  maxim_heart_rate_and_oxygen_saturation(hr_ir, N_SAMPLES, hr_red, &spo2, &vspo2, &hr, &vhr);

  if (vhr && vspo2) {
    Serial.printf("❤️ HR=%ld  SpO2=%ld\n", hr, spo2);
    sendDisplay("HR:" + String(hr));
    sendDisplay("SPO2:" + String(spo2));

    sendToCloud("heartRate", String(hr));   // 🔥 CLOUD
    sendToCloud("spo2", String(spo2));      // 🔥 CLOUD
  }
}

bool waitFinger() {
  unsigned long start = millis();
  while (millis() - start < 5000) {
    if (maxHR.check() && maxHR.getIR() > HR_FINGER_THRESHOLD) return true;
    delay(50);
  }
  return false;
}

void measureTemp() {
  ds18.requestTemperatures();
  float t = ds18.getTempCByIndex(0);

  Serial.printf("🌡 TEMP=%.2f°C\n", t);
  sendDisplay("TEMP:" + String(t, 1));

  sendToCloud("temperature", String(t));   // 🔥 CLOUD
}

void streamECG() {
  Serial.println("📈 ECG Live...");
  unsigned long t0 = millis();

  while (sensorOn[1]) {
    if (digitalRead(LO_PLUS) || digitalRead(LO_MINUS)) continue;

    int val = analogRead(ECG_PIN);
    sendDisplay("ECG:" + String(val));

    // 🔥 CLOUD (200ms rate)
    static unsigned long lastECG = 0;
    if (millis() - lastECG > 200) {
      sendToCloud("ecg", String(val));
      lastECG = millis();
    }

    if (millis() - t0 > 20000) break;
    if (digitalRead(BTN_ECG)) { sensorOn[1] = false; break; }

    delay(10);
  }
}

void measureGSR() {
  int r = analogRead(GSR_PIN);
  sendDisplay("GSR:" + String(r));
  Serial.printf("🤚 GSR=%d\n", r);

  sendToCloud("gsr", String(r));    // 🔥 CLOUD
}

void measureSpiro() {
  int base = analogRead(MPX_PIN);
  unsigned long start = millis();
  int peak = base;

  while (millis() - start < 4000) {
    int v = analogRead(MPX_PIN);
    if (v > peak) peak = v;
    delay(20);
  }

  sendDisplay("SPIRO:" + String(peak));
  Serial.printf("🌬 SPIRO=%d\n", peak);

  sendToCloud("spiro", String(peak));   // 🔥 CLOUD
}

void parseBP(const String &line) {
  if (line.startsWith("success")) {
    int a = line.indexOf(','), b = line.indexOf(',', a + 1), c = line.indexOf(',', b + 1);
    if (a > 0 && b > a && c > b) {
      int sys = line.substring(a + 1, b).toInt();
      int dia = line.substring(b + 1, c).toInt();
      int pul = line.substring(c + 1).toInt();

      Serial.printf("💢 BP=%d/%d Pulse=%d\n", sys, dia, pul);

      sendDisplay("BP_SYS:" + String(sys));
      sendDisplay("BP_DIA:" + String(dia));
      sendDisplay("BP_PUL:" + String(pul));

      // 🔥 CLOUD
      sendToCloud("bp_sys", String(sys));
      sendToCloud("bp_dia", String(dia));
      sendToCloud("bp_pulse", String(pul));
    }
  }
}
