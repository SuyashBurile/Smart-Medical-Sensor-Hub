/*
   ESP32 Display Board – Smart Medical Sensor Hub (7 Sensors)
   Authors: Rajiv Dolas + ChatGPT GPT-5
   - Shows startup splash for 5s
   - Displays 7 sensors (includes new Glucose MAX30102)
   - Glucose sensor connected to SDA=23, SCL=5
*/

#include <TFT_eSPI.h>
#include <SPI.h>
#include <Wire.h>
#include "MAX30105.h"

TFT_eSPI tft = TFT_eSPI();

// --- Colors ---
#define C_BG       TFT_BLACK
#define C_LABEL    TFT_CYAN
#define C_INACTIVE TFT_DARKGREY
#define C_HR       TFT_RED
#define C_SPO2     TFT_GREEN
#define C_TEMP     TFT_ORANGE
#define C_BP       TFT_YELLOW
#define C_GSR      TFT_MAGENTA
#define C_SPIRO    TFT_BLUE
#define C_ECG      TFT_WHITE
#define C_GLU      TFT_PINK

// --- States ---
bool onHR=false,onTEMP=false,onECG=false,onBP=false,onGSR=false,onSPIRO=false,onGLU=false;

// --- Readings ---
int hr=-1,spo2=-1,bp_sys=-1,bp_dia=-1,bp_pul=-1,gsr=-1,spiro=-1;
float tempC=NAN,glucose=0;

// --- ECG ---
const int ECG_BUF=240;
int ecgBuf[ECG_BUF];
int ecgHead=0;
bool ecgActive=false;

// --- Layout ---
int scrW,scrH,rowH;

// --- Glucose Sensor ---
TwoWire I2C2(1);
MAX30105 maxGLU;
bool gluReady=false;

// --- Functions ---
void drawLayout();
void drawSensorSection(const char*,int,int,bool,const char*);
void updateValue(const char*,const char*);
void pushECG(int);
void drawECG();
void measureGlucose();

// --------------------------------------------------
void setup(){
  Serial.begin(115200);
  Serial1.begin(115200, SERIAL_8N1, 18, 19);

  tft.begin();
  tft.setRotation(1);
  scrW=tft.width();
  scrH=tft.height();
  rowH=scrH/10;

  // --- Splash Screen ---
  tft.fillScreen(TFT_BLACK);
  tft.setTextColor(TFT_CYAN); tft.setTextSize(3);
  tft.setCursor(20, scrH/3); tft.println("SMART MEDICAL");
  tft.setCursor(20, scrH/3 + 40); tft.println("SENSOR HUB");
  tft.setTextSize(2); tft.setTextColor(TFT_WHITE);
  tft.setCursor(20, scrH/3 + 90);
  tft.println("Designed by:");
  tft.setCursor(20, scrH/3 + 110);
  tft.println("PIYUSH METHWANI");
  tft.setCursor(20, scrH/3 + 130);
  tft.println("PIYUSH BAHADE");
  tft.setCursor(20, scrH/3 + 150);
  tft.println("RAJIV DOLAS");
  tft.setCursor(20, scrH/3 + 170);
  tft.println("SUYASH BURILE");
  delay(5000);

  // --- Init Glucose MAX30102 ---
  I2C2.begin(23,5,400000);
  if(maxGLU.begin(I2C2)) {
    maxGLU.setup();
    gluReady=true;
    Serial.println("✅ Glucose MAX30102 Ready");
  } else Serial.println("⚠ Glucose sensor not found");

  drawLayout();
  Serial.println("Display ready...");
}

String line="";
void loop(){
  while(Serial1.available()){
    char c=(char)Serial1.read();
    if(c=='\r') continue;
    if(c=='\n'){
      line.trim();
      if(line.length()>0){
        if(line.endsWith("_ON")||line.endsWith("_OFF")){
          String base=line.substring(0,line.indexOf('_'));
          bool state=line.endsWith("_ON");
          base.toUpperCase();
          if(base=="HR"){onHR=state; drawLayout();}
          else if(base=="TEMP"){onTEMP=state; drawLayout();}
          else if(base=="ECG"){onECG=state; ecgActive=state; drawLayout();}
          else if(base=="BP"){onBP=state; drawLayout();}
          else if(base=="GSR"){onGSR=state; drawLayout();}
          else if(base=="SPIRO"){onSPIRO=state; drawLayout();}
          else if(base=="GLU"){onGLU=state; drawLayout(); if(onGLU) measureGlucose();}
        }
        else{
          int idx=line.indexOf(':');
          if(idx>0){
            String key=line.substring(0,idx); String val=line.substring(idx+1);
            key.toUpperCase(); val.trim();
            updateValue(key.c_str(),val.c_str());
          }
        }
      }
      line="";
    } else line+=c;
  }

  if(ecgActive) drawECG();
  delay(5);
}

