// Unified cross-platform sketch (ESP32/ESP8266) with AsyncWebServer, SSE, WiFiManager, OTA
#include <Arduino.h>

// Uncomment to enable AP-only mode with fixed IP
// #define AP_ONLY_MODE

#if defined(ARDUINO_ARCH_ESP32)
#include <WiFi.h>
#include <AsyncTCP.h>
#include <ESPmDNS.h>
#else
#include <ESP8266WiFi.h>
#include <ESPAsyncTCP.h>
#include <ESP8266mDNS.h>
#include <SoftwareSerial.h>
#endif

#include <ESPAsyncWebServer.h>
#include <ArduinoOTA.h>
#include <WiFiManager.h>
#include <mavlink.h>

AsyncWebServer server(80);
AsyncEventSource events("/events");

#if defined(ARDUINO_ARCH_ESP32)
HardwareSerial SerialMAV(2);
#ifndef MAV_RX_PIN
#define MAV_RX_PIN 16
#endif
#ifndef MAV_TX_PIN
#define MAV_TX_PIN 17
#endif
#else
#ifndef MAV_RX_PIN
#define MAV_RX_PIN 5
#endif
#ifndef MAV_TX_PIN
#define MAV_TX_PIN 4
#endif
SoftwareSerial SerialMAV(MAV_RX_PIN, MAV_TX_PIN);
#endif

volatile bool isArmed = false;
uint32_t customMode = 0;
uint8_t targetSystem = 1;
uint8_t targetComponent = 1;
String lastAckText = "";

