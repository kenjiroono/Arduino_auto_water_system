#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <ESP8266WiFiMulti.h>
#include <WebSocketsServer.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>
#include <Hash.h>
#include <ArduinoJson.h>
#include <DHT.h>


const char* ssid = "IODATA-b1b878-2G";
const char* password = "1222575916190";


#define USE_SERIAL Serial


// Run server on port 80 and web socket on port 81
ESP8266WiFiMulti WiFiMulti;
ESP8266WebServer server(80);
WebSocketsServer webSocket = WebSocketsServer(81);


// Count time for sensor updates
unsigned long myTime;

// DHT 11 set up
const int DHT_PIN = D1;
DHT dht(DHT_PIN, DHT11);
int temperature = 0;
int humidity = 0;


// Soil Moisure senor set up
const int SOIL_PIN = A0;
float soil_moisture = 0;


// Water pump set up
const int PUMP_PIN = D3;
int water_thres = 50;


// Web Socket
void webSocketEvent(uint8_t num, WStype_t type, uint8_t * payload, size_t length) {

  switch(type) {
    case WStype_DISCONNECTED:
      USE_SERIAL.printf("[%u] Disconnected!\n", num);
      break;
        
    case WStype_CONNECTED:{
      IPAddress ip = webSocket.remoteIP(num);
      USE_SERIAL.printf("[%u] Connected from %d.%d.%d.%d url: %s\n", num, ip[0], ip[1], ip[2], ip[3], payload);
      // send message to client
      webSocket.sendTXT(num, "Connected");
    }
      break;
   
    case WStype_TEXT:
      USE_SERIAL.printf("[%u] get Text: %s\n", num, payload);

      if (payload[0] == '#') {
        readtempHum();
        readSoilMoist();
        forceSwitchPump();
        webSocket.sendTXT(num, "pump turned on!");
      } 
      
      else {
        water_thres = atoi((const char *)payload);
        readtempHum();
        readSoilMoist();
        String msg = "water_thres:" + String(water_thres);
        webSocket.sendTXT(num, msg);
      }     
      break;
  }
}



void setup() {

  // web server_____________________________________________
  USE_SERIAL.begin(115200);

  USE_SERIAL.println();
  USE_SERIAL.println();
  USE_SERIAL.println();

  for(uint8_t t = 4; t > 0; t--) {
      USE_SERIAL.printf("[SETUP] BOOT WAIT %d...\n", t);
      USE_SERIAL.flush();
      delay(1000);
  }
  
  dht.begin();

  pinMode(SOIL_PIN, INPUT);
  pinMode(PUMP_PIN, OUTPUT);
  digitalWrite(PUMP_PIN, HIGH);

  if(WiFi.getMode() & WIFI_AP) {
        WiFi.softAPdisconnect(true);
  }
  
  WiFiMulti.addAP(ssid, password);

  while(WiFiMulti.run() != WL_CONNECTED) {
      delay(100);
  }

  // start webSocket server
  webSocket.begin();
  webSocket.onEvent(webSocketEvent);

  if(MDNS.begin("esp8266")) {
      USE_SERIAL.println("MDNS responder started");
      USE_SERIAL.print("IP address: ");
      USE_SERIAL.println(WiFi.localIP());
  }

  server.on("/", get_index);
  server.on("/json", get_json);
  server.begin();

  // Add service to MDNS
  MDNS.addService("http", "tcp", 80);
  MDNS.addService("ws", "tcp", 81);
}


void readtempHum(){
  // Read temperature and humidity level
  temperature = dht.readTemperature();
  humidity = dht.readHumidity();
}


