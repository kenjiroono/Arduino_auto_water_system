//************************************************************
// Light source control with PainlessMesh networking and Asyncronous server.
// 
// Sensors & Actuators:
//    - Photoresister: reads luminosity value.
//    - LED Light
//    - NPN Transistor: used to switch electric current to DC motor
//    - DC motor: Fan is attached.
// 
// If the luminosity value is below the specified threshold, the LED is turned on. 
// The threshold value can be adjusted by the slider in the dashboard.
// The node recieves the temperature value via mesh network and fan is turned on 
// if the thempretature is above the set theshold value. It also can be adjusted by 
// the slider in the dashboard   
//************************************************************

#include "IPAddress.h"
#include "painlessMesh.h"
#include <ArduinoJson.h>

#ifdef ESP8266
#include "Hash.h"
#include <ESPAsyncTCP.h>
#else
#include <AsyncTCP.h>
#endif
#include <ESPAsyncWebServer.h>

#define   MESH_PREFIX     "whateverYouLike"
#define   MESH_PASSWORD   "somethingSneaky"
#define   MESH_PORT       5555

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
int temp_thres = 10;
int temp_val = 0;

// Set up LED pin and conditions
const int LED_PIN = D1;
bool manualLED = false;
bool ledON = false;

// Set up slider value
String sliderValue = "0";
const char* PARAM_INPUT = "value";
//##################################################################
// PainlessMesh / AsyncWebServer
void receivedCallback( const uint32_t &from, const String &msg );
IPAddress getlocalIP();

painlessMesh  mesh;
Scheduler userScheduler; // to control your personal task

void sendMessage() ; // Prototype so PlatformIO doesn't complain

Task taskSendMessage( TASK_SECOND * 1 , TASK_FOREVER, &sendMessage );

void sendMessage() {
  String myAPIP = IPAddress(mesh.getAPIP()).toString();
  String myIP = IPAddress(mesh.getStationIP()).toString();
  
  StaticJsonDocument<200> doc;
  doc["nodeName"] = "LightControl";
  doc["tempThreshold"] = temp_thres;
  doc["myAPIP"] = myAPIP;
  doc["myIP"] = myIP;
  String msg; 
  serializeJson(doc, msg);
  mesh.sendBroadcast( msg );
  Serial.println(">>>: " + msg);
  taskSendMessage.setInterval( random( TASK_SECOND * 10, TASK_SECOND * 5 ));
}

