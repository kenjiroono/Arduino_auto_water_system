#include <Arduino.h>
#include <Hash.h>
#include <ESP8266WiFi.h>
#include <ESP8266WiFiMulti.h>
#include <WebSocketsServer.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>
#include <ArduinoJson.h>

//##################################################################
// Variables
// Count time for sensor updates
unsigned long myTime;

// Set up pins
const int PHOTO_PIN = A0;
int photo_val = 0;
int photo_thres = 500;

// Set up Fan pin and conditions
const int FAN_PIN = D0;
bool manualFan = false;
bool fanON = false;

// Set up LED pin and conditions
const int LED_PIN = D1;
bool manualLED = false;
bool ledON = false;

//##################################################################
// PainlessMesh
#include "painlessMesh.h"
#define   MESH_PREFIX     "SmartPlant"
#define   MESH_PASSWORD   "12345678"
#define   MESH_PORT       5555

Scheduler userScheduler; // to control your personal task
painlessMesh  mesh;

void sendMessage();

Task taskSendMessage( TASK_SECOND * 1 , TASK_FOREVER, &sendMessage );

void sendMessage() {
  String msg = "Hello from node (Light Control), ";
  msg += mesh.getNodeId();
  mesh.sendBroadcast( msg );
  taskSendMessage.setInterval( random( TASK_SECOND * 1, TASK_SECOND * 5 ));
}

// Needed for painless library
void receivedCallback( uint32_t from, String &msg ) {
  Serial.printf("startHere: Received from %u msg=%s\n", from, msg.c_str());
}

void newConnectionCallback(uint32_t nodeId) {
  Serial.printf("--> startHere: New Connection, nodeId = %u\n", nodeId);
}

void changedConnectionCallback() {
  Serial.printf("Changed connections\n");
}

void nodeTimeAdjustedCallback(int32_t offset) {
  Serial.printf("Adjusted time %u. Offset = %d\n", mesh.getNodeTime(),offset);
}

//##################################################################
// Web server set up
#define USE_SERIAL Serial

// Define WiFi set up
const char* ssid = "IODATA-b1b878-2G";
const char* password = "1222575916190";

// Run server on port 80 and web socket on port 81
ESP8266WiFiMulti WiFiMulti;
ESP8266WebServer server(80);
WebSocketsServer webSocket = WebSocketsServer(81);

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
      
      // LED control
      if (payload[0] == '#') {
        if (manualLED == true){
          if (ledON) {
            ledON = false;
          } else {
            ledON = true;
          }
          manualLED = false;
        }
        else {
          if (ledON) {
            ledON = false;
          } else {
            ledON = true;
          }
          manualLED = true;
        }
        Serial.println(String(payload[0]));
        webSocket.sendTXT(num, "light turned on!");
      } 

      // Fan control
      else if (payload[0] == '$') {
        if (fanON == true){
          fanON = false;   
          manualFan = false;
        }
        else {
          fanON = true;
          manualFan = true;
        }
        Serial.println(String(payload[0]));
        webSocket.sendTXT(num, "Fan turned on!");
      } 
      
      else {
        photo_thres = atoi((const char *)payload) * 10;
        String msg = "photo_thres:" + String(photo_thres);
        webSocket.sendTXT(num, msg);
      }      
      break;
  }
}

//##################################################################


void setup() {
  //##################################################################
  // Pin IO setup
  pinMode(FAN_PIN, OUTPUT);
  pinMode(LED_PIN, OUTPUT);
  
  //##################################################################
  // Websocket / server
  USE_SERIAL.begin(115200);

  USE_SERIAL.println();
  USE_SERIAL.println();
  USE_SERIAL.println();

  for(uint8_t t = 4; t > 0; t--) {
      USE_SERIAL.printf("[SETUP] BOOT WAIT %d...\n", t);
      USE_SERIAL.flush();
      delay(1000);
  }
  
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
  
  //##################################################################
  // PainlessMesh
//  mesh.setDebugMsgTypes( ERROR | STARTUP );  // set before init() so that you can see startup messages
//  mesh.init( MESH_PREFIX, MESH_PASSWORD, &userScheduler, MESH_PORT );
//  mesh.onReceive(&receivedCallback);
//  mesh.onNewConnection(&newConnectionCallback);
//  mesh.onChangedConnections(&changedConnectionCallback);
//  mesh.onNodeTimeAdjusted(&nodeTimeAdjustedCallback);
//
//  userScheduler.addTask( taskSendMessage );
//  taskSendMessage.enable();
  
  //##################################################################
}