void readSoilMoist(){
  // Read soil moisture level and linearize the value
  soil_moisture = analogRead(SOIL_PIN); 
  soil_moisture = (soil_moisture - 862) * (-1); // 0 ~ 456

  // 0%(0) ~ 25%(350)
  if (soil_moisture < 350) {
    soil_moisture = soil_moisture / 350 * 100 * 0.25;
  } 
  // 25%(350) ~ 50%(410)
  else if (soil_moisture >= 350 && soil_moisture < 410){
    soil_moisture = (((soil_moisture - 350) * 25) / 60) + 25;
  } 
  // 50%(410) ~ 75%(435) 
  else if (soil_moisture >= 410 && soil_moisture < 435){
    soil_moisture = (soil_moisture - 410) + 50;
  } 
  // 75%(435) ~ 100%(456)
  else { 
    soil_moisture = (((soil_moisture - 435) * 25) / 21) + 75;
  }  
}


void switchPump(float soil_moisture, int thres) {
  // Turn water pump on if moisture level is below the threshold

  if (soil_moisture < thres){
    
      digitalWrite(PUMP_PIN, LOW);
      delay(200);
  }  
  digitalWrite(PUMP_PIN, HIGH);
}


void forceSwitchPump() {
  // turn on pump from the dashboard
  digitalWrite(PUMP_PIN, LOW);
  delay(200);
  digitalWrite(PUMP_PIN, HIGH); 
}


