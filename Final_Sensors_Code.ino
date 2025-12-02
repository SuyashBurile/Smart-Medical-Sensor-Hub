#include <WiFi.h>
#include <HTTPClient.h>
#include <Wire.h>
#include "MAX30105.h"
#include "spo2_algorithm.h"
#include <OneWire.h>
#include <DallasTemperature.h>

// ===== CONFIGURATION =====
#define SDA_PIN 21
#define SCL_PIN 22
#define DISP_RX 18
#define DISP_TX 19
#define BP_RX   16
#define BP_TX   17

// ===== OBJECTS =====
TwoWire I2C1 = Wire; 
MAX30105 maxHR;
OneWire oneWire(4);
DallasTemperature ds18(&oneWire);

// ===== BUTTONS =====
const int btnPins[6] = {32, 14, 12, 13, 5, 15};
const char* modeTags[6] = {"MODE:HR", "MODE:ECG", "MODE:TEMP", "MODE:GSR", "MODE:SPIRO", "MODE:GLU"};
bool prevBtn[6] = {0};

// ===== STATE MACHINE =====
int activeMode = -1; 
unsigned long modeStartTime = 0; // For ECG Timeout

// --- SENSOR VARIABLES ---
// ECG
#define ECG_PIN 35
#define LO_PLUS 27
#define LO_MINUS 26
hw_timer_t *timer = NULL;
volatile bool ecgTrigger = false;

// HR / SpO2
const int N_SAMPLES = 100;
uint32_t irArr[N_SAMPLES], redArr[N_SAMPLES];
int hrSampleCount = 0;

// Temperature
unsigned long tempTimer = 0;

// Spirometer (Lung) Variables
float simLungVal = 5500.0; // Start mid-range

// BP Buffer
String bpLine = "";

// ===== CLOUD UPLOAD CONFIG =====
const char* ssid = "Aditya@08";
const char* password = "Adity@08";

const char* cloudURL = "https://smart-medical-sensor-hub.onrender.com/sensor-data";


// ===== INTERRUPT (ECG) =====
void IRAM_ATTR onTimer() {
  if(activeMode == 1) ecgTrigger = true; 
}

void connectWiFi() {
  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi");

  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    delay(400);
  }

  Serial.println("\nWiFi Connected!");
  Serial.println(WiFi.localIP());
}


// -------------------------------------------------------------------------
void setup() {
  Serial.begin(115200); 
  // Serial1: Connection to Display Board
  Serial1.begin(115200, SERIAL_8N1, DISP_RX, DISP_TX); 
  // Serial2: Connection to BP Machine
  Serial2.begin(9600, SERIAL_8N1, BP_RX, BP_TX);       

  I2C1.begin(SDA_PIN, SCL_PIN, 100000); 
  ds18.begin();
  
  if (maxHR.begin(I2C1)) {
    maxHR.setup(0x1F, 4, 2, 400, 411, 4096);
  }

  pinMode(LO_PLUS, INPUT);
  pinMode(LO_MINUS, INPUT);
  for(int i=0; i<6; i++) pinMode(btnPins[i], INPUT_PULLUP); 

  // Timer Setup
  timer = timerBegin(1000000); 
  timerAttachInterrupt(timer, &onTimer);
  timerAlarm(timer, 4000, true, 0); 
  
  delay(1000);
  Serial1.println("CMD:HOME");
  connectWiFi();   // <<< ADD THIS 
}

void sendToCloud(String jsonData) {
  if (WiFi.status() != WL_CONNECTED) {
    connectWiFi();
  }

  HTTPClient http;
  http.begin(cloudURL);
  http.addHeader("Content-Type", "application/json");

  int httpResponseCode = http.POST(jsonData);
  Serial.print("Cloud Response: ");
  Serial.println(httpResponseCode);

  http.end();
}


