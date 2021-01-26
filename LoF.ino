/*  Life of Flowers.
 *  Based on: ESP8266, AM2302 and Blynk.
 *  Created in Arduino IDE.
 *  For more details please visit http://openwind.ru
 *  Contact us: hello@openwind.ru
*/

#include <FS.h>
//#include <EEPROM.h>
#include "version.h"
#include <string.h>

#define BLYNK_PRINT Serial
//blynk
#include <ESP8266WiFi.h>
#include <BlynkSimpleEsp8266.h>

//RTC Time
#include <TimeLib.h>
#include <WidgetRTC.h>

//WiFiManager
#include <DNSServer.h>
#include <ESP8266WebServer.h>
#include <WiFiManager.h>
#include <ArduinoJson.h>

//LED status
#include <Ticker.h>

#include <DHT.h>

#define DHTPIN 12
#define DHTTYPE DHT22   // DHT 22  (AM2302), AM2321
DHT dht(DHTPIN, DHTTYPE);

#define BLYNK_GREEN     "#23C48E"
#define BLYNK_BLUE      "#04C0F8"
#define BLYNK_YELLOW    "#ED9D00"
#define BLYNK_RED       "#D3435C"
#define BLYNK_DARK_BLUE "#5F7CD8"

WidgetLED led1(V10);
WidgetLED led2(V11);
Ticker ticker;

WidgetRTC rtc;

int ADCPin = A0;
int PowerADCPin = 4;
int PWMPin = 15;
int BPin = 14;
int C_DHTPin = 13;
int buzPin = 5;
int ledPin = 15;
int buttonPin = 0;
int buttonState = 1;

float adclight;
float adcwater;
float adcbattery;
float h;
float t;
float f;

int adcwater_int;
int adclight_int;
int adcbattery_int;
int h_int;
int t_int;


// to del?
float old_h = 0;
float old_t = 0;
float old_f = 0;

bool DHTreadOK; //false if not read

char blynk_token[34];

//add EEPROM read + blynk widget
double sleep_time = 3600; // 60 min

bool shouldSaveConfig = false; //flag for saving data

int hour_i;
int minute_i;
int second_i;
int day_i;
int month_i;

String hour_s;
String minute_s;
String second_s;
String day_s;
String month_s;

String currentTime;
String currentDate;

//int address = 0;

// Check flash size
String realSize = String(ESP.getFlashChipRealSize());
String ideSize = String(ESP.getFlashChipSize());
bool flashCorrectlyConfigured = realSize.equals(ideSize);

//if(flashCorrectlyConfigured){
//  Serial.print("\r\nflash correctly configured, SPIFFS starts, IDE size: " + ideSize + ", match real size: " + realSize);
//}
//else{
//  Serial.print("\r\nflash incorrectly configured, SPIFFS cannot start, IDE size: " + ideSize + ", real size: " + realSize);
//}


// NVM Data
#define RTCMemOffset 10 // arbitrary location
#define MAGIC_NUMBER 56 // used to know if the memory is good and been written to 

typedef struct{
    int wakeCount;
    bool bFirstTime;
    bool bShouldRepeat;
    int magicNumber;
} nvmData;

nvmData sleepMemory;

bool bDoneSleeping;

BLYNK_CONNECTED(){
  rtc.begin();
  setSyncInterval(1); // interval of RTC sync
  Blynk.syncAll();
//  rtc.begin();
//  setSyncInterval(1); // interval of RTC sync
//  Blynk.syncAll();  
}

WidgetTerminal terminal(V6);

void tick(){
  //toggle state
  int state = digitalRead(ledPin); 
  digitalWrite(ledPin, !state); 
}

void configModeCallback (WiFiManager *myWiFiManager){
  //gets called when WiFiManager enters configuration mode
  Serial.println("Entered config mode");
  Serial.println(WiFi.softAPIP());
  //if you used auto generated SSID, print it
  Serial.println(myWiFiManager->getConfigPortalSSID());
  //entered config mode, make led toggle faster
  ticker.attach(0.2, tick);
}

