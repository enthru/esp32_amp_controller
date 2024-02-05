#include <WiFi.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebSrv.h>
#include <Adafruit_ADS1X15.h>
#include <Wire.h>
#include <DallasTemperature.h>
#include <ArduinoOTA.h>
#include <Preferences.h>

Preferences preferences;

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

bool enabled;
String state = "unknown";
bool alarma = false;
String alarmType = "None";

#define AMP_POWER 18
#define TEMP1_SENSOR 17

#define PWM_PIN 16
#define PWM_CHANNEL 0
#define PWM_FREQ 5000
#define PWM_RESOLUTION 8

#define PTT_PIN 5
#define HALL_PIN 4 

#define INPUT_VOLTAGE 13.8
#define MIN_POWER 1.0
float maxCurrent;
float maxSWR;
int minCoeff;
int coeffAlarmDelay;
bool changed = true;
int PWMValue;
int fanIncrement = 155;

//ACS785
const float SENSITIVITY = 0.04;
const float OFFSET_VOLT = 2.472;
float current = 0.00;
float adcVoltage = 0.00;

const char* PARAM_INPUT_1 = "state";

//temp1
OneWire oneWire(TEMP1_SENSOR);
DeviceAddress sensor1 = { 0x28, 0xCA, 0xBA, 0x55, 0x5, 0x0, 0x0, 0xD0 };
DallasTemperature sensors(&oneWire);
float temp1 = 0.0;
//char tempbuf[4];

const char* ssid = "BlackIsle";
const char* password = "password";

AsyncWebServer server(80);
AsyncEventSource events("/events");

unsigned long webLastTime = 0;
unsigned long guardLastTime = 0;
unsigned long tempLastTime = 0;
unsigned long coeffLastTime = 0;

unsigned long webTimerDelay = 500;
unsigned long guardTimerDelay = 10;
unsigned long guardTempDelay = 8000;

//checkbox with powerbutton
String outputStateValue;

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
    powerCoeff = (fwdPower + refPower)/(INPUT_VOLTAGE*current)*100;
  }
  else {
    powerCoeff = 100;
  }
}

void initWiFi() {
    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid, password);
    Serial.print("Connecting to WiFi ..");
    while (WiFi.status() != WL_CONNECTED) {
        Serial.print('.');
        delay(1000);
    }
    Serial.println(WiFi.localIP());
}

String processor(const String& var){
  getSensorReadings();
  if(var == "FORWARD"){
    return String(fwdPower);
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
  else if(var == "COEFFDELAY"){
    return String(coeffAlarmDelay);
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
    outputStateValue = state;
    return "<label class=\"switch\"><input type=\"checkbox\" onchange=\"toggleCheckbox(this)\" id=\"output\" " + outputStateValue + "><span class=\"slider\"></span></label>";
  }
  return String();
}

String menu_processor(const String& var){
  if(var == "COEFFDELAY"){
    return String(coeffAlarmDelay);
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
    return String(enabled);
  }
  return String();
}

String outputState(){
  if(enabled){
    return "checked";
  }
  else {
    return "";
  }
  return "";
}

const char index_html[] PROGMEM = R"rawliteral(
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
  <body>
    <div class="topnav">
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
          <p><i class="fas" style="color:#059e8a;"></i> CURRENT</p><p><span class="reading"><span id="current">%CURRENT%</span> A</span></p>
        </div>
        <div class="card">
          <p><i class="fas fa-thermometer-half" style="color:#059e8a;"></i> TEMPERATURE</p><p><span class="reading"><span id="temp1">%TEMP1%</span> C</span></p>
        </div>
        <div class="card">
          <p><i class="fas" style="color:#059e8a;"></i> ALARM TYPE</p><p><span class="reading"><span id="alarma">%ALARM%</span></span></p>
        </div>
        <div class="card">
          <p><i class="fas" style="color:#059e8a;"></i> COEFF</p><p><span class="reading"><span id="powerCoeff">%COEFF%</span> %</span></p>
        </div>
        <div class="card">
          <p><i class="fas" style="color:#059e8a;"></i> FAN SPEED</p><p><span class="reading"><span id="fanSpeed"></span> RPM</span></p>
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

  source.addEventListener('powerCoeff', function(e) {
    console.log("powerCoeff", e.data);
    document.getElementById("powerCoeff").innerHTML = e.data;
  }, false);

  source.addEventListener('temp1', function(e) {
    console.log("temp1", e.data);
    document.getElementById("temp1").innerHTML = e.data;
  }, false);
  }

  function toggleCheckbox(element) {
    var xhr = new XMLHttpRequest();
    if(element.checked){ xhr.open("GET", "/update?state=1", true); }
    else { xhr.open("GET", "/update?state=0", true); }
    xhr.send();
  }

  setInterval(function ( ) {
    var xhttp = new XMLHttpRequest();
    xhttp.onreadystatechange = function() {
      if (this.readyState == 4 && this.status == 200) {
        var inputChecked;
        var outputStateM;
        if( this.responseText == 1){ 
          inputChecked = true;
          outputStateM = "On";
        }
        else { 
          inputChecked = false;
          outputStateM = "Off";
        }
        document.getElementById("output").checked = inputChecked;
      }
    };
    xhttp.open("GET", "/state", true);
    xhttp.send();
  }, 1000 ) ;
  </script>
  </body>
  </html>)rawliteral";

