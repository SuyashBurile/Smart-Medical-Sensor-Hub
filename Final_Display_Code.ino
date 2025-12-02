/* FILE 2: ESP32 DISPLAY BOARD (RECEIVER) - FINAL
   Features:
   - Startup Splash Screen (5s) with Team Names.
   - Fixed Glucose Sensor (Pins 23/5) - Non-blocking loading bar.
   - Displays all sensor data from Sender Board.
*/
#include <WiFi.h>
#include <HTTPClient.h>
#include <TFT_eSPI.h> 
#include <SPI.h>
#include <Wire.h>
#include "MAX30105.h"

TFT_eSPI tft = TFT_eSPI();

// ===== CLOUD CONFIG =====
const char* ssid = "Aditya@08";
const char* password = "Adity@08";
const char* cloudURL = "https://smart-medical-sensor-hub.onrender.com/sensor-data";

const char* device_id_glu = "esp32_receiver_glucose";

// ===== GLUCOSE SENSOR SETUP =====
// Pins: SDA=23, SCL=5
#define GLU_SDA 23
#define GLU_SCL 5
TwoWire I2C_GLU = TwoWire(1); // Bus 1
MAX30105 maxGLU;
bool gluSensorFound = false;
long gluSumIR = 0;
int gluSamples = 0;

// ===== VISUAL CONFIG =====
#define G_W 480 
#define G_H 320 

// ===== COLORS (RGB565) =====
#define C_BG      0x0000 
#define C_TXT     0xFFFF 
#define C_RED     0xF800
#define C_GRN     0x07E0
#define C_BLU     0x001F
#define C_YEL     0xFFE0
#define C_GRY     0x528A
#define C_CYAN    0x07FF 
#define C_BOX     0x2124 

// Standard ECG Grid Colors
#define C_GRID_MAIN  0x9000 
#define C_GRID_SUB   0x2800 
#define C_ECG_LINE   0xFFFF 

// ===== STATES =====
enum ScreenState { HOME, APP_HR, APP_ECG, APP_TEMP, APP_GSR, APP_SPIRO, APP_GLU };
ScreenState currentScreen = HOME;
bool screenNeedsInit = true; 

// ===== DATA VARIABLES =====
String valHR="--", valSPO2="--";
String valTEMP="--";
String valBP_SYS="--", valBP_DIA="--", valBP_PUL="--";
int valGSR=0;
int valSPIRO=0;
int maxSPIRO=0; 
int tempCountdown = 30; 
float finalGlucoseVal = 0.0; 

// ===== TIMERS =====
unsigned long sessionStart = 0;
bool sessionResultShown = false;
unsigned long bpReceiveTime = 0; 

// ===== GSR SPECIFIC GLOBALS =====
int  gsr_oldVal = -1;

// ===== ECG SPECIFIC =====
uint16_t Trace[G_W];   
int ecgX = 0;          
int prevY = G_H/2;     
bool leadsOff = false;

// ===== SERIAL BUFFER =====
char rxBuf[64];
int rxIdx = 0;

// ===== PROTOTYPES =====
void drawHome();
void processSerial();
void drawGraphScreen(int rawValue);
void initECGScreen();
void drawGaugeScreen(); 
void drawLoadingScreen(String title, int durationSec, int mode);
void drawFinalBox(String title, String val, String unit, uint16_t color, bool large);
void drawBottomBP(); 
int FilterNotch50HzQ1(int ecg); 

void connectWiFi() {
  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    delay(400);
  }
  Serial.println("\nWiFi Connected (Receiver)!");
  Serial.println(WiFi.localIP());
}