void saveConfigCallback(){  //callback notifying us of the need to save config
  Serial.println("Should save config");
  shouldSaveConfig = true;
  ticker.attach(0.2, tick);  // led toggle faster

}


//int n;
float adcRead[3];
void quickSort(float *s_arr, int first, int last){
  if (first < last){
      int left = first, right = last, middle = s_arr[(left + right) / 2];
      do{
        while (s_arr[left] < middle) left++;
        while (s_arr[right] > middle) right--;
        if (left <= right){
          int tmp = s_arr[left];
          s_arr[left] = s_arr[right];
          s_arr[right] = tmp;
          left++;
          right--;
        }
      } 
      while (left <= right);
      quickSort(s_arr, first, right);
      quickSort(s_arr, left, last);
  }
}

void analogReadMedian(){
  //add for
  adcRead[0] = analogRead(ADCPin);
  delay(10);
  adcRead[1] = analogRead(ADCPin);
  delay(10);
  adcRead[2] = analogRead(ADCPin);  
}

void readADC_median2(int input){
  switch(input){
    case 1 :
      digitalWrite(BPin, HIGH);
      digitalWrite(C_DHTPin, LOW);
      delay(50);
      analogReadMedian();
      quickSort(adcRead, 0, 2);
      adcbattery = adcRead[1] * 43 / 10;
      digitalWrite(BPin, LOW);
      break;
    case 2 :
      digitalWrite(BPin, LOW);
      digitalWrite(C_DHTPin, LOW);
      analogWrite(PWMPin, 412);
      delay(50);
      analogReadMedian();
      quickSort(adcRead, 0, 2);
      adcwater = adcRead[1];
      analogWrite(PWMPin, 0);
      break;      
    case 3 :
      digitalWrite(BPin, LOW);
      digitalWrite(C_DHTPin, HIGH);
      delay(50);
      analogReadMedian();
      quickSort(adcRead, 0, 2);
      adclight = adcRead[1];
      break;
    default :
      delay(1);
   }  
}

void readDHT22(){
  
  DHTreadOK = false;
  int i = 0;
  Serial.print("\n\rReading DHT22 sensor:");
  while (i < 5 && !DHTreadOK){
    delay(i*75);
    h = dht.readHumidity();
    t = dht.readTemperature();
    f = dht.readTemperature(true); // Read temperature as Fahrenheit (isFahrenheit = true)
    
    if (isnan(h) || isnan(t) || isnan(f)){
      Serial.print(".");
      i++;
    }
    else{   
      DHTreadOK = true;
      if (!isnan(h)) old_h = h;
      if (!isnan(t)) old_t = t;
      if (!isnan(f)) old_f = f;
    }
    if (DHTreadOK){ 
    Serial.print(" ok");
    }
    else{
    Serial.print(" failed");
    }
  }
}

void tones(uint8_t _pin, unsigned int frequency, unsigned long duration){  
  pinMode (_pin, OUTPUT);
  analogWriteFreq(frequency);
  analogWrite(_pin,500);
  delay(duration);
  analogWrite(_pin,0);  
}