void receivedCallback( auto from, auto &msg ) {
  StaticJsonDocument<200> doc;
  DeserializationError err = deserializeJson(doc, msg);

  if (err) {
    Serial.print("ERROR: ");
    Serial.println(err.c_str());
    return;
  }
  
  int temperature = doc["temperatureVal"];
  temp_val = temperature;
  Serial.println("<<<: " + msg);
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

AsyncWebServer server(80);
IPAddress myIP(0,0,0,0);
IPAddress myAPIP(0,0,0,0);

//##################################################################
// HTML
const char strHtml[] PROGMEM = R"rawliteral(
<!DOCTYPE html> 
<html>
    <head>
        <meta name="viewport" content="width=device-width, initial-scale=1.0">
        <script src="https://code.jquery.com/jquery-1.6.4.js"></script>
        <title>Light control - dashboard</title>
        <style>
            html, body {
            margin: 100;
            padding: 100;
            font-family: "Roboto", sans-serif;
            background-color: #ffffff;
            }
            h1 {
                text-align: center;
                padding: 30px;
                font-size: 60px;
                background-color: #4bb39a; 
                color: #ffffff;
            }
            .controlContainer {
                display:inline-flex;
                justify-content: center;
                text-align: center;
                text-align: center;
                width: 100%;
            }
            .btnContainer {
                display: inline-flex;
                width: 50%;
                border: 5px solid #ffffff;
                font-size: 30px;
                font-weight: bold;
                background-color: #4bb39a;
                color: #ffffff;
                padding: 20px;
            }
            .btns {
                width: 100%;
            }
            button {
                text-align: center;
                background-color: #ffffff;
                color: #4bb39a;
                width: 140px;
                padding: 25px 25px;
                text-align: center;
                font-size: 30px;
                border: 3px outset;
                opacity: 0.9;
                font-weight: bold;
            }
            button:hover {
                opacity: 1;
            }
            .dot {
                height: 30px;
                width: 30px;
                background-color: #d3d3d3;
                border-radius: 50%;
                display:inline-block;
                
            }
            .container {
                align-items: center;
                justify-content: center;
                display:flex;
                border: 1px  #4bb39a;
                background-color: #ffffff;
                padding: 30px;
            }
            .gauge {
                text-align: center;
                min-width: 30%;
                padding: 5%;
                font-family: "Roboto", sans-serif;
                font-size: 30px;
                font-weight: bold;
                color: #4bb39a;
            }
            .gauge__body {
                width: 100%;
                height: 0;
                padding-bottom: 50%;
                background: #b4c0be;
                position: relative;
                border-top-left-radius: 100% 200%;
                border-top-right-radius: 100% 200%;
                overflow: hidden;
            }
            .gauge__fill {
                position: absolute;
                top: 100%;
                left: 0;
                width: inherit;
                height: 100%;
                background: #4bb39a;
                transform-origin: center top;
                transform: rotate(0.25turn);
                transition: transform 2s ease-out;
            }
            .gauge__cover {
                width: 75%;
                height: 150%;
                background: #ffffff;
                border-radius: 50%;
                position: absolute;
                top: 25%;
                left: 50%;
                transform: translateX(-50%);
                /* Text */
                display: flex;
                align-items: center;
                justify-content: center;
                padding-bottom: 25%;
                padding-left: 20%;
                padding-right: 20%;
                box-sizing: border-box;
            }
            .slidecontainer {
                text-align: center;
                font-weight: bold;
                color: #ffffff;
                width: 50%;
                border: 5px solid #ffffff;
                padding: 20px;
                font-size: 30px;
                background-color: #4bb39a;
            }
            .slider {
                -webkit-appearance: none;
                width: 80%;
                height: 10px;
                background: #d3d3d3;
                outline: none;
                opacity: 0.7;
                -webkit-transition: .2s;
                transition: opacity .2s;
            }
            .slider:hover {
                opacity: 1;
            }
            .slider::-webkit-slider-thumb {
                -webkit-appearance: none;
                appearance: none;
                width: 15px;
                height: 30px;
                border-radius: 20%;
                background: #ffffff;
                cursor: pointer;
            }
            .slider::-moz-range-thumb {
                width: 20px;
                height: 30px;
                border-radius: 0.1%;
                background: #ffffff;
                cursor: pointer;
            }
            #thres_container {
                font-size: 60px;
            }
        </style>
    </head>
    <body>
        <h1>Light Source Control Dashboard</h1>
        <div class="container" >
            <div id="lightGuage" class="gauge">
                <p>Light</p>
                <div class="gauge__body">
                <div class="gauge__fill"></div>
                <div class="gauge__cover"></div>
                </div>
            </div>
            <div id="tempGuage" class="gauge">
                <p>Temperature</p>
                <div class="gauge__body">
                <div class="gauge__fill"></div>
                <div class="gauge__cover"></div>
                </div>
            </div>
        </div>
        <div class="controlContainer">
            <div class="slidecontainer">
                <p>Set light source threshold</p>     
                <p id="thres_container"><span id="thres_val"></span>%</p>                
                <input type="range" oninput="updateSlider(this)" id="lightSlider" min="0" max="100" value="%SLIDERVALUE%" step="1" class="slider">
            </div>
            <div class="slidecontainer">
                <p>Set temperature to turn on Fan</p>     
                <p id="thres_container"><span id="tempThres_val"></span>C</p>                
                <input type="range" oninput="updateTempSlider(this)" id="tempSlider" min="0" max="50" value="40" step="1" class="slider">
            </div>
                        <div class="btnContainer">
                <div class="btns">
                    <p>LED</p>
                    <button id="ledBtn" onclick="ledClick()">LED ON</button>
                    <br><br>
                    <div id="ledDot" class="dot"></div>
                </div>
                <div class="btns">
                    <p>Fan</p>
                    <button id="fanBtn" onclick="fanClick()">FAN ON</button>
                    <br><br>
                    <div id="fanDot" class="dot"></div>
                </div>
            </div>
        </div>
        <script>
            function updateSlider(element) {
                var sliderValue = document.getElementById("lightSlider").value;
                document.getElementById("thres_val").innerHTML = sliderValue;
                var xhr = new XMLHttpRequest();
                xhr.open("GET", "/slider?value="+sliderValue, true);
                xhr.send();
            }

            function updateTempSlider(element) {
                var sliderValue = document.getElementById("tempSlider").value;
                document.getElementById("tempThres_val").innerHTML = sliderValue;
                var xhr = new XMLHttpRequest();
                xhr.open("GET", "/tempSlider?value="+sliderValue, true);
                xhr.send();
            }

            const lightGauge = document.getElementById("lightGuage");
            const tempGuage = document.getElementById("tempGuage");

            var getSensorStatus = function () {
                var xhr = new XMLHttpRequest();
                xhr.onreadystatechange = function () {
                    if (this.readyState == 4 && this.status == 200) {
                        var sensStatus = JSON.parse(this.responseText);
                        var photoVal = sensStatus.photoVal;
                        var tempVal = sensStatus.tempVal;
                        var ledON = sensStatus.ledON;
                        var fanON = sensStatus.fanON;
                        changeDotColor('ledDot', ledON);
                        changeDotColor('fanDot', fanON);
                        setGaugeValue(lightGauge, photoVal / 1024, "%");
                        setGaugeValue(tempGuage, tempVal / 100, "C");
                    }
                };
                xhr.open("GET", "/sensorStatus", true);
                xhr.send(null);
            }

            setInterval(getSensorStatus, 1000);

            function setGaugeValue(gauge, value, unit) {
                let val = value;
                if (value < 0) { val = 0;};
                if (value > 1) { val = 1;};
                gauge.querySelector(".gauge__fill").style.transform = `rotate(${ val / 2 }turn)`;
                gauge.querySelector(".gauge__cover").textContent = `${Math.round( val * 100 )}${unit}`;
            }

            function changeDotColor(id, ON) {
                var dot = document.getElementById(id);
                if (ON) {
                    dot.style.backgroundColor = 'salmon';
                } else {
                    dot.style.backgroundColor = '#d3d3d3';
                }
            }

            const ledBtn = document.getElementById('ledBtn');

            ledBtn.addEventListener('click', function onClick() {
                ledBtn.style.backgroundColor = 'salmon';
                ledBtn.style.color = 'white';
                ledBtn.style.border = '3px inset';
                setTimeout(function(){ 
                    ledBtn.style.backgroundColor = '#ffffff'; 
                    ledBtn.style.color = '#4bb39a';
                    ledBtn.style.border = '3px outs`
                }, 1000);               
            });

            function ledClick(){
                var xhr = new XMLHttpRequest();
                xhr.open("GET", "/ledSwitch?value="+"1", true);
                xhr.send();
            };

            const fanBtn = document.getElementById('fanBtn');
            
            fanBtn.addEventListener('click', function onClick() {
                fanBtn.style.backgroundColor = 'salmon';
                fanBtn.style.color = 'white';
                fanBtn.style.border = '3px inset';
                setTimeout(function(){ 
                    fanBtn.style.backgroundColor = '#ffffff'; 
                    fanBtn.style.color = '#4bb39a';
                    fanBtn.style.border = '3px outset';
                }, 1000);               
            });

            function fanClick(){
                var xhr = new XMLHttpRequest();
                xhr.open("GET", "/fanSwitch?value="+"1", true);
                xhr.send();
            };
        </script>
    </body>