const char menu_html[] PROGMEM = R"rawliteral(
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
  <body>
    <div class="topnav">
      <h1>Menu screen</h1>
    </div>
    <div class="content">
        <form action="/get">
          <p>Coeff delay: <input type="text" name="coeffDelay" id="coeffDelay" value="%COEFFDELAY%"> <input type="submit" value="Submit"></p>
        </form>
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
          <p>Power enabled on boot: <input type="text" name="defaultEnabled" id="defaultEnabled" value="%DEFAULTENABLED%"> <input type="submit" value="Submit"></p>
        </form>
        <br><a href="/">Return to main screen</a>
    </div>
  <script>
  </script>
  </body>
  </html>)rawliteral";

void setup() {
  Serial.begin(115200);
  initWiFi();

  ArduinoOTA.setPort(3232);
  ArduinoOTA.setHostname("amp_controller");
  ArduinoOTA.setPassword("admin");

  preferences.begin("file", false);
  coeffAlarmDelay = preferences.getInt("coeffAlarmDelay", 1000);
  maxCurrent = preferences.getFloat("maxCurrent", 17.0);
  maxSWR = preferences.getFloat("maxSWR", 2.0);
  minCoeff = preferences.getInt("minCoeff", 44);
  PWMValue = preferences.getInt("PWMValue", 100);
  enabled = preferences.getBool("defaultEnabled", 0);

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
  digitalWrite(AMP_POWER, enabled);
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

  server.on("/state", HTTP_GET, [] (AsyncWebServerRequest *request) {
    request->send(200, "text/plain", String(digitalRead(AMP_POWER)).c_str());
  });

  server.on("/update", HTTP_GET, [] (AsyncWebServerRequest *request) {
    if (request->hasParam(PARAM_INPUT_1)) {
      inputMessage = request->getParam(PARAM_INPUT_1)->value();
      inputParam = PARAM_INPUT_1;
      digitalWrite(AMP_POWER, inputMessage.toInt());
      enabled = !enabled;
      alarma = false;
    }
    else {
      inputMessage = "No message sent";
      inputParam = "none";
    }
    Serial.println("Power status" + inputMessage);
    request->send(200, "text/plain", "OK");
  });

  server.on("/get", HTTP_GET, [] (AsyncWebServerRequest *request) {
    String inputMessage;
    String inputParam;

    if (request->hasParam("coeffDelay")) {
      inputMessage = request->getParam("coeffDelay")->value();
      inputParam = "coeffDelay";
      if (inputMessage.toInt() < 1000) {
        coeffAlarmDelay = inputMessage.toInt();
        preferences.begin("file", false);
        preferences.putInt("coeffAlarmDelay", inputMessage.toInt());
        preferences.end();
      }
      else {
        coeffAlarmDelay = 1000;
        preferences.begin("file", false);
        preferences.putInt("coeffAlarmDelay", 1000);
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
      enabled = inputMessage.toInt();
      preferences.begin("file", false);
      preferences.putBool("defaultEnabled", inputMessage.toInt());
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
    if (SWR > maxSWR) {
      digitalWrite(AMP_POWER, LOW);
      enabled = false;
      alarma = true;
      alarmType = "SWR";
      Serial.println("SWR Alarm! SWR: " + String(SWR));
    }

    if (current > maxCurrent) {
      digitalWrite(AMP_POWER, LOW);
      enabled = false;
      alarma = true;
      alarmType = "Current";
      Serial.println("Overcurrent Alarm! Current: " + String(current));
    }
    guardLastTime = millis();
  }

  //edflay to check eficiency coefficient
  if ((millis() - coeffLastTime) > coeffAlarmDelay) {
    //this can be caused by problems with LPF for example
    if (powerCoeff < minCoeff ) {
      digitalWrite(AMP_POWER, LOW);
      enabled = false;
      alarma = true;
      alarmType = "Coeff";
      Serial.println("Powercoeff Alarm! Coeff: " + String(powerCoeff));
    } 
    coeffLastTime = millis();
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
    //Serial.println(coeffAlarmDelay);
    events.send("ping",NULL,millis());
    events.send(String(buf).c_str(),"millis",millis());
    events.send(String(fwdPower).c_str(),"forward",millis());
    events.send(String(refPower).c_str(),"reflected",millis());
    if (SWR == 0.0) {
      events.send(String("Unknown").c_str(),"SWR",millis());    
    }
    else {
      events.send(String(SWR).c_str(),"SWR",millis());
    }
    events.send(String(state).c_str(),"state",millis());
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
