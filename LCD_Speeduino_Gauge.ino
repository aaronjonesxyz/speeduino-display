/*  Speeduino LCD Gauge - Aaron Jones 2020

    The gaugeMap array contains the Serial3 data indexes that will be displayed, a full list can be found at:
    https://speeduino.com/wiki/index.php/Secondary_Serial_IO_interface#Retrieve_realtime_data

*/

//#define HEADLESS   // uncomment to run the logger with no LCD

#include <Arduino.h>
#ifndef HEADLESS
#include "TFT_22_ILI9225.h"
#endif
#define SDCARD_SPI SPI2
#include <SPI.h>
#include <SD.h>

#include <../fonts/FreeSans12pt7b.h>
#include <../fonts/FreeSans24pt7b.h>

#define TFT_RST PA1
#define TFT_RS  PA2
#define TFT_CS  PA0 // SS
#define TFT_SDI PA7 // MOSI
#define TFT_CLK PA5 // SCK
#define TFT_LED 0
#define TFT_BRIGHTNESS 200

#define LOG_UPDATE 50

unsigned long GaugeUpdateMillis = 0;
unsigned long LogUpdateMillis = 0;

#ifndef HEADLESS
TFT_22_ILI9225 tft = TFT_22_ILI9225(TFT_RST, TFT_RS, TFT_CS, TFT_LED, TFT_BRIGHTNESS);
#endif

extern uint8_t Source_Code_Pro_Black37x62[];

File file;
int fnum;
char filename[12];
byte nReply = 0;
byte serialData[40];
uint16_t parsedData[40];
unsigned long serialMillis = 0;
int i = 0;
int gaugeMap[6] = { 5, 6, 16, 15, 20, 4 };
int gaugePosMap[6][2] = { { 0, 0 }, { 0, 90 }, { 0, 140 }, { 135, 0 }, { 135, 70 }, { 135, 140} };
int gaugeCurr = 0;

const char header1[] PROGMEM  = {"Time,SecL,Squirt,EngStatus,Dwell,MAP,IAT,CLT,BattCorr,BatteryV,AFR,EGOCorrection,IATCorrection,WUE,RPM,AE,GammaCorr,VE,AFRTarget,PW1,TPSDOT,Advance,TPS,loopsPerSecond,freeRAM,boostTarget,boostPWM,Spark,RPMDOT,Eth,FlexCorr,IgnCorr,idleLoad,testOutputs,AFR2,Baro"};
const char header2[] PROGMEM  = {"sec,sec,,,ms,kpa,C,C,%,V,AFR,%,%,%,RPM,%,%,%,AFR,%,DOT,,%,LPS,b,kpa,%,bits,DOT,%,%,deg,%,bits,AFR,kpa"};
const char *gaugeUnits[] PROGMEM = {"","","","ms","kpa","C","C","%","V","AFR","%","%","%","RPM","%","%","%","AFR","%","DOT","","%","LPS","b","kpa","%","bits","DOT","%","%","deg","%","bits","AFR","kpa"};
const char *gaugeNames[] PROGMEM = {"SecL","Squirt","EngStatus","Dwell","MAP","IAT","CLT","Batt.C","BattV","AFR","EGO.C","IAT.C","WUE","RPM","AE","Gamma.C","VE","AFRTarget","PW1","TPSDOT","Advance","TPS","loopsPerSecond","freeRAM","boostTarget","boostPWM","Spark","RPMDOT","Eth","FlexCorr","IgnCorr","idleLoad","testOutputs","AFR2","Baro"};

void gaugeUpdate( int );
void updateTimer( void );
void parseData( void );
void resetData( void );

HardwareSerial Serial3(PB11, PB10);

void setup() {

  #ifndef HEADLESS
    tft.begin();
    tft.clear();

    tft.setOrientation(3); // If your LCD isn't oriented correctly, change this number. 

    tft.setFont(Terminal12x16);
  #endif
  
  Serial3.begin(115200);
  Serial.begin(115200);

  Serial.println("Initializing SD card...");
  
  // see if the card is present and can be initialized:
  if (!SD.begin(PB12)) {
    Serial.println("Card failed, or not present");
    #ifndef HEADLESS
      tft.drawText(0,0,"No SD Card.");
      delay(250);
      tft.drawText(0,0,"                    ");
    }
    // don't do anything more:
    return;
  }
  Serial.println("Card initialized.");
  #ifndef HEADLESS
    tft.drawText(0,0,"SD Init.");
    delay(50);
    tft.drawText(0,0,"                    ");
  }
  if( SD.exists("f.num") ){
    Serial.println("Counter file found.");
    file = SD.open("f.num", O_RDWR);
    if(file){
      fnum = file.read();
      Serial.print(fnum);
      Serial.print(" log files\n\r");
      file.seek(0);
      fnum++;
      file.write(fnum);
      file.close();
    } else{
      Serial.println("File I/O error!");
    }
  } else {
    file = SD.open("f.num", FILE_WRITE);
    if(file){
      file.write(int(0));
      file.close();
      fnum=0;
    } else {
      Serial.println("File I/O error!");
    }
  }
  sprintf(filename, "SDLog%d.csv", fnum );
  Serial.println(filename);
  file = SD.open(filename, FILE_WRITE);
  file.close();
  file = SD.open(filename, O_WRITE | O_CREAT);
  Serial.println(file);
  if(file){
    file.println(header1);
    file.println(header2);
    file.flush();
  } else {
    Serial.println("File I/O Error!");
  }
}

