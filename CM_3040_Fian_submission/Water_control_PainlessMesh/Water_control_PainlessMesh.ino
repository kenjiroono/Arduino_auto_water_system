//************************************************************
// Irrigation control with PainlessMesh networking and Asyncronous server.
// 
// Sensors & Actuators:
//    - DHT11: reads temperature and humidity values
//    - Soil Moisuture Sensor: reads moisture contained in soil
//    - Water pump
// 
// If the Soil moisture level is below set threshold value, the water pump is turned on for 
// specified duration. Alternatively, the pump can be turned on remotely from the 
// dashboard(server). 
// The temperature value is broadcasted via mesh network and the fan is turned on on other 
// node to lower the surface temperature of the plant.  
//************************************************************

#include "IPAddress.h"
#include "painlessMesh.h"
#include <ArduinoJson.h>
#include <DHT.h>

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
bool manualPump = false;
bool pumpON = false;

unsigned long wateredTime;
bool wateredTimeout = true;

// Asyncronous variable
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
  doc["nodeName"] = "WaterControl";
  doc["temperatureVal"] = temperature;
  doc["myAPIP"] = myAPIP;
  doc["myIP"] = myIP;
  String msg; 
  serializeJson(doc, msg);
  mesh.sendBroadcast( msg );

  Serial.println(">>>  " + msg);

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
                background-color: #127BDE; 
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
                background-color: #127BDE;
                color: #ffffff;
                padding: 20px;
            }
            
            .btns {
                width: 100%;
            }
            
            button {
                text-align: center;
                background-color: #ffffff;
                color: #127BDE;
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
                border: 1px  #127BDE;
                background-color: #ffffff;
                padding: 30px;
            }
            
            .gauge {
                text-align: center;
                width: 100%;
                padding: 5%;
                font-family: "Roboto", sans-serif;
                font-size: 30px;
                font-weight: bold;
                color: #127BDE;
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
                background: #127BDE;
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
                background-color: #127BDE;
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
        <h1>Irrigation Control Dashboard</h1>
        <div class="container" >
            <div id="tempGauge" class="gauge">
                <p>Temperature</p>
                <div class="gauge__body">
                <div class="gauge__fill"></div>
                <div class="gauge__cover"></div>
                </div>
            </div>

            <div id = "humidGauge" class="gauge">
                <p>Humidity</p>
                <div class="gauge__body">
                <div class="gauge__fill"></div>
                <div class="gauge__cover"></div>
                </div>
            </div>

            <div id = "soilGauge" class="gauge">
                <p>Soil Moisture</p>
                <div class="gauge__body">
                <div class="gauge__fill"></div>
                <div class="gauge__cover"></div>
                </div>
            </div>
        </div>
        
        <div class="controlContainer">
            <div class="slidecontainer">
                <p>Set irrigation threshold</p>     
                <p id="thres_container"><span id="thres_val"></span>%</p>                
                <input type="range" oninput="updateSlider(this)" id="waterSlider" min="0" max="100" step="1" class="slider">
            </div>
            
            <div class="btnContainer">
                <div class="btns">
                    <p>Water PUMP</p>
                    <button id="pumpBtn" onclick="pumpClick()">PUMP ON</button>
                    <br><br>
                    <div id="pumpDot" class="dot"></div>
                </div>
            </div>
        </div>
        <script>
        
            function updateSlider(element) {
                var sliderValue = document.getElementById("waterSlider").value;
                document.getElementById("thres_val").innerHTML = sliderValue;
                var xhr = new XMLHttpRequest();
                xhr.open("GET", "/slider?value="+sliderValue, true);
                xhr.send();
            }

            const tempGauge = document.getElementById("tempGauge");
            const humidGauge = document.getElementById("humidGauge");
            const soilGauge = document.getElementById("soilGauge");

            var getSensorStatus = function () {
                var xhr = new XMLHttpRequest();
                xhr.onreadystatechange = function () {
                    if (this.readyState == 4 && this.status == 200) {
                        var sensStatus = JSON.parse(this.responseText);
                        console.log(sensStatus);
                        
                        var pumpON = sensStatus.pumpON;
                        var temperatureVal = sensStatus.temperatureVal;
                        var humidityVal = sensStatus.humidityVal;                        
                        var soilMoistureVal = sensStatus.soilMoistureVal;
                        
                        changeDotColor('pumpDot', pumpON);
            
                        setGaugeValue(tempGauge, temperatureVal/100, 'C');
                        setGaugeValue(humidGauge, humidityVal/100, '%');
                        setGaugeValue(soilGauge, soilMoistureVal/100, '%');                    
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


            const pumpBtn = document.getElementById('pumpBtn');

            pumpBtn.addEventListener('click', function onClick() {
                pumpBtn.style.backgroundColor = 'salmon';
                pumpBtn.style.color = 'white';
                pumpBtn.style.border = '3px inset';
                setTimeout(function(){ 
                    pumpBtn.style.backgroundColor = '#ffffff'; 
                    pumpBtn.style.color = '#127BDE';
                    pumpBtn.style.border = '3px outset';
                }, 1000);               
            });

            function pumpClick(){
                var xhr = new XMLHttpRequest();
                xhr.open("GET", "/pumpSwitch?value="+"1", true);
                xhr.send();
            };

        </script>
    </body>
</html>
)rawliteral";

//##################################################################
// Sensor readings in JSON format
String get_json() {
  StaticJsonDocument<200> doc;
  doc["Status"] = 200;
  doc["temperatureVal"] = temperature;
  doc["humidityVal"] = humidity;
  doc["soilMoistureVal"] = (int)soil_moisture;
  doc["pumpON"] = pumpON;
  doc["manualPump"] = manualPump;
  String jsonStr; 
  serializeJson(doc, jsonStr);
  return jsonStr;
}

//##################################################################
void setup() {
  // Pin IO setup
  pinMode(SOIL_PIN, INPUT);
  pinMode(PUMP_PIN, OUTPUT);
  Serial.begin(115200);
  dht.begin();

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
  //Async webserver
  
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send_P(200, "text/html", strHtml);
  });

  // Get slider value from the client and set to threshold variable.
  server.on("/slider", HTTP_GET, [] (AsyncWebServerRequest *request) {
    String inputMessage;
    // GET input1 value on <ESP_IP>/slider?value=<inputMessage>
    if (request->hasParam(PARAM_INPUT)) {
      inputMessage = request->getParam(PARAM_INPUT)->value();
      water_thres = inputMessage.toInt();
    }
    request->send(200, "text/plain", "OK");
  });

  // Send client sensor status in JSON format
  server.on("/sensorStatus", HTTP_GET, [](AsyncWebServerRequest *request){
     request->send(200, "text/plain", get_json().c_str());
  });

  // Switch Pump if the button is pressed by the client
  server.on("/pumpSwitch", HTTP_GET, [] (AsyncWebServerRequest *request) {
    if (manualPump == true){
      if (pumpON) {
        pumpON = false;
      } else {
        pumpON = true;
      }
      manualPump = false;
    }
    else {
      if (pumpON) {
        pumpON = false;
      } else {
        pumpON = true;
      }
      manualPump = true;
    }
    Serial.println("Pump switched!");
    request->send(200, "text/plain", "OK");
  });
  
  server.begin();
}

// Read temperature and humidity level
void readtempHum(){
  temperature = dht.readTemperature();
  humidity = dht.readHumidity();
}

// Read soil moisture level and linearize the value
void readSoilMoist(){
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

// Turn Pump on if soil moisture level is below the threshold
void autoPumpControl(int val, int thres) {
  // turn on the Pump depending on the soil moisture level
  if (manualPump == false){  
    if (val < thres) {
      pumpON = true;
    } else {
      pumpON = false;
    }
  }
}

void pumpControl() {
  digitalWrite(PUMP_PIN, LOW);
  delay(200);
  digitalWrite(PUMP_PIN, HIGH);
  wateredTime = millis();    
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
    readSoilMoist();
    readtempHum();
    autoPumpControl(soil_moisture, water_thres);

    // Wait 10 secounds before irrigate again
    if (pumpON && myTime - wateredTime > 10000) {
      pumpControl();
    }
    
  }
}


IPAddress getlocalIP() {
  return IPAddress(mesh.getStationIP());
}
