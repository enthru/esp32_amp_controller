#include <WiFi.h>
#include <DNSServer.h>
#include <WebServer.h>
#include <WiFiManager.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebSrv.h>
#include <Adafruit_ADS1X15.h>
#include <Wire.h>
#include <DallasTemperature.h>
#include <ArduinoOTA.h>
#include <Preferences.h>

Preferences preferences;
WiFiManager wifiManager;

bool menu = false;

Adafruit_ADS1115 ads;
float fwdVoltage = 0.0;
float refVoltage = 0.0;
float SWR = 0.0;
float realVoltage = 0.0;
float squareVoltage = 0.0;
float fwdPower = 0.0;
float refPower = 0.0;
String strFwdVoltage;
int16_t adc0;
int16_t adc1;
int16_t adc2;
String strRefVoltage;
String strSWR;
float powerCoeff = 0.00;

bool powerEnabled;
bool protectionEnabled = true;
String powerState = "unknown";
String protectionState = "unknown";

bool alarma = false;
String alarmType = "none";

#define AMP_POWER 18
#define TEMP1_SENSOR 17

#define PWM_PIN 16
#define PWM_CHANNEL 0
#define PWM_FREQ 5000
#define PWM_RESOLUTION 8

#define PTT_PIN 5
#define HALL_PIN 4 

float inputVoltage;
#define MIN_POWER 1.0
float maxCurrent;
float maxSWR;
int minCoeff;
int alarmaTickTimer;
bool changed = true;
int PWMValue;
int fanIncrement = 155;

int alarmatick = 0;

//ACS785
const float SENSITIVITY = 0.04;
const float OFFSET_VOLT = 2.472;
float current = 0.00;
float adcVoltage = 0.00;

const char* PARAM_INPUT_1 = "powerState";
const char* PARAM_INPUT_2 = "protectionState";

//temp1
OneWire oneWire(TEMP1_SENSOR);
DeviceAddress sensor1 = { 0x28, 0xCA, 0xBA, 0x55, 0x5, 0x0, 0x0, 0xD0 };
DallasTemperature sensors(&oneWire);
float temp1 = 0.0;

AsyncWebServer server(80);
AsyncEventSource events("/events");

unsigned long webLastTime = 0;
unsigned long guardLastTime = 0;
unsigned long tempLastTime = 0;

unsigned int webTimerDelay;
unsigned int guardTimerDelay;
unsigned int guardTempDelay;

//checkbox with powerbutton
String powerStateValue;
String protectionStateValue;

//webserver update
String inputMessage;
String inputParam;

void getSensorReadings(){
  //swr and power
  adc0 = ads.readADC_SingleEnded(0);
  adc1 = ads.readADC_SingleEnded(1);
  fwdVoltage = (adc0 * 0.1875)/1000;
  refVoltage = (adc1 * 0.1875)/1000;
  realVoltage = fwdVoltage*20;
  squareVoltage = realVoltage*realVoltage;
  fwdPower = (squareVoltage/50)*0.7;
  realVoltage = refVoltage*20;
  squareVoltage = realVoltage*realVoltage;
  refPower = (squareVoltage/50)*0.7;
  if (fwdPower > MIN_POWER) {
    SWR = (fwdPower+refPower)/(fwdPower-refPower);
  }
  else {
    SWR = 0;
  }
  
  //current sensor
  adc2 = ads.readADC_SingleEnded(2);
  adcVoltage = (adc2 * 0.1875)/1000;
  current = (adcVoltage - OFFSET_VOLT)/ SENSITIVITY;
  //coeff
  if (fwdPower > MIN_POWER) {
    powerCoeff = (fwdPower + refPower)/(inputVoltage*current)*100;
  }
  else {
    powerCoeff = 100;
  }
}

