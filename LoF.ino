#include <FS.h>
#include "version.h"

//blynk
#include <ESP8266WiFi.h>
#include <BlynkSimpleEsp8266.h>

//RTC Time
//#include <TimeLib.h>
//#include <WidgetRTC.h>

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

//WidgetRTC rtc;

int buzPin = 5;
int ledPin = 15;

int adcPin = A0;
int PadcPin = 4;
int PWMPin = 15;
//int APin = 15;
int BPin = 14;
int CPin = 13;

int buttonPin = 0;
int buttonState = 1;

float adclight;
float adcwater;
float adcbattery;

float q_w;
float q_l;

char blynk_token[34];

bool DHTreadOK = false; //false if not read

float h;
float t;
float f;
float old_h = 0;
float old_t = 0;
float old_f = 0;

bool shouldSaveConfig = false; //flag for saving data

//int days, hours, minutes, seconds;
//String currentTime;
//String currentDate;

//BLYNK_CONNECTED(){
//rtc.begin();
//}

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

void readADC(int input){
  switch(input){
    case 1 :
      digitalWrite(BPin, LOW);
      digitalWrite(CPin, LOW);
      analogWrite(PWMPin, 412);
      delay(50);
      adcwater = analogRead(adcPin);
      analogWrite(PWMPin, 0);
      break;
    case 2 :
      digitalWrite(BPin, HIGH);
      digitalWrite(CPin, LOW);
      delay(50);
      adcbattery = analogRead(adcPin) * 4;
      digitalWrite(BPin, LOW);
      break;
    case 3 :
      digitalWrite(BPin, LOW);
      digitalWrite(CPin, HIGH);
      delay(50);
      adclight = analogRead(adcPin);
      //digitalWrite(CPin, LOW);
      break;
    default :
      delay(1);
   } 
  
  
}


void readADC_norm(int input){
  switch(input){
    case 1 :
      digitalWrite(BPin, HIGH);
      digitalWrite(CPin, LOW);
      delay(50);
      adcbattery = analogRead(adcPin) * 4;      
      q_w = (adcbattery * 4) / 15;
      q_l = (adcbattery * 25) / 101;
   
      digitalWrite(BPin, LOW);
      break;
    case 2 :
      digitalWrite(BPin, LOW);
      digitalWrite(CPin, LOW);
      analogWrite(PWMPin, 412);
      delay(50);
      adcwater = analogRead(adcPin);
      adcwater = 5*(100 - 100*(adcwater / q_w));
      if (adcwater > 100) adcwater = 100;
      if (adcwater < 0) adcwater = 0;
      analogWrite(PWMPin, 0);
      break;      
    case 3 :
      digitalWrite(BPin, LOW);
      digitalWrite(CPin, HIGH);
      delay(50);
      //adclight = analogRead(adcPin);
      adclight = 100*(analogRead(adcPin) / q_l);
      if (adclight > 100) adclight = 100;
      if (adclight < 0) adclight = 0;
      //digitalWrite(CPin, LOW);
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

       if (!isnan(h)){
          old_h = h;}
       if (!isnan(h)){
          old_t = t;}
       if (!isnan(h)){
          old_f = f;}
        
      
    }
  }
  if (DHTreadOK){ 
    led2.on();
    led2.setColor(BLYNK_GREEN);
    Serial.print(" ok");
  }
  else{
    led2.on();
    led2.setColor(BLYNK_RED);
    Serial.print(" failed");
  }
  
}

void tone(uint8_t _pin, unsigned int frequency, unsigned long duration){
  
  pinMode (_pin, OUTPUT);
  analogWriteFreq(frequency);
  analogWrite(_pin,500);
  delay(duration);
  analogWrite(_pin,0);
  
}

