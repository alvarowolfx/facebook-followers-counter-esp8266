#include <ESP8266WiFi.h>
#include <WiFiClientSecure.h>

// For storing configurations
#include <FS.h>
#include <ArduinoJson.h>

// WiFiManager Libraries
#include <DNSServer.h>            //Local DNS Server used for redirecting all rs to the configuration portal
#include <ESP8266WebServer.h>     //Local WebServer used to serve the configuration portal
#include <WiFiManager.h>          //https://github.com/tzapu/WiFiManager WiFi Configuration Magic

//7-Segment Display Output
#include <DigitLedDisplay.h>
#include <Ticker.h>

//User Config
#define LAMP_PIN D3 // Pin the LED is tied to
#define NUM_DIGITS 8 // number of digits you're using
#define BUZZER D8 // Pin that the buzzer is connected to
const int resetConfigPin = D0; //When high will reset the wifi manager config



char PAGE_ID[45] = "";
char ACCESS_TOKEN[200] = "";

// Facebook config.
#define FACEBOOK_HOST "graph.facebook.com"
#define FACEBOOK_PORT 443

#define LAMP_PIN D3
#define NUM_DIGITS 8
// Search every 10 seconds
#define REQUEST_INTERVAL 1000*10*1

Ticker tickerRequest;
bool requestNewState = true;
WiFiClientSecure tlsClient;

Ticker tickerDisplay;
DigitLedDisplay ld = DigitLedDisplay(D7, D5, D6); // DIN, LOAD, CLK

int pageFansCount = 0;
//int likecount = 0;
//int likecounttemp = 0;
// graph.facebook.com SHA1 fingerprint
const char* facebookGraphFingerPrint = "93 6F 91 2B AF AD 21 6F A5 15 25 6E 57 2C DC 35 A1 45 1A A5";

void request(){
  requestNewState = true;
}
void showNumber(int num){
  ld.printDigit(num);
}

void setupPins(){
  ld.setBright(15);  
  ld.setDigitLimit(8);
  ld.clear();
  ld.on();
  pinMode(BUZZER, OUTPUT); // Buzzer pin
  digitalWrite(BUZZER, LOW); // Set buzzer to Off
  pinMode(LAMP_PIN, OUTPUT);
  digitalWrite(LAMP_PIN, LOW);  
}
// flag for saving data
bool shouldSaveConfig = true;

//callback notifying us of the need to save config
void saveConfigCallback () {
  Serial.println("Should save config");
  shouldSaveConfig = true;
}

void setup() {

  Serial.begin(115200);

  if (!SPIFFS.begin()) {
    Serial.println("Failed to mount FS");
    return;
  }

 

  pinMode(LED_BUILTIN, OUTPUT);     // Initialize the LED_BUILTIN pin as an output
  digitalWrite(LED_BUILTIN, LOW);   // Turn the LED on (Note that LOW is the voltage level
  loadConfig();

  

  WiFiManager wifiManager;
  wifiManager.setSaveConfigCallback(saveConfigCallback);
  int tries = 0;
  // Adding an additional config on the WIFI manager webpage for the API Key and Channel ID
  WiFiManagerParameter custompageid("PAGE_ID", "Page ID", PAGE_ID, 50);
  WiFiManagerParameter customaccesstoken("ACCESS_TOKEN", "Access Token", ACCESS_TOKEN, 200);
  wifiManager.addParameter(&custompageid);
  wifiManager.addParameter(&customaccesstoken);

  // If it fails to connect it will create a FB-Counter access point with the password supersecret
  wifiManager.autoConnect("FB-Counter", "supersecret");

  strcpy(PAGE_ID, custompageid.getValue());
  strcpy(ACCESS_TOKEN, customaccesstoken.getValue());

  if (shouldSaveConfig) {
    saveConfig();
  }
  
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    delay(500);
    tries++;

    //showNumber(tries); 
    int mod = tries % 8;
    ld.write(tries % 8, B00011101);
    if(mod == 0){
      ld.clear();
    }
  }
  //lc.clearDisplay(0);
  ld.clear();
   Serial.println();
  Serial.print("connected: ");
  Serial.println(WiFi.localIP());
  {
  Serial.begin(9600);

  setupPins();
//  setupWifi();   
  
  // Registers ticker to pick up new data from time to time
  tickerRequest.attach_ms(REQUEST_INTERVAL, request);

  showNumber(pageFansCount);
  }

 // Force Config mode if there is no API key