const char header_html[] PROGMEM = R"rawliteral(
  <!DOCTYPE HTML><html>
  <head>
    <title>ESP32 Amplifier controller</title>
    <meta name="viewport" content="width=device-width, initial-scale=1">     
    <link rel="stylesheet" href="https://use.fontawesome.com/releases/v5.7.2/css/all.css" integrity="sha384-fnmOCqbTlWIlj8LyTjo7mOUStjsKC4pOpQbqyi7RrhN7udi9RwhKkMHpvLbHG9Sr" crossorigin="anonymous">
    <link rel="icon" href="data:,">
    <style>
      html {font-family: Arial; display: inline-block; text-align: center;}
      p { font-size: 1.2rem;}
      body {  margin: 0;}
      .topnav { overflow: hidden; background-color: #50B8B4; color: white; font-size: 1rem; }
      .content { padding: 20px; }
      .card { background-color: white; box-shadow: 2px 2px 12px 1px rgba(140,140,140,.5); }
      .cards { max-width: 800px; margin: 0 auto; display: grid; grid-gap: 2rem; grid-template-columns: repeat(auto-fit, minmax(200px, 1fr)); }
      .reading { font-size: 1.4rem; }
      h2 {font-size: 3.0rem;}
      .switch {position: relative; display: inline-block; width: 60px; height: 34px} 
      .switch input {display: none}
      .slider {position: absolute; top: 0; left: 0; right: 0; bottom: 0; background-color: #ccc; border-radius: 17px}
      .slider:before {position: absolute; content: ""; height: 26px; width: 26px; left: 4px; bottom: 4px; background-color: #fff; -webkit-transition: .4s; transition: .4s; border-radius: 34px}
      input:checked+.slider {background-color: #2196F3}
      input:checked+.slider:before {-webkit-transform: translateX(26px); -ms-transform: translateX(26px); transform: translateX(26px)}
    </style>
  </head>
)rawliteral";

String processor(const String& var){
  getSensorReadings();
  if(var == "FORWARD"){
    return String(fwdPower);
  }
  if(var == "HEADER"){
    return String(header_html);
  }
  else if(var == "REFLECTED"){
    return String(refPower);
  }
  else if(var == "SWR"){
    return String(SWR);
  }
  else if(var == "CURRENT"){
    return String(current);
  }
  else if(var == "ALARM"){
    if (!alarma){
      return String("None");
    }
  }
  else if(var == "COEFF"){
    if (alarma){
      return String(powerCoeff);
    }
  }
  else if(var == "ALARMATICKTIMER"){
    return String(alarmaTickTimer);
  }
  else if(var == "MAXCURRENT"){
    return String(maxCurrent);
  }
  else if(var == "MINCOEFF"){
    return String(minCoeff);
  }
  else if(var == "MAXSWR"){
    return String(maxSWR);
  }
  
  else if(var == "TEMP1"){
    return String(temp1);
  }
  
  else if(var == "POWERBUTTON"){
    powerStateValue = powerState;
    return "<label class=\"switch\"><input type=\"checkbox\" onchange=\"togglePowerCheckbox(this)\" id=\"powerStateOutput\" " + powerStateValue + "><span class=\"slider\"></span></label>";
  }

  else if(var == "PROTECTIONBUTTON"){
    protectionStateValue = protectionState;
    return "<label class=\"switch\"><input type=\"checkbox\" onchange=\"toggleProtectionCheckbox(this)\" id=\"protectionStateOutput\" " + protectionStateValue + "><span class=\"slider\"></span></label>";
  }
  return String();
}

String menu_processor(const String& var){
  if(var == "ALARMATICKTIMER"){
    return String(alarmaTickTimer);
  }
  if(var == "HEADER"){
    return String(header_html);
  }
  else if(var == "MAXCURRENT"){
    return String(maxCurrent);
  }
  else if(var == "MINCOEFF"){
    return String(minCoeff);
  }
  else if(var == "MAXSWR"){
    return String(maxSWR);
  }
  else if(var == "PWMVALUE"){
    return String(PWMValue);
  }
  else if(var == "DEFAULTENABLED"){
    return String(powerEnabled);
  }
  else if(var == "WEBTIMERDELAY"){
    return String(webTimerDelay);
  }
  else if(var == "GUARDTIMERDELAY"){
    return String(guardTimerDelay);
  }
  else if(var == "GUARDTEMPDELAY"){
    return String(guardTempDelay);
  }
  else if(var == "INPUTVOLTAGE"){
    return String(inputVoltage);
  }
  return String();
}

String outputPowerState(){
  if(powerEnabled){
    return "checked";
  }
  else {
    return "";
  }
  return "";
}

String outputProtectionState(){
  if(protectionEnabled){
    return "checked";
  }
  else {
    return "";
  }
  return "";
}

const char index_html[] PROGMEM = R"rawliteral(
  %HEADER%
  <body>
    <div class="topnav" id="topnava">
      <h1>Main screen</h1>
      <span id="millis"></span>
    </div>
    <div class="content">
      <div class="cards">
        <div class="card">
          <p><i class="fas" style="color:#059e8a;"></i> FORWARD</p><p><span class="reading"><span id="fwd">%FORWARD%</span> W</span></p>
        </div>
        <div class="card">
          <p><i class="fas" style="color:#00add6;"></i> REFLECTED</p><p><span class="reading"><span id="ref">%REFLECTED%</span> W</span></p>
        </div>
        <div class="card">
          <p><i class="fas" style="color:#e1e437;"></i> SWR</p><p><span class="reading"><span id="SWR">%SWR%</span></span></p>
        </div>
        <div class="card">
          <p><i class="fas" style="color:#059e8a;"></i> POWER</p><p><span class="reading"></span></p>
          %POWERBUTTON%
        </div>
        <div class="card">
          <p><i class="fas" style="color:#059e8a;"></i> PROTECTION</p><p><span class="reading"></span></p>
          %PROTECTIONBUTTON%
        </div>
        <div class="card">
          <p><i class="fas" style="color:#059e8a;"></i> CURRENT</p><p><span class="reading"><span id="current">%CURRENT%</span> A</span></p>
        </div>
        <div class="card">
          <p><i class="fas fa-thermometer-half" style="color:#059e8a;"></i> TEMPERATURE</p><p><span class="reading"><span id="temp1">%TEMP1%</span> C</span></p>
        </div>
        <div class="card" id="alarmacolor">
          <p><i class="fas" style="color:#059e8a;"></i> ALARM TYPE</p><p><span class="reading"><span id="alarma">%ALARM%</span></span></p>
        </div>
        <div class="card">
          <p><i class="fas" style="color:#059e8a;"></i> COEFF</p><p><span class="reading"><span id="powerCoeff">%COEFF%</span> %</span></p>
        </div>
      </div>
      <br><a href="/menu.html">Enter menu</a>
    </div>
  <script>
  if (!!window.EventSource) {
  var source = new EventSource('/events');
  
  source.addEventListener('open', function(e) {
    console.log("Events Connected");
  }, false);
  
  source.addEventListener('error', function(e) {
    if (e.target.readyState != EventSource.OPEN) {
      console.log("Events Disconnected");
    }
  }, false);
  
  source.addEventListener('message', function(e) {
    console.log("message", e.data);
  }, false);

  source.addEventListener('millis', function(e) {
    console.log("millis", e.data);
    document.getElementById("millis").innerHTML = e.data;
  }, false);

  source.addEventListener('navcolor', function(e) {
    console.log("navcolor", e.data);
    document.getElementById("topnava").style.backgroundColor = e.data;
  }, false);

  source.addEventListener('forward', function(e) {
    console.log("forward", e.data);
    document.getElementById("fwd").innerHTML = e.data;
  }, false);
  
  source.addEventListener('reflected', function(e) {
    console.log("reflected", e.data);
    document.getElementById("ref").innerHTML = e.data;
  }, false);
  
  source.addEventListener('SWR', function(e) {
    console.log("SWR", e.data);
    document.getElementById("SWR").innerHTML = e.data;
  }, false);

  source.addEventListener('current', function(e) {
    console.log("current", e.data);
    document.getElementById("current").innerHTML = e.data;
  }, false);

  source.addEventListener('alarma', function(e) {
    console.log("alarma", e.data);
    document.getElementById("alarma").innerHTML = e.data;
  }, false);

  source.addEventListener('alarmacolor', function(e) {
    document.getElementById("alarmacolor").style.backgroundColor = e.data;
  }, false);

  source.addEventListener('powerCoeff', function(e) {
    console.log("powerCoeff", e.data);
    document.getElementById("powerCoeff").innerHTML = e.data;
  }, false);

  source.addEventListener('temp1', function(e) {
    console.log("temp1", e.data);
    document.getElementById("temp1").innerHTML = e.data;
  }, false);
  }

  function togglePowerCheckbox(element) {
    var xhr = new XMLHttpRequest();
    if(element.checked){ xhr.open("GET", "/update?powerState=1", true); }
    else { xhr.open("GET", "/update?powerState=0", true); }
    xhr.send();
  }

  function toggleProtectionCheckbox(element) {
    var xhr = new XMLHttpRequest();
    if(element.checked){ xhr.open("GET", "/update?protectionState=1", true); }
    else { xhr.open("GET", "/update?protectionState=0", true); }
    xhr.send();
  }

  setInterval(function ( ) {
    var xhttp = new XMLHttpRequest();
    xhttp.onreadystatechange = function() {
      if (this.readyState == 4 && this.status == 200) {
        var inputChecked;
        if( this.responseText == 1){ 
          inputChecked = true;
          console.log("Power", "on");
        }
        else { 
          inputChecked = false;
          console.log("Power", "off");
        }
        document.getElementById("powerStateOutput").checked = inputChecked;
      }
    };
    xhttp.open("GET", "/powerState", true);
    xhttp.send();
  }, 100);
  setInterval(function ( ) {
    var xhttp = new XMLHttpRequest();
    xhttp.onreadystatechange = function() {
      if (this.readyState == 4 && this.status == 200) {
        var inputChecked;
        if( this.responseText == 1){ 
          inputChecked = true;
          console.log("Protection", "on");
        }
        else { 
          inputChecked = false;
          console.log("Protection", "off");
        }
        document.getElementById("protectionStateOutput").checked = inputChecked;
      }
    };
    xhttp.open("GET", "/protectionState", true);
    xhttp.send();
  }, 100);
  </script>
  </body>
  </html>)rawliteral";

const char menu_html[] PROGMEM = R"rawliteral(
  %HEADER%
  <body>
    <div class="topnav" id="topnava">
      <h1>Menu screen</h1>
    </div>
    <div class="content">
        <form action="/get">
          <p>Max current: <input type="text" name="maxCurrent" id="maxCurrent" value="%MAXCURRENT%"> <input type="submit" value="Submit"></p>
        </form>
        <form action="/get">
          <p>Min coeff: <input type="text" name="minCoeff" id="minCoeff" value="%MINCOEFF%"> <input type="submit" value="Submit"></p>
        </form>
        <form action="/get">
          <p>Max SWR: <input type="text" name="maxSWR" id="maxSWR" value="%MAXSWR%"> <input type="submit" value="Submit"></p>
        </form>
        <form action="/get">
          <p>Base PWM: <input type="text" name="PWMValue" id="PWMValue" value="%PWMVALUE%"> <input type="submit" value="Submit"></p>
        </form>
        <form action="/get">
          <p>Default enabled: <input type="text" name="defaultEnabled" id="defaultEnabled" value="%DEFAULTENABLED%"> <input type="submit" value="Submit"></p>
        </form>
        <form action="/get">
          <p>Voltage to calculate coeff: <input type="text" name="inputVoltage" id="inputVoltage" value="%INPUTVOLTAGE%"> <input type="submit" value="Submit"></p>
        </form>
        <p>Timers and delays: </p>
        <form action="/get">
          <p>Alarm reset time: <input type="text" name="alarmaTickTimer" id="alarmaTickTimer" value="%ALARMATICKTIMER%"> <input type="submit" value="Submit"></p>
        </form>
        <form action="/get">
          <p>Web page: <input type="text" name="webTimerDelay" id="webTimerDelay" value="%WEBTIMERDELAY%"> <input type="submit" value="Submit"></p>
        </form>
        <form action="/get">
          <p>Protectuion check: <input type="text" name="guardTimerDelay" id="guardTimerDelay" value="%GUARDTIMERDELAY%"> <input type="submit" value="Submit"></p>
        </form>
        <form action="/get">
          <p>Temp check: <input type="text" name="guardTempDelay" id="guardTempDelay" value="%GUARDTEMPDELAY%"> <input type="submit" value="Submit"></p>
        </form>
        <br><a href="/">Return to main screen</a>
    </div>
  <script>
    if (!!window.EventSource) {
      var source = new EventSource('/events');
      
      source.addEventListener('open', function(e) {
        console.log("Events Connected");
      }, false);
    
      source.addEventListener('navcolor', function(e) {
        console.log("navcolor", e.data);
        document.getElementById("topnava").style.backgroundColor = e.data;
      }, false);
    }
  </script>
  </body>
  </html>)rawliteral";

void setup() {
  Serial.begin(115200);
  wifiManager.autoConnect("AMP_CONTROL");
  ArduinoOTA.setPort(3232);
  ArduinoOTA.setHostname("amp_controller");
  ArduinoOTA.setPassword("admin");

  preferences.begin("file", false);
  alarmaTickTimer = preferences.getInt("alarmaTickTimer", 1000);
  maxCurrent = preferences.getFloat("maxCurrent", 17.0);
  maxSWR = preferences.getFloat("maxSWR", 2.0);
  minCoeff = preferences.getInt("minCoeff", 44);
  PWMValue = preferences.getInt("PWMValue", 100);
  powerEnabled = preferences.getBool("defaultEnabled", 0);
  protectionEnabled = preferences.getBool("protectionEnabled", 1);
  webTimerDelay = preferences.getInt("webTimerDelay", 500);
  guardTimerDelay = preferences.getInt("guardTimerDelay", 10);
  guardTempDelay = preferences.getInt("guardTempDelay", 8000);
  inputVoltage = preferences.getFloat("inputVoltage", 13.8);
    ArduinoOTA
    .onStart([]() {
      String type;
      if (ArduinoOTA.getCommand() == U_FLASH)
        type = "sketch";
      else // U_SPIFFS
        type = "filesystem";

      // NOTE: if updating SPIFFS this would be the place to unmount SPIFFS using SPIFFS.end()
      Serial.println("Start updating " + type);
    })
    .onEnd([]() {
      Serial.println("\nEnd");
    })
    .onProgress([](unsigned int progress, unsigned int total) {
      Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
    })
    .onError([](ota_error_t error) {
      Serial.printf("Error[%u]: ", error);
      if (error == OTA_AUTH_ERROR) Serial.println("Auth Failed");
      else if (error == OTA_BEGIN_ERROR) Serial.println("Begin Failed");
      else if (error == OTA_CONNECT_ERROR) Serial.println("Connect Failed");
      else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Failed");
      else if (error == OTA_END_ERROR) Serial.println("End Failed");
    });

  ArduinoOTA.begin();

  sensors.setResolution(sensor1, 9);

  pinMode(AMP_POWER, OUTPUT);
  digitalWrite(AMP_POWER, powerEnabled);
  pinMode(PTT_PIN, INPUT); 
  pinMode(HALL_PIN, INPUT);

  const int MAX_DUTY_CYCLE = (int)(pow(2, PWM_RESOLUTION) - 1);
  ledcSetup(PWM_CHANNEL, PWM_FREQ, PWM_RESOLUTION);
  ledcAttachPin(PWM_PIN, PWM_CHANNEL);

  for (int i = 3; i > 0; i--) {
    if (!ads.begin())
    {
      Serial.println("Failed to initialize ADS.");
      delay(1000);
    }
  }

  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send_P(200, "text/html", index_html, processor);
  });
  
  server.on("/menu.html", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send_P(200, "text/html", menu_html, menu_processor);
  });

  server.on("/powerState", HTTP_GET, [] (AsyncWebServerRequest *request) {
    request->send(200, "text/plain", String(digitalRead(AMP_POWER)).c_str());
  });

  server.on("/protectionState", HTTP_GET, [] (AsyncWebServerRequest *request) {
    request->send(200, "text/plain", String(protectionEnabled).c_str());
  });

  server.on("/update", HTTP_GET, [] (AsyncWebServerRequest *request) {
    if (request->hasParam(PARAM_INPUT_1)) {
      inputMessage = request->getParam(PARAM_INPUT_1)->value();
      inputParam = PARAM_INPUT_1;
      digitalWrite(AMP_POWER, inputMessage.toInt());
      powerEnabled = !powerEnabled;
      alarma = false;
      alarmatick = 0;
      alarmType = "none";
    } 
    if (request->hasParam(PARAM_INPUT_2)) {
      inputMessage = request->getParam(PARAM_INPUT_2)->value();
      inputParam = PARAM_INPUT_2;
      protectionEnabled = !protectionEnabled;
      preferences.begin("file", false);
      preferences.putBool("protectionEnabled", protectionEnabled);
      preferences.end();
    }
    request->send(200, "text/plain", "OK");
  });

  server.on("/get", HTTP_GET, [] (AsyncWebServerRequest *request) {
    String inputMessage;
    String inputParam;

    if (request->hasParam("alarmaTickTimer")) {
      inputMessage = request->getParam("alarmaTickTimer")->value();
      inputParam = "alarmaTickTimer";
      if (inputMessage.toInt() < 1000) {
        alarmaTickTimer = inputMessage.toInt();
        preferences.begin("file", false);
        preferences.putInt("alarmaTickTimer", inputMessage.toInt());
        preferences.end();
      }
      else {
        alarmaTickTimer = 1000;
        preferences.begin("file", false);
        preferences.putInt("alarmaTickTimer", 1000);
        preferences.end();
      }
    } 
    else if (request->hasParam("maxCurrent")) {
      inputMessage = request->getParam("maxCurrent")->value();
      inputParam = "maxCurrent";
      maxCurrent = inputMessage.toFloat();
      preferences.begin("file", false);
      preferences.putFloat("maxCurrent", inputMessage.toFloat());
      preferences.end();
    }
    else if (request->hasParam("maxSWR")) {
      inputMessage = request->getParam("maxSWR")->value();
      inputParam = "maxSWR";
      maxSWR = inputMessage.toFloat();
      preferences.begin("file", false);
      preferences.putFloat("maxSWR", inputMessage.toFloat());
      preferences.end();
    }
    else if (request->hasParam("minCoeff")) {
      inputMessage = request->getParam("minCoeff")->value();
      inputParam = "minCoeff";
      minCoeff = inputMessage.toInt();
      preferences.begin("file", false);
      preferences.putInt("minCoeff", inputMessage.toInt());
      preferences.end();
    }
    else if (request->hasParam("PWMValue")) {
      inputMessage = request->getParam("PWMValue")->value();
      inputParam = "PWMValue";
      PWMValue = inputMessage.toInt();
      preferences.begin("file", false);
      preferences.putInt("PWMValue", inputMessage.toInt());
      preferences.end();
    }
    else if (request->hasParam("defaultEnabled")) {
      inputMessage = request->getParam("defaultEnabled")->value();
      inputParam = "defaultEnabled";
      preferences.begin("file", false);
      preferences.putBool("defaultEnabled", inputMessage.toInt());
      preferences.end();
    }
    else if (request->hasParam("webTimerDelay")) {
      inputMessage = request->getParam("webTimerDelay")->value();
      inputParam = "webTimerDelay";
      webTimerDelay = inputMessage.toInt();
      preferences.begin("file", false);
      preferences.putBool("webTimerDelay", inputMessage.toInt());
      preferences.end();
    }
    else if (request->hasParam("guardTimerDelay")) {
      inputMessage = request->getParam("guardTimerDelay")->value();
      inputParam = "guardTimerDelay";
      guardTimerDelay = inputMessage.toInt();
      preferences.begin("file", false);
      preferences.putBool("guardTimerDelay", inputMessage.toInt());
      preferences.end();
    }
    else if (request->hasParam("guardTempDelay")) {
      inputMessage = request->getParam("guardTempDelay")->value();
      inputParam = "guardTempDelay";
      guardTempDelay = inputMessage.toInt();
      preferences.begin("file", false);
      preferences.putBool("guardTempDelay", inputMessage.toInt());
      preferences.end();
    }
    else if (request->hasParam("inputVoltage")) {
      inputMessage = request->getParam("inputVoltage")->value();
      inputParam = "inputVoltage";
      inputVoltage = inputMessage.toFloat();
      preferences.begin("file", false);
      preferences.putBool("inputVoltage", inputMessage.toFloat());
      preferences.end();
    }
    else {
      inputMessage = "No message sent";
      inputParam = "none";
    }
    
    Serial.println(inputMessage);
    request->send(200, "text/html", inputParam + " set with value: " + inputMessage +
                                     "<br><a href=\"/menu.html\">Return to menu</a>");
    });

  events.onConnect([](AsyncEventSourceClient *client){
    if(client->lastId()){
      Serial.printf("Client reconnected! Last message ID that it got is: %u\n", client->lastId());
    }
    client->send("hello!", NULL, millis(), 10000);
  });

  server.addHandler(&events);
  server.begin();
  preferences.end();
}