// --------------------------------------------------
void drawLayout(){
  tft.fillScreen(C_BG);
  tft.setTextColor(C_LABEL); tft.setTextSize(2);
  tft.setCursor(10,5); tft.print("SMART MEDICAL SENSOR HUB");

  int y=rowH;
  drawSensorSection("Heart Rate / SpO2",y,onHR?C_HR:C_INACTIVE,onHR,onHR?"Measuring...":"Standby");
  y+=rowH;
  drawSensorSection("Temperature",y,onTEMP?C_TEMP:C_INACTIVE,onTEMP,onTEMP?"Measuring...":"Standby");
  y+=rowH;
  drawSensorSection("Blood Pressure",y,onBP?C_BP:C_INACTIVE,onBP,onBP?"Measuring...":"Standby");
  y+=rowH;
  drawSensorSection("GSR (Stress)",y,onGSR?C_GSR:C_INACTIVE,onGSR,onGSR?"Active":"Standby");
  y+=rowH;
  drawSensorSection("Spirometer",y,onSPIRO?C_SPIRO:C_INACTIVE,onSPIRO,onSPIRO?"Active":"Standby");
  y+=rowH;
  drawSensorSection("ECG",y,onECG?C_ECG:C_INACTIVE,onECG,onECG?"Live":"Standby");
  y+=rowH;
  drawSensorSection("Glucose",y,onGLU?C_GLU:C_INACTIVE,onGLU,onGLU?"Measuring...":"Standby");

  if(onECG){
    tft.drawRect(10, scrH-rowH*3, scrW-20, rowH*2, TFT_DARKGREY);
    tft.setCursor(14, scrH-rowH*3-16); tft.setTextSize(1); tft.setTextColor(C_LABEL); tft.print("ECG Waveform");
  }
}

void drawSensorSection(const char* name,int y,int color,bool active,const char* status){
  tft.fillRect(10,y,scrW-20,rowH-4,active?TFT_DARKGREY:TFT_BLACK);
  tft.drawRect(10,y,scrW-20,rowH-4,color);
  tft.setTextSize(2); tft.setTextColor(color);
  tft.setCursor(20,y+8); tft.print(name);
  tft.setTextSize(1); tft.setTextColor(TFT_WHITE);
  tft.setCursor(scrW/2,y+10); tft.print(status);
}

// --------------------------------------------------
void updateValue(const char* key,const char* val){
  String k=key; k.toUpperCase();
  int y=0;
  if(k=="HR"){y=rowH; tft.setCursor(scrW/2,rowH+10); tft.setTextColor(C_HR); tft.printf("%s BPM",val);}
  else if(k=="SPO2"){y=rowH; tft.setCursor(scrW-120,rowH+10); tft.setTextColor(C_SPO2); tft.printf("%s%%",val);}
  else if(k=="TEMP"){y=rowH*2; tft.setCursor(scrW/2,y+10); tft.setTextColor(C_TEMP); tft.printf("%s C",val);}
  else if(k=="BP_SYS"){bp_sys=atoi(val);}
  else if(k=="BP_DIA"){bp_dia=atoi(val);}
  else if(k=="BP_PUL"){bp_pul=atoi(val);}
  else if(k=="GSR"){y=rowH*4; int barW=map(atoi(val),0,4095,0,scrW-150); tft.fillRect(scrW/2,y+8,barW,10,C_GSR);}
  else if(k=="SPIRO"){y=rowH*5; int barW=map(atoi(val),0,4095,0,scrW-150); tft.fillRect(scrW/2,y+8,barW,10,C_SPIRO);}
  else if(k=="ECG"){pushECG(atoi(val));}
  else if(k=="GLU"){y=rowH*7; tft.setCursor(scrW/2,y+10); tft.setTextColor(C_GLU); tft.printf("%s mg/dL",val);}

  if(k.startsWith("BP")){
    y=rowH*3;
    tft.fillRect(scrW/2,y+8,150,20,C_BG);
    tft.setCursor(scrW/2,y+10); tft.setTextColor(C_BP);
    if(bp_sys>0 && bp_dia>0) tft.printf("%d/%d mmHg",bp_sys,bp_dia);
  }
}

// --------------------------------------------------
void pushECG(int val){
  if(!ecgActive) return;
  val=constrain(val,0,4095);
  ecgBuf[ecgHead]=val;
  ecgHead=(ecgHead+1)%ECG_BUF;
}

void drawECG(){
  if(!ecgActive) return;
  int baseY=scrH-rowH*2;
  int h=rowH*2-10;
  tft.fillRect(12,baseY-h,scrW-24,h,C_BG);
  for(int i=1;i<ECG_BUF;i++){
    int x1=12+i-1;
    int x2=12+i;
    int y1=baseY - map(ecgBuf[(ecgHead+i-1)%ECG_BUF],0,4095,0,h);
    int y2=baseY - map(ecgBuf[(ecgHead+i)%ECG_BUF],0,4095,0,h);
    tft.drawLine(x1,y1,x2,y2,C_ECG);
  }
}

// --------------------------------------------------
void measureGlucose(){
  if(!gluReady){Serial.println("⚠ Glucose not ready"); return;}
  Serial.println("🧪 Measuring Glucose...");
  unsigned long start=millis();
  long sumIR=0; int n=0;
  while(millis()-start<2000){
    if(maxGLU.check()){
      sumIR+=maxGLU.getIR(); n++;
    }
  }
  if(n>0){
    glucose = map(sumIR/n, 20000, 100000, 80, 140); // simulated mg/dL
    Serial.printf("GLUCOSE=%.1f mg/dL\n",glucose);
    updateValue("GLU", String(glucose,1).c_str());
  } else {
    updateValue("GLU","---");
  }
  onGLU=false;
}