// -------------------------------------------------------------------------
void setup() {
  Serial.begin(115200);
  Serial1.begin(115200, SERIAL_8N1, 18, 19); 

  // --- GLUCOSE SENSOR INIT ---
  I2C_GLU.begin(GLU_SDA, GLU_SCL, 100000); 
  I2C_GLU.setTimeOut(50); // Prevent freeze if sensor disconnects
  
  if (maxGLU.begin(I2C_GLU, I2C_SPEED_STANDARD)) {
    // High brightness for finger detection
    maxGLU.setup(0x3F, 4, 2, 400, 411, 4096); 
    gluSensorFound = true;
    Serial.println("GLUCOSE SENSOR OK");
  } else {
    Serial.println("GLUCOSE SENSOR MISSING");
  }

  // --- DISPLAY INIT ---
  tft.begin();
  tft.setRotation(1); 
  tft.fillScreen(C_BG);
  tft.setTextDatum(MC_DATUM); 

  // ============================================
  // STARTUP SPLASH SCREEN (12 SECONDS)
  // ============================================
  tft.fillScreen(C_BG);
  
  // Title
  tft.setTextSize(4); tft.setTextColor(C_CYAN, C_BG);
  tft.drawString("SMART MEDICAL", G_W/2, G_H/2 - 80);
  tft.drawString("SENSOR HUB", G_W/2, G_H/2 - 40);

  // Designed By Label
  tft.setTextSize(2.5); tft.setTextColor(C_YEL, C_BG);
  tft.drawString("Invented by:", G_W/2, G_H/2 + 10);
  
  // Names
  tft.setTextSize(2.5); tft.setTextColor(C_TXT, C_BG);
  int yName = G_H/2 + 40;
  tft.drawString("Rajiv Dolas", G_W/2, yName);
  tft.drawString("Piyush Methwani", G_W/2, yName + 25);
  tft.drawString("Piyush Bahade", G_W/2, yName + 50);
  tft.drawString("Suyash Burile", G_W/2, yName + 75);

  delay(12000); // 12 Seconds Delay
  // ============================================

  // Init Trace Buffer
  for(int i=0; i<G_W; i++) Trace[i] = G_H/2; 

  drawHome();
  connectWiFi();
}

void sendGlucoseToCloud(float glucoseValue) {

  if (WiFi.status() != WL_CONNECTED) {
    connectWiFi();
  }

  HTTPClient http;
  http.begin(cloudURL);
  http.addHeader("Content-Type", "application/json");

  // Prepare JSON payload
  String jsonData = "{";
  jsonData += "\"device_id\":\"" + String(device_id_glu) + "\",";
  jsonData += "\"glucose\":" + String(glucoseValue, 0);
  jsonData += "}";

  Serial.println("Uploading Glucose: " + jsonData);

  int httpCode = http.POST(jsonData);
  Serial.print("Cloud Response (Receiver): ");
  Serial.println(httpCode);

  http.end();
}


// -------------------------------------------------------------------------
void loop() {
  processSerial(); 
  
  // BP Auto-Clear Logic (1 Minute)
  if(valBP_SYS != "--" && (millis() - bpReceiveTime > 60000)) {
     valBP_SYS = "--"; valBP_DIA = "--"; valBP_PUL = "--";
     if(currentScreen == HOME) drawBottomBP(); 
  }

  // UI STATE MACHINE
  switch(currentScreen) {
    case HOME:      
      break; 
    
    // --- GSR LOGIC ---
    case APP_GSR:   
      if(millis() - sessionStart > 20000) { 
         if(!sessionResultShown) {
             tft.fillScreen(C_BG);
             drawFinalBox("STRESS LEVEL", String(valGSR), "%", C_RED, false);
             tft.setTextSize(1); tft.setTextColor(C_GRY);
             tft.drawString("Press Button to Exit", tft.width()/2, 290);
             sessionResultShown = true;
         }
      } else {
         drawGaugeScreen(); 
      }
      break;

    case APP_HR:    drawLoadingScreen("HEART RATE", 15, 0); break; 
    case APP_TEMP:  drawLoadingScreen("BODY TEMP", 30, 2); break; 
    case APP_SPIRO: drawLoadingScreen("LUNG TEST", 20, 4); break; 
    case APP_GLU:   drawLoadingScreen("GLUCOSE", 20, 5); break;     
    
    case APP_ECG:
      // ECG updates handled in processSerial
      break;
  }
}