if(strcmp(ACCESS_TOKEN, "") > 0) {
    Serial.println("Access Token OK");
    
  } else {
    Serial.println("Forcing Config Mode");
    ld.clear();
    ld.write(1,B01111110);
    forceConfigMode();
  }

  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  IPAddress ip = WiFi.localIP();
  Serial.println(ip);
  }
bool loadConfig() {
  File configFile = SPIFFS.open("/config.json", "r");
  if (!configFile) {
    Serial.println("Failed to open config file");
    return false;
  }

  size_t size = configFile.size();
  if (size > 1024) {
    Serial.println("Config file size is too large");
    return false;
  }

  // Allocate a buffer to store contents of the file.
  std::unique_ptr<char[]> buf(new char[size]);

  configFile.readBytes(buf.get(), size);

  StaticJsonBuffer<300> jsonBuffer;
  JsonObject& json = jsonBuffer.parseObject(buf.get());

  if (!json.success()) {
    Serial.println("Failed to parse config file");
    return false;
  }

  strcpy(ACCESS_TOKEN, json["ACCESS_TOKEN"]);
  strcpy(PAGE_ID, json["PAGE_ID"]);
  return true;
}

bool saveConfig() { 
  StaticJsonBuffer<300> jsonBuffer;
  JsonObject& json = jsonBuffer.createObject();
  json["PAGE_ID"] = PAGE_ID;
  json["ACCESS_TOKEN"] = ACCESS_TOKEN;

  File configFile = SPIFFS.open("/config.json", "w");
  if (!configFile) {
    Serial.println("Failed to open config file for writing");
    return false;
  }

  json.printTo(configFile);
  return true;
}

void forceConfigMode() {
  Serial.println("Reset");
  WiFi.disconnect();
  Serial.println("Dq");
  delay(500);
  ESP.restart();
  delay(5000);
}
String makeRequestGraph(){
  if (!tlsClient.connect(FACEBOOK_HOST, FACEBOOK_PORT)) {
    Serial.println("Host connection failed");
    return "";    
  }
  
  String params = "?pretty=0&fields=fan_count&access_token="+String(ACCESS_TOKEN);
  String path = "/v2.8/" + String(PAGE_ID);
  String url = path + params;
  Serial.print("requesting URL: ");
  Serial.println(url);

  String request = "GET " + url + " HTTP/1.1\r\n" +
    "Host: " + String(FACEBOOK_HOST) + "\r\n\r\n";
  
  tlsClient.print(request);

  String response = "";
  String chunk = "";  
  
  do {
    if (tlsClient.connected()) {
      delay(5);
      chunk = tlsClient.readStringUntil('\n');
      if(chunk.startsWith("{")){
        response += chunk;
      }
    }
  } while (chunk.length() > 0);
    
  Serial.print(" Message ");
  Serial.println(response);  

  return response;
}

int getPageFansCount(){
  String response = makeRequestGraph();  
  
  const size_t bufferSize = JSON_OBJECT_SIZE(2) + 40;
  DynamicJsonBuffer jsonBuffer(bufferSize);  
  
  JsonObject& root = jsonBuffer.parseObject(response);
  
  int fanCount = root["fan_count"];  
  return fanCount;
}


void loop() {

  if (digitalRead(resetConfigPin) == HIGH) {
    forceConfigMode();
  }

    // Just ask for new data when you pass the given time
  if(requestNewState){
    Serial.println("Request new State");               
       
    int pageFansCountTemp = getPageFansCount();
    
    if(pageFansCountTemp > pageFansCount){
     Serial.println("Count Increased");
     digitalWrite(D8,HIGH);
     delay(1000);
     digitalWrite(D8,LOW); 
  }
    Serial.print("Page fans count: ");
    Serial.println(pageFansCountTemp);
    
    if(pageFansCountTemp <= 0){
      
      Serial.println("Error requesting data");    
      
    }
    
    else{
      pageFansCount = pageFansCountTemp;
      requestNewState = false; 

      showNumber(pageFansCount);
    }
     
  
}
  delay(20);
}