</html>
)rawliteral";

//##################################################################
// Build String in JSON format
String get_json() {
  StaticJsonDocument<200> doc;
  doc["Status"] = 200;
  doc["photoVal"] = photo_val;
  doc["tempVal"] = temp_val;
  doc["fanON"] = fanON;
  doc["manualFan"] = manualFan;
  doc["ledON"] = ledON;
  doc["manualLED"] = manualLED;
  String jsonStr; 
  serializeJson(doc, jsonStr);
  return jsonStr;
}

//##################################################################
void setup() {
  // Pin IO setup
  pinMode(FAN_PIN, OUTPUT);
  pinMode(LED_PIN, OUTPUT);
  Serial.begin(115200);

  //##################################################################
  // PainlessMesh setup
  mesh.setDebugMsgTypes( ERROR | STARTUP | CONNECTION);  // set before init() so that you can see startup messages

  mesh.init( MESH_PREFIX, MESH_PASSWORD, &userScheduler, MESH_PORT );
  mesh.onReceive([](auto nodeId, auto msg) {
    receivedCallback(nodeId, msg);
  });
  mesh.onNewConnection(&newConnectionCallback);
  mesh.onChangedConnections(&changedConnectionCallback);
  mesh.onNodeTimeAdjusted(&nodeTimeAdjustedCallback);
 
  userScheduler.addTask( taskSendMessage );
  taskSendMessage.enable();

  myAPIP = IPAddress(mesh.getAPIP());
  Serial.println("My AP IP is " + myAPIP.toString());

  //##################################################################
  //Async webserver setup

  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send_P(200, "text/html", strHtml);
  });

  // Get slider value from the client and set to threshold variable.
  server.on("/slider", HTTP_GET, [] (AsyncWebServerRequest *request) {
    String inputMessage;

    if (request->hasParam(PARAM_INPUT)) {
      inputMessage = request->getParam(PARAM_INPUT)->value();
      sliderValue = inputMessage;
      photo_thres = sliderValue.toInt() * 10;
    }
    else {
      inputMessage = "No message sent";
    }
    request->send(200, "text/plain", "OK");
  });

  // Get temperature slider value and set to temperature threshold variable
  server.on("/tempSlider", HTTP_GET, [] (AsyncWebServerRequest *request) {
    String inputMessage;

    if (request->hasParam(PARAM_INPUT)) {
      inputMessage = request->getParam(PARAM_INPUT)->value();
      sliderValue = inputMessage;
      temp_thres = sliderValue.toInt();
    }
    else {
      inputMessage = "No message sent";
    }
    request->send(200, "text/plain", "OK");
  });

  // Send client sensor status in JSON format
  server.on("/sensorStatus", HTTP_GET, [](AsyncWebServerRequest *request){
     request->send(200, "text/plain", get_json().c_str());
  });

  // Switch LED if the button is pressed by the client
  server.on("/ledSwitch", HTTP_GET, [] (AsyncWebServerRequest *request) {
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
    Serial.println("LED switched!");
    request->send(200, "text/plain", "led switched!");
   });

   // Switch Fan if the button is pressed by the client
   server.on("/fanSwitch", HTTP_GET, [] (AsyncWebServerRequest *request) {
    if (manualFan == true){
      if (fanON) {
        fanON = false;
      } else {
        fanON = true;
      }
      manualFan = false;
    }
    else {
      if (fanON) {
        fanON = false;
      } else {
        fanON = true;
      }
      manualFan = true;
    }
    Serial.println("Fan switched!");
    request->send(200, "text/plain", "OK");
  });

  server.begin();
}

// Turn LED on if luminosity is below the threshold
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

// Turn Fan on if temperature is above the threshold
void autoFanControl(int val, int thres) {
  // turn on the LED depending on the photoresister value
  if (manualFan == false){
    if (val > thres) {
      fanON = true;
    } 
    else {
      fanON = false;
    }
  }
}


void fanControl() {
  if (fanON) {
    digitalWrite(FAN_PIN, HIGH);
  } else {
    digitalWrite(FAN_PIN, LOW);
  }
}


void loop() {
  mesh.update();
  if(myIP != getlocalIP()){
    myIP = getlocalIP();
    Serial.println("My IP is " + myIP.toString());
  }

  // Perform read and action every second
  myTime = millis();
  if (myTime % 1000 == 0) {
    photo_val = analogRead(PHOTO_PIN);
    autoLedControl(photo_val, photo_thres);
    ledControl();
    autoFanControl(temp_val, temp_thres);
    fanControl();
  }
}


IPAddress getlocalIP() {
  return IPAddress(mesh.getStationIP());
}