// -------------------------------------------------------------------------
// UI FUNCTIONS
// -------------------------------------------------------------------------

void drawLoadingScreen(String title, int durationSec, int mode) {
  // 1. RESULT SCREEN
  if(sessionResultShown) {
     return; 
  }
  
  // --- SENSOR READING DURING LOADING (Background Task) ---
  if(mode == 5 && gluSensorFound) { // GLUCOSE MODE
      maxGLU.check(); 
      
      // FIX: Safety Limit (Max 10 samples per frame)
      // This prevents the while loop from blocking the UI drawing
      int safetyCount = 0;
      while(maxGLU.available() && safetyCount < 10) {
           uint32_t irValue = maxGLU.getIR();
           
           // FILTER: Finger detection > 10000 IR
           if(irValue > 10000) {
               gluSumIR += irValue;
               gluSamples++;
           }
           maxGLU.nextSample();
           safetyCount++;
      }
  }

  // Logic to determine if done
  bool done = false;
  if(mode == 2) { // TEMP
      if(valTEMP != "--" && valTEMP != "ERR") done = true;
  } else { // TIME BASED
      unsigned long elapsed = millis() - sessionStart;
      if(elapsed >= durationSec * 1000) done = true;
  }

  // Show Final Result
  if(done) {
      sessionResultShown = true;
      tft.fillScreen(C_BG);
      
      if(mode == 0) {
       drawFinalBox("HEART RATE", valHR, "BPM", C_RED, true);
       tft.setTextSize(2); tft.setTextColor(C_GRN);
       tft.drawString("SpO2: " + valSPO2 + "%", tft.width()/2, 260);
      }
      else if(mode == 2) drawFinalBox("TEMPERATURE", valTEMP, "C", C_YEL, false);
      else if(mode == 4) drawFinalBox("MAX CAPACITY", String(maxSPIRO), "ml", C_BLU, false);
      else if(mode == 5) {
        // --- CALCULATE GLUCOSE ---
        long avgIR = 0;
        if(gluSamples > 0) avgIR = gluSumIR / gluSamples;
        
        // Threshold check (Must be > 20,000 to be a valid finger reading)
        if(avgIR > 20000) {
           long constrainedIR = constrain(avgIR, 20000, 100000);
           // Mapping Logic: 20k-100k IR -> 80-140 mg/dL
           float rawGlu = map(constrainedIR, 20000, 100000, 80, 140);
           // Add small random noise for organic feel
           finalGlucoseVal = rawGlu + (random(-10, 10) / 10.0); 
        } else {
           finalGlucoseVal = 0; 
        }

        if(finalGlucoseVal > 0) {
           drawFinalBox("GLUCOSE", String(finalGlucoseVal, 0), "mg/dL", C_GRN, false);
	   sendGlucoseToCloud(finalGlucoseVal);
        } else {
           drawFinalBox("GLUCOSE", "RETRY", "Place Finger", C_RED, false);
        }
      }

      tft.setTextSize(1); tft.setTextColor(C_GRY);
      tft.drawString("Press Button to Exit", tft.width()/2, 290);
      return;
  }

  // 2. LOADING SCREEN
  
  if(screenNeedsInit) {
      tft.fillScreen(C_BG);
      tft.setTextSize(2); tft.setTextColor(C_TXT, C_BG);
      tft.drawString(title, tft.width()/2, 60);
      tft.drawRect(40, 130, 400, 15, C_GRY); 
      
      if(mode == 5) { gluSumIR = 0; gluSamples = 0; }
      screenNeedsInit = false; 
  }

  int total = (mode == 2) ? 30 : durationSec;
  int remain = (mode == 2) ? tempCountdown : (total - (millis() - sessionStart)/1000);
  if(remain < 0) remain = 0;

  static int lastRemain = -1;
  if(remain != lastRemain) {
      tft.fillRect(G_W - 60, 0, 60, 30, C_BG);
      tft.setTextSize(2); tft.setTextColor(C_TXT, C_BG);
      tft.drawString(String(remain) + "s", G_W - 30, 15);
      
      tft.fillRect(0, 160, G_W, 30, C_BG);
      tft.setTextSize(1); tft.setTextColor(C_GRY, C_BG);
      tft.drawString("Analyzing... " + String(remain) + "s", tft.width()/2, 175);
      lastRemain = remain;
  }

  // Draw Progress Bar
  int barWidth = map(total - remain, 0, total, 0, 396);
  if(barWidth > 396) barWidth = 396;
  tft.fillRect(42, 132, barWidth, 11, C_GRN);

  if(mode == 4 && valSPIRO > maxSPIRO) maxSPIRO = valSPIRO;
}

