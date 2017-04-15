

#include <ESP8266WiFi.h>
#include <WiFiClientSecure.h>
#include <ArduinoJson.h>
#include <DigitLedDisplay.h>
#include <Ticker.h>

// Facebook config.
#define FACEBOOK_HOST "graph.facebook.com"
#define FACEBOOK_PORT 443
#define PAGE_ID "iotbootcamp"
#define ACCESS_TOKEN "ACCESS_TOKEN"

// graph.facebook.com SHA1 fingerprint
const char* facebookGraphFingerPrint = "93 6F 91 2B AF AD 21 6F A5 15 25 6E 57 2C DC 35 A1 45 1A A5";

// Configuração de Wifi
#define WIFI_SSID "ssid"
#define WIFI_PASSWORD "password"

#define LAMP_PIN D3
#define NUM_DIGITS 8
// Busque a cada 1 min
#define REQUEST_INTERVAL 1000*60*1

Ticker tickerRequest;
bool requestNewState = true;
WiFiClientSecure tlsClient;

Ticker tickerDisplay;
DigitLedDisplay ld = DigitLedDisplay(D7, D5, D6); // DIN, LOAD, CLK

int pageFansCount = 0;

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
  
  pinMode(LAMP_PIN, OUTPUT);
  digitalWrite(LAMP_PIN, LOW);  
}

void setupWifi(){
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.print("connecting");
  int tries = 0;
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

void setup() {
  Serial.begin(9600);

  setupPins();
  setupWifi();   
  
  // Registra o ticker para pegar dados novos de tempos em tempos
  tickerRequest.attach_ms(REQUEST_INTERVAL, request);

  showNumber(pageFansCount);
}

void loop() {

  // Apenas peça novos dados quando passar o tempo determinado
  if(requestNewState){
    Serial.println("Request new State");               
       
    int pageFansCountTemp = getPageFansCount();
    
    Serial.print("Page fans count: ");
    Serial.println(pageFansCountTemp);
    
    if(pageFansCountTemp <= 0){
      
      Serial.println("Error requesting data");    
      
    }else{
      pageFansCount = pageFansCountTemp;
      requestNewState = false; 

      showNumber(pageFansCount);
    }
  }
  
  delay(20);
}