void setup(){

  WiFi.mode(WIFI_OFF);
  WiFi.forceSleepBegin();
  delay(1);
  dht.begin();
  Serial.begin(9600);

  /* Woke up and entered setup */
  bDoneSleeping = false;
  
  if(ESP.rtcUserMemoryRead((uint32_t)RTCMemOffset, (uint32_t*) &sleepMemory, sizeof(sleepMemory))){
      if(sleepMemory.magicNumber != MAGIC_NUMBER) // memory got corrupt or this is the first time
      {
          sleepMemory.bFirstTime = true;
          sleepMemory.bShouldRepeat = false;
          sleepMemory.magicNumber = MAGIC_NUMBER;
          sleepMemory.wakeCount = 4;
      }
      else
      {
          sleepMemory.wakeCount += 1; // incrememnt the sleep counter
          sleepMemory.bFirstTime = false;
      }
  
      if(sleepMemory.wakeCount >= 4 || sleepMemory.bShouldRepeat) // reset the counter and do whatever else you need to do since 4 hours have passed
      {
          sleepMemory.wakeCount = 0;
          sleepMemory.bShouldRepeat = false;
          bDoneSleeping = true;          
      }   
  
      ESP.rtcUserMemoryWrite((uint32_t)RTCMemOffset, (uint32_t*) &sleepMemory, sizeof(sleepMemory)); // write the new values to memory
  }
  
  if(!bDoneSleeping){
    Serial.print("\r\nsleepMemory.wakeCount: ");
    Serial.print(sleepMemory.wakeCount);  
    Serial.print("\r\nGo to SLEEP");
    sleep_time = sleep_time * 1000000;
    ESP.deepSleep(sleep_time, WAKE_RF_DISABLED);
  }
  else{
    Serial.print("\r\nsleepMemory.wakeCount: ");
    Serial.print(sleepMemory.wakeCount);  
    Serial.print("\r\nGo to WORK");
  }
 
 
  pinMode(PowerADCPin, OUTPUT);
  pinMode(PWMPin, OUTPUT);

  pinMode(BPin, OUTPUT);
  pinMode(C_DHTPin, OUTPUT);
  
  digitalWrite(BPin, LOW);
  digitalWrite(C_DHTPin, LOW);
  digitalWrite(PowerADCPin, LOW);
  analogWrite(PWMPin, 0); 

  //pinMode(ledPin, OUTPUT);
  pinMode(buzPin, OUTPUT);
  
  pinMode(buttonPin, INPUT);  
  pinMode(ADCPin, INPUT);

  //ESP.wdtDisable();
  //SPIFFS.format();
  //read configuration from FS json
  Serial.print("\n\rmounting FS...");
  if (SPIFFS.begin()){  
  Serial.println("mounted file system");
  if (SPIFFS.exists("/config.json")) {
    //file exists, reading and loading
    Serial.println("reading config file");
    File configFile = SPIFFS.open("/config.json", "r");
    if (configFile){
      Serial.println("opened config file");
      size_t size = configFile.size();
      // Allocate a buffer to store contents of the file.
      std::unique_ptr<char[]> buf(new char[size]);
  
      configFile.readBytes(buf.get(), size);
      DynamicJsonBuffer jsonBuffer;
      JsonObject& json = jsonBuffer.parseObject(buf.get());
      json.printTo(Serial);
      if (json.success()) {
        Serial.println("\nparsed json");
        strcpy(blynk_token, json["blynk_token"]);
      } 
      else{
        Serial.println("Failed to load json config");
      }
    }
  }
  } 
  else{
    Serial.println("Failed to mount FS");
  }
 
    tones(5,1000,300);
    //analogWrite(PWMPin, 1000);
    delay(500);
    //analogWrite(PWMPin, 0);
//  digitalWrite(ledPin, HIGH);
//  digitalWrite(ledPin, LOW);
  
  buttonState = digitalRead(buttonPin);
 
  //buttonState = 1;
  if (buttonState == 0){  
    //SPIFFS.format();
   
    //WiFi.disconnect(true);
    
    Serial.println("Enter WiFi config mode");
    ticker.attach(0.6, tick);       
    
    WiFiManagerParameter custom_blynk_token("blynk", "blynk token", blynk_token, 33);   // was 32 length ???
    WiFiManager wifiManager;

    
     
    //set minimu quality of signal so it ignores AP's under that quality
    //defaults to 8%
    //wifiManager.setMinimumSignalQuality();
  
    //set callback that gets called when connecting to previous WiFi fails, and enters Access Point mode
    wifiManager.setAPCallback(configModeCallback);
  
    //set config save notify callback
    wifiManager.setSaveConfigCallback(saveConfigCallback);
  
    wifiManager.addParameter(&custom_blynk_token);   //add all your parameters here
  
    //sets timeout until configuration portal gets turned off
    //useful to make it all retry or go to sleep, in seconds
    wifiManager.setTimeout(300);   // 5 minutes to enter data and then ESP resets to try again.
  
    //fetches ssid and pass and tries to connect
    //if it does not connect it starts an access point with the specified name
  
    //wifiManager.resetSettings();
   
    if (!wifiManager.autoConnect("LoF - tap to config")){
      ESP.restart();    
    }  
    ticker.detach();
    strcpy(blynk_token, custom_blynk_token.getValue());    //read updated parameters  
    if (shouldSaveConfig){      //save the custom parameters to FS
      Serial.println("saving config");
      DynamicJsonBuffer jsonBuffer;
      JsonObject& json = jsonBuffer.createObject();
      json["blynk_token"] = blynk_token;    
      
      File configFile = SPIFFS.open("/config.json", "w");
      if (!configFile) {
        Serial.println("Failed to open config file for writing");
      }  
      json.printTo(Serial);
      json.printTo(configFile);
      configFile.close();
      //end save
      
    delay(1000);
    Serial.println("Restart ESP to apply new WiFi settings..");
    ESP.restart();  
    }
  
}

  Serial.print("\n\rLet's start!");
  Serial.print("\n\rblynk token: " + String(blynk_token));
  Serial.print("\n\rSoftware version: " + String(SW_VERSION));

  analogWriteFreq(75000);  

}
void loop(){
  
  Serial.print("\n\rWiFi status: ");
  Serial.print(WiFi.SSID());
  Serial.print(" ");
  Serial.print(WiFi.status());

  if(!flashCorrectlyConfigured){
    Serial.print("\r\nflash incorrectly configured, SPIFFS cannot start, IDE size: " + ideSize + ", real size: " + realSize);
  }
  Serial.print("\r\nDo work");


  digitalWrite(PowerADCPin, HIGH);
  for (int i = 1; i < 4; i++){
    readADC_median2(i);
  }  

  // todo smth
  delay(4000);
  //digitalWrite(ledPin, HIGH);
  analogWrite(ledPin, 1000);
  delay(1000);  
  readDHT22();
  digitalWrite(C_DHTPin, LOW);
  digitalWrite(PowerADCPin, LOW);
  analogWrite(ledPin, 0);
  
  adcwater_int = adcwater;
  adclight_int = adclight;
  adcbattery_int = adcbattery;
  h_int = h;
  t_int = t;

  if (adcwater_int > 880) adcwater_int = 880;
  if (adcwater_int < 680) adcwater_int = 680;

  adcwater_int = (adcwater_int - 680) / 2;
 
  Serial.print("\r\nResults (W L T H B): ");
  Serial.print(adcwater_int);
  Serial.print(" ");
  Serial.print(adclight_int);
  Serial.print(" ");
  Serial.print(t);
  Serial.print(" ");
  Serial.print(h);
  Serial.print(" ");
  Serial.print(adcbattery_int);
  Serial.print(" adcwater: ");
  Serial.print(adcwater);

  Serial.print("\r\nWake up modem:");  
  WiFi.forceSleepWake();
  WiFi.mode(WIFI_STA);
  delay(100);

  if (WiFi.SSID()){
    WiFi.begin();
  }

  //  Serial.print("\n\rWiFi network: ");
  //  Serial.print(WiFi.SSID());
  //  Serial.print("\n\rWiFi status: ");
  //  Serial.print(WiFi.status());
  //  Serial.print("\r\nRSSI: ");
  //  Serial.print(WiFi.RSSI());  
  //  Serial.print("\n\rIP: ");
  //  Serial.print(WiFi.localIP());

  buttonState = digitalRead(buttonPin);
  
  if (buttonState == 0){
    Serial.print("\n\rReset WiFi settings");
    WiFiManager wifiManager;
    wifiManager.resetSettings();
  }
  digitalWrite(ledPin, LOW);
  int t_w;
  int t_b;

  int i = 100;
  while(WiFi.status() != 3 && i){
    Serial.print("-"); 
    delay(200);
    i--;
  }

  t_w = (100-i)/5;

  if (WiFi.status() == 3){  
    if (blynk_token[0] != '\0'){        
      //Blynk.config(blynk_token);
      //Blynk.config(blynk_token, "blynk-cloud.com", 8442);      
      //Blynk.config(blynk_token, "h.100010001.xyz", 8442);
      //Blynk.config(blynk_token, IPAddress(192,168,0,250), 8080);
      Blynk.config(blynk_token, "b.openwind.ru", 8080);
      Blynk.connect();        
    }
    Serial.print("\r\nConnecting blynk:");

    i = 100;
    while(!Blynk.connected() && i){
      Serial.print("."); 
      delay(200); 
      i--;
    }
    t_b = (100-i)/5;
    
    if (Blynk.connected()){

      hour_i = hour();
      minute_i = minute();
      second_i = second();
      day_i = day();
      month_i = month();
      
      hour_s = String(hour_i);
      minute_s = String(minute_i);
      second_s = String(second_i);
      day_s = String(day_i);
      month_s = String(month_i);
      
      if (hour_i < 10) hour_s = "0" + hour_s;
      if (minute_i < 10) minute_s = "0" + minute_s;
      if (second_i < 10) second_s = "0" + second_s;
      if (day_i < 10) day_s = "0" + day_s;
      if (month_i < 10) month_s = "0" + month_s;
      
      currentTime = hour_s + ":" + minute_s + ":" + second_s;
      currentDate = day_s + "/" + month_s + "/" + String(year());
      
      Blynk.virtualWrite(V1, adclight_int);
      Blynk.virtualWrite(V2, adcwater);
      
      if(DHTreadOK){
        Blynk.virtualWrite(V3, t_int);
        Blynk.virtualWrite(V4, h_int);
        led2.on();
        led2.setColor(BLYNK_GREEN);
      }
      else{
        led2.on();
        led2.setColor(BLYNK_RED);        
        }
      Blynk.virtualWrite(V5, adcbattery_int);            
      Blynk.virtualWrite(V7, t_w);
      Blynk.virtualWrite(V8, t_b);
      
      terminal.print("\r\nLast sync: " + currentDate + " " + currentTime);
      terminal.print("\r\nwifi blynk timeout (v7-8): " + String(t_w) + " " + String(t_b));
      terminal.print("\r\nIDE size: " + ideSize + ", real size: " + realSize);
      terminal.flush();

      Serial.print("\r\nLast sync:" + currentDate + " " + currentTime);
      Serial.print("\r\nSync blynk!");
    }
    else{
      Serial.print("\r\nblynk not connected.");
      sleep_time = 1800;
      
      sleepMemory.bShouldRepeat = true;
      ESP.rtcUserMemoryWrite((uint32_t)RTCMemOffset, (uint32_t*) &sleepMemory, sizeof(sleepMemory));
    }
  }
  else{
    Serial.print("\r\nWiFi not connected.");
    sleep_time = 1800;
    sleepMemory.bShouldRepeat = true;
    ESP.rtcUserMemoryWrite((uint32_t)RTCMemOffset, (uint32_t*) &sleepMemory, sizeof(sleepMemory));
    
    // ADD few retries and then reset WiFi sett. + EEPROM
  }

  Serial.print("\n\rWiFi status: ");
  Serial.print(WiFi.SSID());
  Serial.print(" ");
  Serial.print(WiFi.status());

  Serial.print("\r\nwifi blynk timeout: "); 
  Serial.print(t_w);
  Serial.print(" ");
  Serial.print(t_b);
  Serial.print("\r\nDeep sleep for: "); 
  Serial.print(sleep_time);
  Serial.print(" sec");
  sleep_time = sleep_time * 1000000;
  
  delay(10);

  //WAKE_RF_DISABLED to keep the WiFi radio disabled when we wake up
  ESP.deepSleep(sleep_time, WAKE_RF_DISABLED);
  
}