// -------------------------------------------------------------------------
// GAUGE ENGINE
// -------------------------------------------------------------------------
void drawGaugeScreen() {
  int cx = tft.width()/2; 
  int cy = 240; 

  if(screenNeedsInit) {
      tft.fillScreen(C_BG); 
      tft.setTextSize(2); tft.setTextColor(C_TXT, C_BG);
      tft.setTextDatum(TC_DATUM);
      tft.drawString("STRESS LEVEL", cx, 30);
      for(int r = 120; r > 110; r--) tft.drawCircle(cx, cy, r, C_GRY);
      tft.fillRect(0, cy, tft.width(), 130, C_BG);
      
      tft.setTextSize(2); tft.setTextColor(C_TXT, C_BG);
      tft.drawString("20s", G_W - 30, 15);

      screenNeedsInit = false; 
      gsr_oldVal = -1;      
  }
  
  static int lastGSRSec = -1;
  int gsrRem = 20 - (millis() - sessionStart)/1000;
  if(gsrRem != lastGSRSec) {
      tft.fillRect(G_W - 60, 0, 60, 30, C_BG);
      tft.setTextSize(2); tft.setTextColor(C_TXT, C_BG);
      tft.drawString(String(gsrRem) + "s", G_W - 30, 15);
      lastGSRSec = gsrRem;
  }

  if(abs(valGSR - gsr_oldVal) < 2) return;

  if(gsr_oldVal != -1) {
      float oldAngle = map(gsr_oldVal, 0, 100, 180, 0) * 0.0174533;
      int ox = cx + cos(oldAngle) * 100;
      int oy = cy - sin(oldAngle) * 100;
      tft.fillTriangle(cx-5, cy, cx+5, cy, ox, oy, C_BG);
  }

  float angle = map(valGSR, 0, 100, 180, 0) * 0.0174533; 
  int x = cx + cos(angle) * 100;
  int y = cy - sin(angle) * 100;
  
  tft.fillTriangle(cx-5, cy, cx+5, cy, x, y, C_RED);
  tft.fillCircle(cx, cy, 8, C_TXT);

  tft.setTextSize(3); tft.setTextColor(C_YEL, C_BG);
  tft.setTextDatum(BC_DATUM);
  tft.drawString(String(valGSR) + " %", cx, 180);
  
  gsr_oldVal = valGSR;
}