void loop() {

  updateTimer();

}

void updateTimer() {
    if( ( millis() - LogUpdateMillis ) >= LOG_UPDATE ){
      resetData();
      Serial3.print("n");

      serialMillis = millis();
      while( Serial3.available() == 0 ){ if( millis() - serialMillis > 100 ){ break; } }
      i = 0;
      while( Serial3.available() > 0 ){
        if( i == 0 ){
          nReply = Serial3.read();
        }else if( i > 2 && i < 40 ){
          serialData[i-3] = Serial3.read();
        } else {
          Serial3.read();
        }
        i++;
      }
      if( nReply == 110 ){
        parseData();
        runLog();
      }

      LogUpdateMillis = millis();
      Serial.println(LogUpdateMillis);

      if( gaugeCurr > 5 && !HEADLESS ){ 
      gaugeCurr = 0;
      }
      gaugeUpdate( gaugeCurr );
      gaugeCurr++;
    
  }
}

void gaugeUpdate(int i) {
   if( i == 0 ){
     tft.drawText( gaugePosMap[0][0], gaugePosMap[0][1], String( gaugeNames[gaugeMap[0]] ), COLOR_BLUE );
     tft.setFont(Source_Code_Pro_Black37x62);
     tft.drawText( gaugePosMap[0][0], (gaugePosMap[0][1]+20), String(parsedData[gaugeMap[0]])+"  ", COLOR_YELLOWGREEN );
     tft.setFont(Terminal12x16);
   } else {
     tft.drawText( gaugePosMap[i][0], gaugePosMap[i][1], String( gaugeNames[gaugeMap[i]] ), COLOR_BLUE );
     tft.drawText( gaugePosMap[i][0], (gaugePosMap[i][1]+20), String(parsedData[gaugeMap[i]])+"    ", COLOR_YELLOWGREEN );
  }
}

void runLog() {
  if(file){
    char tC[6];
    
    float t = millis()/1000.00;
    uint16_t tInt = t;
    int tMs = (t - tInt)*100.00;

    sprintf(tC,"%d.%02d,",tInt,tMs);
    file.print(tC);
    for( i = 0; i < 34; i++ ){
      char d[4];
      sprintf(d,"%d,",parsedData[i]);
      file.print(d);
    }
    i++;
    char d[4];
    sprintf(d,"%d",parsedData[i]);
    file.println(d);
    file.flush();
  } else {
    Serial.println("File I/O Error!");
  }
}

// TODO: floats for AFR/Lambda, BattV, etc. values
void parseData() {
  for( int i = 0; i < 40; i++ ){
    switch( i ){
      case 4:
        parsedData[4] = ( serialData[5] << 8 ) | serialData[4];
        break;

      case 5:
        if( serialData[6] ){
          parsedData[5] = serialData[6] - 40; // IAT offset
          break;
        }

      case 6:
        if( serialData[7] ){
          parsedData[6] = serialData[7] - 40; // CLT offset
          break;
        }
        
      case 13:
        parsedData[13] = ( serialData[15] << 8 ) | serialData[14];
        break;

      case 19:
        parsedData[19] = ( serialData[21] << 8 ) | serialData[20];
        break;
      
      case 23:
        parsedData[23] = ( serialData[26] << 8 ) | serialData[25];
        break;

      case 24:
        parsedData[24] = ( serialData[28] << 8 ) | serialData[27];
        break;
      
      case 27:
        parsedData[27] = ( serialData[33] << 8 ) | serialData[32];
        break;

      default:
        if( i > 27 ){
          parsedData[i] = serialData[i+6];
        }else if( i > 24 ){
          parsedData[i] = serialData[i+5];
        }else if( i > 23 ){
          parsedData[i] = serialData[i+4];
        }else if( i > 19 ){
          parsedData[i] = serialData[i+3];
        }else if( i > 13 ){
          parsedData[i] = serialData[i+2];
        }else if( i > 4 ){
          parsedData[i] = serialData[i+1];
        }else{
          parsedData[i] = serialData[i];
        }
        break;
    }
  }
}

void resetData() {
  for( int i = 0; i < 40; i++ ){
    serialData[i] = 0;
    parsedData[i] = 0;
  }
}