void autoLedControl(int val, int thres) {
  // turn on the LED depending on the photoresister value
  if (manualLED == false){  
    if (val < thres) {
      ledON = true;
    } else {
      ledON = false;
    }
  }
}


void ledControl() {
  if (ledON) {
    digitalWrite(LED_PIN, HIGH);
  } else {
    digitalWrite(LED_PIN, LOW);
  }
}


void autoFanControl(int val, int thres) {
  // turn on the LED depending on the photoresister value
  if (manualFan == false){
    if (val < thres) {
      digitalWrite(FAN_PIN, HIGH);
    } 
    else {
      digitalWrite(FAN_PIN, LOW);
    }
  }
}


void manualFanControl() {
  if (manualFan == true) {
    if (fanON == true) {
      digitalWrite(FAN_PIN, HIGH);
      Serial.println("Fan: ON ");
    }
  }
  else {
    digitalWrite(FAN_PIN, LOW);
  }
} 



// Utility function to serve the home page dashboard
void get_index() {
  //Print a welcoming message on the index page
  String html ="<!DOCTYPE html> <html> <head> <meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0\"> <script src=\"https://code.jquery.com/jquery-1.6.4.js\"></script> <title>Water control - dashboard</title> <style> html, body { margin: 100; padding: 100; font-family: \"Roboto\", sans-serif; background-color: #ffffff; } h1 { text-align: center; padding: 30px; font-size: 60px; background-color: #4bb39a; color: #ffffff; } .controlContainer { display:inline-flex; justify-content: center; text-align: center; text-align: center; width: 100%; } .btnContainer { display: inline-flex; width: 50%; border: 5px solid #ffffff; font-size: 30px; font-weight: bold; background-color: #4bb39a; color: #ffffff; padding: 20px; } .btns { width: 100%; } button { text-align: center; background-color: #ffffff; color: #4bb39a; width: 140px; padding: 25px 25px; text-align: center; font-size: 30px; border: 3px outset; opacity: 0.9; font-weight: bold; } button:hover { opacity: 1; } .dot { height: 30px; width: 30px; background-color: #d3d3d3; border-radius: 50%; display:inline-block; } .dot:hover { background-color: red; } .container { align-items: center; justify-content: center; display:flex; border: 1px #4bb39a; background-color: #ffffff; padding: 30px; } .gauge { text-align: center; min-width: 30%; padding: 5%; font-family: \"Roboto\", sans-serif; font-size: 30px; font-weight: bold; color: #4bb39a; } .gauge__body { width: 100%; height: 0; padding-bottom: 50%; background: #b4c0be; position: relative; border-top-left-radius: 100% 200%; border-top-right-radius: 100% 200%; overflow: hidden; } .gauge__fill { position: absolute; top: 100%; left: 0; width: inherit; height: 100%; background: #4bb39a; transform-origin: center top; transform: rotate(0.25turn); transition: transform 2s ease-out; } .gauge__cover { width: 75%; height: 150%; background: #ffffff; border-radius: 50%; position: absolute; top: 25%; left: 50%; transform: translateX(-50%); /* Text */ display: flex; align-items: center; justify-content: center; padding-bottom: 25%; padding-left: 20%; padding-right: 20%; box-sizing: border-box; } .slidecontainer { text-align: center; font-weight: bold; color: #ffffff; width: 50%; border: 5px solid #ffffff; padding: 20px; font-size: 30px; background-color: #4bb39a; } .slider { -webkit-appearance: none; width: 80%; height: 10px; background: #d3d3d3; outline: none; opacity: 0.7; -webkit-transition: .2s; transition: opacity .2s; } .slider:hover { opacity: 1; } .slider::-webkit-slider-thumb { -webkit-appearance: none; appearance: none; width: 15px; height: 30px; border-radius: 20%; background: #ffffff; cursor: pointer; } .slider::-moz-range-thumb { width: 20px; height: 30px; border-radius: 0.1%; background: #ffffff; cursor: pointer; } #thres_container { font-size: 60px; } </style> </head> <body> <h1>Light Source Control Dashboard</h1> <div class=\"container\" > <div id=\"lightGuage\" class=\"gauge\"> <p>Light</p> <div class=\"gauge__body\"> <div class=\"gauge__fill\"></div> <div class=\"gauge__cover\"></div> </div> </div> </div> <div class=\"controlContainer\"> <div class=\"slidecontainer\"> <p>Set light source threshold</p> <p id=\"thres_container\"><span id=\"thres_val\"></span>%</p> <input type=\"range\" min=\"1\" max=\"100\" value=\"50\" class=\"slider\" id=\"myRange\"> </div> <div class=\"btnContainer\"> <dev class=\"btns\"> <p>LED</p> <button id=\"ledBtn\" onclick=\"ledOn()\">LED ON</button> <br><br> <div id=\"ledDot\" class=\"dot\"></div> </dev> <dev class=\"btns\"> <p>Fan</p> <button id=\"fanBtn\" onclick=\"fanOn()\">FAN ON</button> <br><br> <div id=\"fanDot\" class=\"dot\"></div> </dev> </div> </div> <script> var connection = new WebSocket('ws://'+location.hostname+':81/', ['arduino']); connection.onopen = function () { connection.send('Connect ' + new Date()); }; connection.onerror = function (error) { console.log('WebSocket Error ', error);}; connection.onmessage = function (e) { console.log('Server: ', e.data);}; const lightGauge = document.getElementById(\"lightGuage\"); function setGaugeValue(gauge, value, unit) { let val = value; if (value < 0) { val = 0;}; if (value > 1) { val = 1;}; gauge.querySelector(\".gauge__fill\").style.transform = `rotate(${ val / 2 }turn)`; gauge.querySelector(\".gauge__cover\").textContent = `${Math.round( val * 100 )}${unit}`; } function changeDotColor(id, ledON) { var ledDot = document.getElementById(id); if (ledON) { ledDot.style.backgroundColor = 'salmon'; } else { ledDot.style.backgroundColor = '#d3d3d3'; } } setInterval(function() { $.getJSON('/json', function(data) { let photoVal = data.photoVal; let ledON = data.ledON; let fanON = data.fanON; changeDotColor('ledDot', ledON); changeDotColor('fanDot', fanON); setGaugeValue(lightGauge, photoVal/1024, '%'); }); }, 1000); function ledOn() { connection.send(\"#\"); }; function fanOn() { connection.send(\"$\"); }; ledBtn.addEventListener('click', function onClick() { ledBtn.style.backgroundColor = 'salmon'; ledBtn.style.color = 'white'; ledBtn.style.border = '3px inset'; setTimeout(function(){ ledBtn.style.backgroundColor = '#ffffff'; ledBtn.style.color = '#4bb39a'; ledBtn.style.border = '3px outset'; }, 1000); }); const fanBtn = document.getElementById('fanBtn'); fanBtn.addEventListener('click', function onClick() { fanBtn.style.backgroundColor = 'salmon'; fanBtn.style.color = 'white'; fanBtn.style.border = '3px inset'; setTimeout(function(){ fanBtn.style.backgroundColor = '#ffffff'; fanBtn.style.color = '#4bb39a'; fanBtn.style.border = '3px outset'; }, 1000); }); var slider = document.getElementById(\"myRange\"); var output = document.getElementById(\"thres_val\"); output.innerHTML = slider.value; slider.oninput = function() { output.innerHTML = this.value; connection.send(slider.value); } </script> </body> </html>";
  
  server.send(200, "text/html", html);
}


void get_json() {
  StaticJsonDocument<200> doc;
  doc["Content-type"] = "application/json";
  doc["Status"] = 200;
  doc["photoVal"] = photo_val;
  doc["fanON"] = fanON;
  doc["manualFan"] = manualFan;
  doc["ledON"] = ledON;
  doc["manualLED"] = manualLED;
  String jsonStr;
  serializeJsonPretty(doc, jsonStr);
  server.send(200, "application/json", jsonStr);
}


void loop() {
  webSocket.loop();
  server.handleClient();
//  mesh.update();

  // Update sensor values evcery second 
  myTime = millis();
  if (myTime % 1000 == 0) {
    photo_val = analogRead(PHOTO_PIN);
    autoLedControl(photo_val, photo_thres);
    ledControl();
    manualFanControl();

    Serial.print("LED ON value: ");
    Serial.println(ledON);
  }
}