void loop() {
  if (digitalRead(PTT_PIN)) {
    ledcWrite(PWM_CHANNEL, 255);
  }
  else {
    ledcWrite(PWM_CHANNEL, PWMValue);
  }
    ArduinoOTA.handle();

  //temp delay
  if ((millis() - tempLastTime) > guardTempDelay) {
    sensors.requestTemperatures();
    temp1 = sensors.getTempCByIndex(0);
    events.send(String(temp1).c_str(),"temp1",millis());
    tempLastTime = millis();
  }

  //protection delay
  if ((millis() - guardLastTime) > guardTimerDelay) {
    getSensorReadings();

    if((SWR > maxSWR) && (protectionEnabled)) {
      if (alarmatick > 3) {
        digitalWrite(AMP_POWER, LOW);
        powerEnabled = false;
        alarma = true;
        events.send(String("#FF0000").c_str(),"alarmacolor",millis());
        alarmType = "SWR: " + String(SWR);
        Serial.println("SWR Alarm! SWR: " + String(SWR));
      }
      alarmatick++;
      alarmType = "F! SWR: " + String(SWR);
      Serial.println("F! SWR Alarm! SWR: " + String(SWR));
    }

    if ((current > maxCurrent) && (protectionEnabled)) {
      if (alarmatick > 3) {
        digitalWrite(AMP_POWER, LOW);
        powerEnabled = false;
        alarma = true;
        events.send(String("#FF0000").c_str(),"alarmacolor",millis());
        alarmType = "Current: "+ String(current);
        Serial.println("Overcurrent Alarm! Current: " + String(current));
      }
      alarmatick++;
      alarmType = "F! Current: "+ String(current);
      Serial.println("F! Overcurrent Alarm! Current: " + String(current));
    }

    if (powerCoeff < minCoeff ) {
      if (alarmatick > 3) {
        digitalWrite(AMP_POWER, LOW);
        powerEnabled = false;
        alarma = true;
        events.send(String("#FF0000").c_str(),"alarmacolor",millis());
        alarmType = "Coeff: " + String(powerCoeff);
        Serial.println("Powercoeff Alarm! Coeff: " + String(powerCoeff));
      }
      alarmatick++;
      alarmType = "F! Coeff: " + String(powerCoeff);
      Serial.println("F! Powercoeff Alarm! Coeff: " + String(powerCoeff));
    } 
    guardLastTime = millis();
  }

  //reset alarm counter
  if ((millis() - guardLastTime) > alarmaTickTimer) {
    alarmatick = 0;
  }

  //refresh web page delay
  if ((millis() - webLastTime) > webTimerDelay) {
    unsigned long allSeconds = millis()/1000;
    int runHours = allSeconds/3600;
    int secsRemaining = allSeconds%3600;
    int runMinutes = secsRemaining/60;
    int runSeconds = secsRemaining%60;
    char buf[21];
    sprintf(buf,"Uptime: %02d:%02d:%02d",runHours,runMinutes,runSeconds);
    getSensorReadings();
    events.send("ping",NULL,millis());
    events.send(String(buf).c_str(),"millis",millis());
    events.send(String(fwdPower).c_str(),"forward",millis());
    events.send(String(refPower).c_str(),"reflected",millis());
    if (alarma) {
      events.send(String("#FF0000").c_str(),"alarmacolor",millis());
    }
    else {
      events.send(String("#FFFFFF").c_str(),"alarmacolor",millis());
    }
    if (digitalRead(PTT_PIN)) {
      events.send(String("#FF0000").c_str(),"navcolor",millis());
    }
    else {
      events.send(String("#50B8B4").c_str(),"navcolor",millis());
    }
    if (SWR == 0.0) {
      events.send(String("Unknown").c_str(),"SWR",millis());    
    }
    else {
      events.send(String(SWR).c_str(),"SWR",millis());
    }
    events.send(String(powerState).c_str(),"powerstate",millis());
    events.send(String(protectionState).c_str(),"protectionstate",millis());
    events.send(String(current).c_str(),"current",millis());
    events.send(String(alarmType).c_str(),"alarma",millis());
    events.send(String(PWMValue).c_str(),"PWMValue",millis());
    if (powerCoeff == 100.0) {
      events.send(String("Unknown").c_str(),"powerCoeff",millis());
    }
    else {
      events.send(String(powerCoeff).c_str(),"powerCoeff",millis());
    }
    webLastTime = millis();
  }
}