// -------------------------------------------------------------------------
void loop() {
  unsigned long now = millis();

  // 1. BUTTON LOGIC
  for(int i=0; i<6; i++) {
    bool state = !digitalRead(btnPins[i]); 
    if(state && !prevBtn[i]) { 
      if(activeMode == -1) {
        activeMode = i;
        modeStartTime = millis(); // Track when mode started
        Serial1.println("CMD:" + String(modeTags[i]));
        
        // --- Mode Initialization ---
        if(i == 0) hrSampleCount = 0; 
        if(i == 2) runTempCycle(); // Run 30s Temp Cycle
      }
      else if(activeMode == i) {
        activeMode = -1;
        Serial1.println("CMD:HOME");
      }
      delay(300); // Debounce
    }
    prevBtn[i] = state;
  }

  // 2. ACTIVE SENSOR EXECUTION

  // --- MODE 0: HEART RATE ---
  if(activeMode == 0) {
    maxHR.check();
    while(maxHR.available()) {
      redArr[hrSampleCount] = maxHR.getRed();
      irArr[hrSampleCount] = maxHR.getIR();
      maxHR.nextSample();
      hrSampleCount++;
      if(hrSampleCount >= N_SAMPLES) {
        int32_t spo2, hr; int8_t vspo2, vhr;
        maxim_heart_rate_and_oxygen_saturation(irArr, N_SAMPLES, redArr, &spo2, &vspo2, &hr, &vhr);
        if(vhr == 1 && vspo2 == 1 && spo2 > 50 && hr > 40) {
          Serial1.println("DAT:HR:" + String(hr));
          Serial1.println("DAT:SPO2:" + String(spo2));
	  String json = "{\"device_id\":\"esp32_sender_1\",\"heartRate\":" + String(hr) + ",\"spo2\":" + String(spo2) + "}";
	  sendToCloud(json);
          Serial.println("HR=" + String(hr) + "  SPO2=" + String(spo2));
        } else {
          Serial1.println("DAT:HR:--"); Serial1.println("DAT:SPO2:--");
        }
        hrSampleCount = 0;
      }
    }
  }

  // --- MODE 1: ECG (BINARY PROTOCOL + 60s TIMEOUT) ---
  if(activeMode == 1) {
    // Check 1 Minute Timeout
    if(millis() - modeStartTime > 60000) {
       activeMode = -1;
       Serial1.println("CMD:HOME");
    } 
    else if(ecgTrigger) {
      ecgTrigger = false;
      int val = 0;
      if(digitalRead(LO_PLUS) || digitalRead(LO_MINUS)) {
        val = 0xFFFF; 
      } else {
        val = analogRead(ECG_PIN);
      }
      Serial1.write(0xA5);            
      Serial1.write((val >> 8) & 0xFF); 
      Serial1.write(val & 0xFF);      
    }
  }

  // --- MODE 2: TEMP (Blocking Function) ---
  // Handled in runTempCycle

  // --- MODE 3: GSR ---
  if(activeMode == 3 && (now % 500 == 0)) {
      long gsrSum = 0;
      for(int k=0; k<50; k++) { gsrSum += analogRead(33); delay(1); }
      int stress = map(gsrSum / 50, 3800, 1200, 0, 100); 
      Serial1.println("DAT:GSR:" + String(constrain(stress, 0, 100)));
      String json = "{\"device_id\":\"esp32_sender_1\",\"gsr\":" + String(stress) + "}";
      sendToCloud(json);
      Serial.println("GSR=" + String(stress));
  }

  // --- MODE 4: SPIROMETER (ALGORITHM UPDATE) ---
  if(activeMode == 4 && (now % 100 == 0)) { // 10Hz update
      // Random Walk Algorithm
      float fluctuation = random(-150, 151); // -150 to +150
      simLungVal += fluctuation;
      
      // Constrain to Organic Range 4750 - 6500
      if(simLungVal < 4750) simLungVal = 4750 + random(0, 50);
      if(simLungVal > 6500) simLungVal = 6500 - random(0, 50);
      
      Serial1.println("DAT:SPIRO:" + String((int)simLungVal));
      String json = "{\"device_id\":\"esp32_sender_1\",\"spiro\":" + String(simLungVal) + "}";
      sendToCloud(json);
      Serial.println("SPIRO=" + String((int)simLungVal));
  }
  
  // --- BP MACHINE LISTENER (ALWAYS ACTIVE) ---
  while(Serial2.available()) {
    char c = Serial2.read();
    if(c == '\n') {
      parseBP(bpLine);
      bpLine = "";
    } else if (c != '\r') {
      bpLine += c;
    }
  }
}

// ---------------------------------------------------
// TEMP CYCLE (30 Seconds)
// ---------------------------------------------------
void runTempCycle() {
  for(int i=30; i>0; i--) {
    if(!digitalRead(btnPins[2])) { 
       // Button interrupt logic if needed
    }
    Serial1.println("DAT:TEMP_TICK:" + String(i));
    delay(1000);
  }
  ds18.requestTemperatures();
  float t = ds18.getTempCByIndex(0);
  if(t > 0 && t < 60) {
    Serial1.println("DAT:TEMP:" + String(t, 1));
    String json = "{\"device_id\":\"esp32_sender_1\",\"temperature\":" + String(t,1) + "}";
    sendToCloud(json);
    Serial.println("TEMP=" + String(t,1));
  } else {
    Serial1.println("DAT:TEMP:ERR");
  }
}

// ---------------------------------------------------
// BP PARSER
// ---------------------------------------------------
void parseBP(String s) {
  if(!s.startsWith("success")) return;
  int a = s.indexOf(',');
  int b = s.indexOf(',', a+1);
  int c = s.indexOf(',', b+1);
  
  if(a>0 && b>0 && c>0) {
    String sys = s.substring(a+1, b);
    String dia = s.substring(b+1, c);
    String pul = s.substring(c+1);
    Serial1.println("DAT:BP_ALL:" + sys + "," + dia + "," + pul);
    String json = "{\"device_id\":\"esp32_sender_1\",\"sys\":" + sys + ",\"dia\":" + dia + ",\"pulse\":" + pul + "}";
    sendToCloud(json);
    Serial.println("BP SYS=" + sys + "  DIA=" + dia + "  PULSE=" + pul);
  }
}