void setup(){

  //WiFi.disconnect();
  WiFi.mode(WIFI_OFF);
  //WiFi.setSleepMode(WIFI_LIGHT_SLEEP);
  WiFi.forceSleepBegin();
  delay(1);
  dht.begin();
  Serial.begin(9600);

  pinMode(PadcPin, OUTPUT);
  pinMode(PWMPin, OUTPUT);

  pinMode(BPin, OUTPUT);
  pinMode(CPin, OUTPUT);
  digitalWrite(BPin, LOW);
  digitalWrite(CPin, LOW); 

  pinMode(ledPin, OUTPUT);
  pinMode(buzPin, OUTPUT);
  pinMode(buttonPin, INPUT);
  
  pinMode(adcPin, INPUT);

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
 
  tone(5,1000,300);
  digitalWrite(ledPin, HIGH);
  delay(400);
  digitalWrite(ledPin, LOW);
  
  delay(300);
  buttonState = digitalRead(buttonPin);
 
//buttonState = 1;
  if (buttonState == 0){   
  
  //SPIFFS.format();
    
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

  Serial.print("\r\nStart!");

  //WiFi.mode(WIFI_OFF);
  //WiFi.forceSleepBegin();
  //wifi_set_sleep_type(MODEM_SLEEP_T);

  Serial.print("\n\rblynk token: ");
  Serial.print(blynk_token);
  Serial.print("\n\rSoftware version: ");
  Serial.print(SW_VERSION);

  analogWriteFreq(75000);

  

}
void loop(){
  
  //  Serial.print("\r\nModem sleep");
  //  WiFi.disconnect();
  //  WiFi.forceSleepBegin();
  //  delay(10000);

  Serial.print("\n\rWiFi status: ");
  Serial.print(WiFi.SSID());
  Serial.print(" ");
  Serial.print(WiFi.status());

 
  Serial.print("\r\nDo work");  
  
  digitalWrite(PadcPin, HIGH);
  
  for (int i = 1; i < 4; i++){  
    readADC_norm(i);
  }
  
  delay(5000);
  Serial.print("\r\nreadDHT22");
  readDHT22();
  digitalWrite(CPin, LOW);
  digitalWrite(PadcPin, LOW);

  //analogWrite(PadcPin, 0);

  int adcwater_int;
  int adclight_int;
  int adcbattery_int;
  int h_int;
  int t_int;
  
  adcwater_int = adcwater;
  adclight_int = adclight;
  adcbattery_int = adcbattery;
  h_int = h;
  t_int = t;
 
  Serial.print("\r\nADC (W L T H B): ");
  Serial.print(adcwater);
  Serial.print(" ");
  Serial.print(adclight);
  Serial.print(" ");
  Serial.print(t);
  Serial.print(" ");
  Serial.print(h);
  Serial.print(" ");
  Serial.print(adcbattery);

  Serial.print("\r\nWake up modem");  
  WiFi.forceSleepWake();

   //WiFi.begin();
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
    Serial.print("\n\rresetSettings");
    WiFiManager wifiManager;
    wifiManager.resetSettings();
  }

  while(WiFi.status() != 3){
  Serial.print("-"); 
  delay(100);  
  }
  if (blynk_token[0] != '\0'){        
    Blynk.config(blynk_token);
    Blynk.connect();
    //setSyncInterval(10*60); // interval of RTC sync
  }
  delay(100);
  //while(!Blynk.connected()){
  //Serial.print("."); 
  //delay(500); 
  //}
  if (Blynk.connected()){
    Blynk.virtualWrite(V1, adclight_int);
    Blynk.virtualWrite(V2, adcwater_int);
    Blynk.virtualWrite(V3, t_int);
    Blynk.virtualWrite(V4, h_int);
    Blynk.virtualWrite(V5, adcbattery_int);
    Serial.print("\r\nSync blynk");
  }
  Serial.print("\n\rWiFi status: ");
  Serial.print(WiFi.SSID());
  Serial.print(" ");
  Serial.print(WiFi.status());
  
  //  Serial.print("\r\nModem sleep");
  //  WiFi.disconnect();
  //  WiFi.forceSleepBegin();
  //  delay(1);

  Serial.print("\r\nDeep sleep");
  Serial.print("\n\r");

  //WiFi.disconnect(true);
  delay(1);

  //WAKE_RF_DISABLED to keep the WiFi radio disabled when we wake up
  //ESP.deepSleep( SLEEPTIME, WAKE_RF_DISABLED );
  
  //ESP.deepSleep(59000000, WAKE_RF_DISABLED); // once a minute
  ESP.deepSleep(3540000000, WAKE_RF_DISABLED); // once a hour
  
}