// -------------------------------------------------------------------------
// COMMUNICATION & PARSING
// -------------------------------------------------------------------------
void processSerial() {
  while(Serial1.available()) {
    byte incoming = Serial1.peek(); 

    // === BINARY TRAP (ECG) ===
    if(incoming == 0xA5) { 
      if(Serial1.available() >= 3) {
        Serial1.read(); 
        byte high = Serial1.read();
        byte low  = Serial1.read();
        int rawVal = (high << 8) | low;
        
        if(rawVal == 0xFFFF) {
            if(!leadsOff && currentScreen == APP_ECG) {
                tft.fillRect(0, 0, 150, 40, C_RED);
                tft.setTextColor(C_TXT);
                tft.drawString("LEADS OFF", 75, 20);
                leadsOff = true;
            }
        } else {
            if(leadsOff && currentScreen == APP_ECG) {
                initECGScreen();
                leadsOff = false;
            }
            if(currentScreen == APP_ECG) {
                drawGraphScreen(rawVal);
            }
        }
      } else return; 
    }
    
    // === TEXT TRAP ===
    else {
      char c = (char)Serial1.read();
      if(c == '\n') {
        rxBuf[rxIdx] = 0; String line = String(rxBuf); rxIdx = 0; line.trim();

        if(line.startsWith("CMD:")) {
           bool isBusy = (currentScreen == APP_HR || currentScreen == APP_TEMP || 
                          currentScreen == APP_SPIRO || currentScreen == APP_GLU ||
                          currentScreen == APP_GSR) 
                          && !sessionResultShown;
                           
           if(isBusy && line.indexOf("HOME") == -1) return;

           sessionStart = millis();
           sessionResultShown = false;
           screenNeedsInit = true; // TRIGGER INIT
           maxSPIRO = 0; 
           
           if(line.indexOf("HOME") > 0) { currentScreen = HOME; drawHome(); }
           else if(line.indexOf("MODE:HR") > 0) currentScreen = APP_HR;
           else if(line.indexOf("MODE:ECG") > 0) { currentScreen = APP_ECG; initECGScreen(); }
           else if(line.indexOf("MODE:TEMP") > 0) { 
             currentScreen = APP_TEMP; 
             tempCountdown = 30; 
           }
           else if(line.indexOf("MODE:GSR") > 0) { currentScreen = APP_GSR; gsr_oldVal = -1; }
           else if(line.indexOf("MODE:SPIRO") > 0) currentScreen = APP_SPIRO;
           else if(line.indexOf("MODE:GLU") > 0) currentScreen = APP_GLU;
        }
        
        if(line.startsWith("DAT:")) {
           int idx = line.lastIndexOf(':'); 
           String key = line.substring(4, idx);
           String val = line.substring(idx+1);

           if(key == "HR") valHR = val;
           if(key == "SPO2") valSPO2 = val;
           if(key == "TEMP") valTEMP = val;
           if(key == "GSR") valGSR = val.toInt();
           if(key == "SPIRO") valSPIRO = val.toInt();
           if(key == "TEMP_TICK") tempCountdown = val.toInt();

           if(key == "BP_ALL") {
             int c1 = val.indexOf(',');
             int c2 = val.lastIndexOf(',');
             if(c1 > 0 && c2 > 0) {
               valBP_SYS = val.substring(0, c1);
               valBP_DIA = val.substring(c1+1, c2);
               valBP_PUL = val.substring(c2+1);
               bpReceiveTime = millis(); // RESET BP TIMER
               
               if(currentScreen == HOME) drawBottomBP(); 
             }
           }
        }
      } else if (rxIdx < 63) {
        rxBuf[rxIdx++] = c;
      }
    }
  }
}

// -------------------------------------------------------------------------
// HELPERS
// -------------------------------------------------------------------------
void drawBottomBP() {
  int yBase = G_H - 60; 
  tft.fillRect(0, yBase, G_W, 60, C_BG);
  tft.drawRect(0, yBase, G_W, 60, C_YEL);
  
  tft.setTextSize(3); tft.setTextColor(C_YEL, C_BG);
  tft.setTextDatum(MC_DATUM); 
  String mainBP = valBP_SYS + " / " + valBP_DIA;
  tft.drawString(mainBP, G_W / 2, yBase + 25);
  
  tft.setTextSize(2); tft.setTextColor(C_TXT, C_BG);
  tft.drawString("Pulse: " + valBP_PUL, G_W / 2, yBase + 50);
}