static const char INDEX_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="UTF-8" />
  <meta name="viewport" content="width=device-width, initial-scale=1.0" />
  <title>ESP-Pixhawk-Arm</title>
  <style>
    body{font-family:system-ui,-apple-system,Segoe UI,Roboto,sans-serif;padding:20px}
    #status{font-weight:700}
    .armed{color:#087f23}
    .disarmed{color:#b00020}
    button{margin-right:8px;padding:8px 12px}
    #msg{color:#666;font-size:14px;margin-top:10px}
    .mode-controls{margin-top:15px}
    .mode-controls button{background:#f0f0f0;border:1px solid #ccc}
  </style>
  <script>
    let es;
    async function getStatus(){
      try{const r=await fetch('/status');if(!r.ok)throw 0;const d=await r.json();apply(d)}catch(e){const el=document.getElementById('status');el.textContent='ERROR';el.className='disarmed'}
    }
    function apply(d){const el=document.getElementById('status');el.textContent=d.armed?'ARMED':'DISARMED';el.className=d.armed?'armed':'disarmed'}
    function showMsg(text){const el=document.getElementById('msg');el.textContent=text;setTimeout(()=>el.textContent='',3000)}
    async function post(path){const bs=document.querySelectorAll('button');bs.forEach(b=>b.disabled=true);try{await fetch(path,{method:'POST'});showMsg('Command sent')}catch(e){showMsg('Error')}finally{bs.forEach(b=>b.disabled=false)}}
    window.addEventListener('DOMContentLoaded',()=>{
      document.getElementById('arm').addEventListener('click',()=>post('/arm'))
      document.getElementById('disarm').addEventListener('click',()=>post('/disarm'))
      document.getElementById('rtl').addEventListener('click',()=>post('/mode/rtl'))
      document.getElementById('stabilize').addEventListener('click',()=>post('/mode/stabilize'))
      document.getElementById('althold').addEventListener('click',()=>post('/mode/alt_hold'))
      if('EventSource' in window){
        es=new EventSource('/events');
        es.addEventListener('status',e=>{try{apply(JSON.parse(e.data))}catch(_){}})
        es.addEventListener('ack',e=>{try{const d=JSON.parse(e.data);if(d.text)showMsg(d.text)}catch(_){}})
      }
      getStatus()
    })
  </script>
</head>
<body>
  <h2>ESP-Pixhawk-Arm</h2>
  <h4>Status: <span id="status" class="disarmed">DISARMED</span></h4>
  <div id="msg"></div>
  <div>
    <button id="arm">ARM</button>
    <button id="disarm">DISARM</button>
  </div>
  <div class="mode-controls">
    <button id="rtl">RTL</button>
    <button id="stabilize">STABILIZE</button>
    <button id="althold">ALT HOLD</button>
  </div>
</body>
</html>
)rawliteral";

void notifyStatus() {
  String s = String("{\"armed\":") + (isArmed ? "true" : "false") + ",\"mode\":" + String(customMode) + "}";
  events.send(s.c_str(), "status");
}

void notifyAck(const String& text) {
  String escaped = text;
  escaped.replace("\"", "\\\"");
  escaped.replace("\\", "\\\\");
  escaped.replace("\n", "\\n");
  escaped.replace("\r", "\\r");
  escaped.replace("\t", "\\t");
  String s = String("{\"text\":\"") + escaped + "\"}";
  events.send(s.c_str(), "ack");
}

void sendMavlinkCommand(uint16_t command, float param1) {
  if (!SerialMAV) return;
  mavlink_message_t m; uint8_t buf[MAVLINK_MAX_PACKET_LEN];
  mavlink_msg_command_long_pack(255, 190, &m, targetSystem, targetComponent, command, 0, param1, 0, 0, 0, 0, 0, 0);
  uint16_t len = mavlink_msg_to_send_buffer(buf, &m);
  SerialMAV.write(buf, len);
}

void handleMavlink() {
  while (SerialMAV.available()) {
    uint8_t c = SerialMAV.read();
    static mavlink_message_t msg; static mavlink_status_t st;
    if (mavlink_parse_char(MAVLINK_COMM_0, c, &msg, &st)) {
      if (msg.msgid == MAVLINK_MSG_ID_HEARTBEAT) {
        mavlink_heartbeat_t hb; mavlink_msg_heartbeat_decode(&msg, &hb);
        bool prev = isArmed; isArmed = (hb.base_mode & MAV_MODE_FLAG_SAFETY_ARMED) != 0; customMode = hb.custom_mode;
        if (isArmed != prev) notifyStatus();
      } else if (msg.msgid == MAVLINK_MSG_ID_COMMAND_ACK) {
        mavlink_command_ack_t ack; mavlink_msg_command_ack_decode(&msg, &ack);
        String result = "CMD " + String(ack.command) + ": ";
        switch (ack.result) {
          case MAV_RESULT_ACCEPTED: result += "ACCEPTED"; break;
          case MAV_RESULT_TEMPORARILY_REJECTED: result += "REJECTED"; break;
          case MAV_RESULT_DENIED: result += "DENIED"; break;
          case MAV_RESULT_UNSUPPORTED: result += "UNSUPPORTED"; break;
          case MAV_RESULT_FAILED: result += "FAILED"; break;
          default: result += "UNKNOWN"; break;
        }
        notifyAck(result);
      }
    }
  }
}

void setupWiFi() {
#ifdef AP_ONLY_MODE
  WiFi.mode(WIFI_AP);
  WiFi.softAPConfig(IPAddress(172,217,28,1), IPAddress(172,217,28,1), IPAddress(255,255,255,0));
#if defined(ARDUINO_ARCH_ESP32)
  if (WiFi.softAP("pixhawk32-ap", "12345678")) {
    Serial.println("AP-only mode active: 172.217.28.1");
  } else {
    Serial.println("AP setup failed!");
  }
#else
  if (WiFi.softAP("pixhawk8266-ap", "12345678")) {
    Serial.println("AP-only mode active: 172.217.28.1");
  } else {
    Serial.println("AP setup failed!");
  }
#endif
#else
  WiFi.mode(WIFI_STA);
  WiFiManager wm;
#if defined(ARDUINO_ARCH_ESP32)
  wm.setHostname("pixhawk32");
  if (!wm.autoConnect("esp32-pixhawk")) {
    Serial.println("WiFiManager failed, restarting...");
    ESP.restart();
  }
#else
  wm.setHostname("pixhawk8266");
  if (!wm.autoConnect("esp8266-pixhawk")) {
    Serial.println("WiFiManager failed, restarting...");
    ESP.restart();
  }
#endif
  Serial.println("WiFi connected");
#endif
}

void sendSetMode(uint8_t mode) {
  if (!SerialMAV) return;
  mavlink_message_t m; uint8_t buf[MAVLINK_MAX_PACKET_LEN];
  mavlink_msg_set_mode_pack(255, 190, &m, targetSystem, mode, 0);
  uint16_t len = mavlink_msg_to_send_buffer(buf, &m);
  SerialMAV.write(buf, len);
}

void setupServer() {
  server.on("/", HTTP_GET, [](AsyncWebServerRequest* request){ request->send_P(200, "text/html", INDEX_HTML); });
  server.on("/status", HTTP_GET, [](AsyncWebServerRequest* request){ String s = String("{\"armed\":") + (isArmed?"true":"false") + ",\"mode\":" + String(customMode) + "}"; request->send(200, "application/json", s); });
  server.on("/arm", HTTP_POST, [](AsyncWebServerRequest* request){ sendMavlinkCommand(400, 1.0f); request->send(200, "application/json", "{\"ok\":true}"); });
  server.on("/disarm", HTTP_POST, [](AsyncWebServerRequest* request){ sendMavlinkCommand(400, 0.0f); request->send(200, "application/json", "{\"ok\":true}"); });
  server.on("/mode/rtl", HTTP_POST, [](AsyncWebServerRequest* request){ sendSetMode(6); request->send(200, "application/json", "{\"ok\":true}"); });
  server.on("/mode/stabilize", HTTP_POST, [](AsyncWebServerRequest* request){ sendSetMode(0); request->send(200, "application/json", "{\"ok\":true}"); });
  server.on("/mode/alt_hold", HTTP_POST, [](AsyncWebServerRequest* request){ sendSetMode(2); request->send(200, "application/json", "{\"ok\":true}"); });
  server.onNotFound([](AsyncWebServerRequest* request){ request->send(404, "application/json", "{\"error\":\"not found\"}"); });
  server.addHandler(&events);
  server.begin();
}

void setup() {
  Serial.begin(115200);
  setupWiFi();
#if defined(ARDUINO_ARCH_ESP32)
  MDNS.begin("pixhawk32");
  ArduinoOTA.begin();
  SerialMAV.begin(57600, SERIAL_8N1, MAV_RX_PIN, MAV_TX_PIN);
#else
  MDNS.begin("pixhawk8266");
  ArduinoOTA.begin();
  SerialMAV.begin(57600);
#endif
  setupServer();
}

void loop() {
  ArduinoOTA.handle();
  handleMavlink();
}