// Utility function to serve the home page dashboard
void get_index() {
  
  //Print a welcoming message on the index page
  String html ="<!DOCTYPE html> <html> <head> <meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0\"> <script src=\"https://code.jquery.com/jquery-1.6.4.js\"></script> <title>Water control - dashboard</title> <style> html, body { margin: 100; padding: 100; font-family: \"Roboto\", sans-serif; background-color: #ffffff; } h1 { text-align: center; padding: 30px; font-size: 60px; background-color: #127BDE; color: #ffffff; } .controlContainer { display: flex; } .btnContainer { text-align: center; width: 50%; border: 5px solid #ffffff; font-size: 30px; font-weight: bold; background-color: #127BDE; color: #ffffff; padding: 20px; } button { background-color: #ffffff; color: #127BDE; width: 140px; padding: 25px 25px; text-align: center; text-decoration: none; display: inline-block; font-size: 30px; border: 3px outset; opacity: 0.9; font-weight: bold; } button:hover { opacity: 1; } .container { display:flex; border: 1px #127BDE; background-color: #ffffff; padding: 30px; } .gauge { text-align: center; width: 100%; padding: 5%; font-family: \"Roboto\", sans-serif; font-size: 30px; font-weight: bold; color: #127BDE; } .gauge__body { width: 100%; height: 0; padding-bottom: 50%; background: #b4c0be; position: relative; border-top-left-radius: 100% 200%; border-top-right-radius: 100% 200%; overflow: hidden; } .gauge__fill { position: absolute; top: 100%; left: 0; width: inherit; height: 100%; background: #127BDE; transform-origin: center top; transform: rotate(0.25turn); transition: transform 2s ease-out; } .gauge__cover { width: 75%; height: 150%; background: #ffffff; border-radius: 50%; position: absolute; top: 25%; left: 50%; transform: translateX(-50%); /* Text */ display: flex; align-items: center; justify-content: center; padding-bottom: 25%; padding-left: 20%; padding-right: 20%; box-sizing: border-box; } .slidecontainer { text-align: center; font-weight: bold; color: #ffffff; width: 45%; border: 5px solid #ffffff; padding: 20px; font-size: 30px; background-color: #127BDE; } .slider { -webkit-appearance: none; width: 80%; height: 10px; background: #d3d3d3; outline: none; opacity: 0.7; -webkit-transition: .2s; transition: opacity .2s; } .slider:hover { opacity: 1; } .slider::-webkit-slider-thumb { -webkit-appearance: none; appearance: none; width: 15px; height: 30px; border-radius: 20%; background: #ffffff; cursor: pointer; } .slider::-moz-range-thumb { width: 20px; height: 30px; border-radius: 0.1%; background: #ffffff; cursor: pointer; } #thres_container { font-size: 60px; } </style> </head> <body> <h1>Irrigation Control Dashboard</h1> <div class=\"container\" > <div id=\"tempGauge\" class=\"gauge\"> <p>Temperature</p> <div class=\"gauge__body\"> <div class=\"gauge__fill\"></div> <div class=\"gauge__cover\"></div> </div> </div> <div id = \"humidGauge\" class=\"gauge\"> <p>Humidity</p> <div class=\"gauge__body\"> <div class=\"gauge__fill\"></div> <div class=\"gauge__cover\"></div> </div> </div> <div id = \"soilGauge\" class=\"gauge\"> <p>Soil Moisture</p> <div class=\"gauge__body\"> <div class=\"gauge__fill\"></div> <div class=\"gauge__cover\"></div> </div> </div> </div> <div class=\"controlContainer\"> <div class=\"slidecontainer\"> <p>Set irrigation threshold</p> <p id=\"thres_container\"><span id=\"thres_val\"></span>%</p> <input type=\"range\" min=\"1\" max=\"100\" value=\"50\" class=\"slider\" id=\"myRange\"> </div> <div class=\"btnContainer\"> <p>Turn ON water pump</p> <br> <button id=\"btn\" onclick=\"pumpOn()\">Pump on</button> </div> </div> <script> var connection = new WebSocket('ws://'+location.hostname+':81/', ['arduino']); connection.onopen = function () { connection.send('Connect ' + new Date()); }; connection.onerror = function (error) { console.log('WebSocket Error ', error);}; connection.onmessage = function (e) { console.log('Server: ', e.data);}; setInterval(function() { $.getJSON('/json', function(data) { let tempVal = data.temperatureVal; let humidVal = data.humidityVal; let moistVal = data.soilMoistureVal; setGaugeValue(tempGauge, tempVal/100, 'C'); setGaugeValue(humidGauge, humidVal/100, '%'); setGaugeValue(soilGauge, moistVal/100, '%'); }); }, 1000); const tempGauge = document.getElementById(\"tempGauge\"); const humidGauge = document.getElementById(\"humidGauge\"); const soilGauge = document.getElementById(\"soilGauge\"); function setGaugeValue(gauge, value, unit) { let val = value; if (value < 0) { val = 0;}; if (value > 1) { val = 1;}; gauge.querySelector(\".gauge__fill\").style.transform = `rotate(${ val / 2 }turn)`; gauge.querySelector(\".gauge__cover\").textContent = `${Math.round( val * 100 )}${unit}`; } const btn = document.getElementById('btn'); btn.addEventListener('click', function onClick() { btn.style.backgroundColor = 'salmon'; btn.style.color = 'white'; btn.style.border = '3px inset'; pumpOn(); setTimeout(function(){ btn.style.backgroundColor = '#ffffff'; btn.style.color = '#127BDE'; btn.style.border = '3px outset'; }, 1000); }); function pumpOn() { connection.send(\"#\"); }; var slider = document.getElementById(\"myRange\"); var output = document.getElementById(\"thres_val\"); output.innerHTML = slider.value; slider.oninput = function() { output.innerHTML = this.value; connection.send(slider.value); } </script> </body> </html>";  
 
  server.send(200, "text/html", html);
}


void get_json() {

  StaticJsonDocument<200> doc;
  doc["Content-type"] = "application/json";
  doc["Status"] = 200;
  doc["temperatureVal"] = temperature;
  doc["humidityVal"] = humidity;
  doc["soilMoistureVal"] = (int)soil_moisture;
  String jsonStr;
  serializeJsonPretty(doc, jsonStr);
  server.send(200, "application/json", jsonStr);
  
}


void loop() {
  
  webSocket.loop();
  server.handleClient();

  // Update sensor values evcery 10secs 
  myTime = millis();
  if (myTime % 1000 == 0) {
      
    USE_SERIAL.println(myTime);
    readSoilMoist();
    readtempHum();
    switchPump(soil_moisture, water_thres);
    Serial.print("Temp: ");
    Serial.print(temperature);
    Serial.print("  Humid: ");
    Serial.print(humidity);
    Serial.print("  soil moist: ");
    Serial.print(soil_moisture);
    Serial.print("  water thres: ");
    Serial.print(water_thres);
  }
                
}