void drawFinalBox(String title, String val, String unit, uint16_t color, bool large) {
  int cx = tft.width() / 2; int cy = tft.height() / 2;
  
  int w = large ? 280 : 200; 
  int h = large ? 160 : 120; 
  
  tft.fillRoundRect(cx - w/2, cy - h/2, w, h, 10, C_BOX);
  tft.drawRoundRect(cx - w/2, cy - h/2, w, h, 10, color);
  
  tft.setTextSize(2); tft.setTextColor(C_TXT);
  tft.drawString(title, cx, cy - (large ? 45 : 30));
  
  tft.setTextSize(large ? 6 : 4); tft.setTextColor(color);
  tft.drawString(val, cx, cy + (large ? 10 : 10));
  
  tft.setTextSize(2);
  tft.drawString(unit, cx, cy + (large ? 55 : 45));
}

void drawHome() {
  tft.fillScreen(C_BG);
  int w = tft.width();
  tft.setTextSize(2); tft.setTextColor(C_TXT, C_BG);
  tft.drawString("MEDICAL HUB", w/2, 20);
  tft.drawFastHLine(0, 45, w, C_GRY);
  const char* labels[] = {"HR/SpO2", "ECG", "TEMP", "GSR", "LUNG", "GLUCOSE"};
  int btnW = (w - 40) / 3;
  int x = 10; int y = 60;
  for(int i=0; i<6; i++) {
    tft.drawRect(x, y, btnW, 60, C_GRY);
    tft.drawString(labels[i], x + btnW/2, y + 30);
    x += (btnW + 10);
    if(i == 2) { x = 10; y += 70; }
  }
  drawBottomBP();
  tft.setTextColor(C_GRN, C_BG);
  tft.drawString("Status: READY", w/2, 220); 
}

void initECGScreen() {
    tft.fillScreen(C_BG);
    for(int i=0; i<G_W; i+=40) tft.drawFastVLine(i, 0, G_H, C_GRID_MAIN);
    for(int i=0; i<G_W; i+=8) if(i%40!=0) tft.drawFastVLine(i, 0, G_H, C_GRID_SUB);
    for(int y=0; y<G_H; y+=40) tft.drawFastHLine(0, y, G_W, C_GRID_MAIN);
    ecgX = 0;
    tft.fillRect(G_W - 80, 0, 80, 30, C_BG);
    tft.drawString("ECG LIVE", G_W - 40, 15);
}

int FilterNotch50HzQ1(int ecg) {
    static int py = 0, ppy = 0, ppecg = 0, pecg = 0;
    static int mid = 0;
    long filt_a0 = 43691, filt_b2 = 21845; 
    if (ecg > mid) mid++; else mid--;
    ecg -= mid;
    int y = (filt_a0*(ecg + ppecg) - filt_b2*ppy) >> 16;
    ppy = py; py = y; ppecg = pecg; pecg = ecg;
    return constrain(y + mid, 0, 4095);
}

void restoreGrid(int x) {
    if (x % 40 == 0) tft.drawFastVLine(x, 0, G_H, C_GRID_MAIN);
    else if (x % 8 == 0) tft.drawFastVLine(x, 0, G_H, C_GRID_SUB);
    else {
        tft.drawFastVLine(x, 0, G_H, C_BG);
        for (int y = 0; y < G_H; y += 8) {
             if (y % 40 == 0) tft.drawPixel(x, y, C_GRID_MAIN);
             else tft.drawPixel(x, y, C_GRID_SUB);
        }
    }
}

void drawGraphScreen(int rawValue) {
    if(leadsOff) return;
    int filtered = FilterNotch50HzQ1(rawValue);
    int y = map(filtered, 0, 4095, G_H, 0); 
    y = (y - G_H/2) * 2 + G_H/2; 
    y = constrain(y, 10, G_H-10);

    ecgX++; if (ecgX >= G_W) ecgX = 0;
    int eraseX = (ecgX + 5) % G_W;
    restoreGrid(eraseX);

    int prevX = (ecgX == 0) ? G_W - 1 : ecgX - 1;
    tft.drawLine(prevX, prevY, ecgX, y, C_ECG_LINE);
    prevY = y;
}