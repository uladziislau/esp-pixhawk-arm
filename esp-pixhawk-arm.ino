#include <checksum.h>
#include <SoftwareSerial.h>
#include <ESP8266WiFi.h>
#include <DNSServer.h>
#include <standard/mavlink.h>
#include <String.h>

// Network credentials
const char* ssid     = "DroneEspModule";
const char* password = "";

IPAddress apIP(172, 217, 28, 1);
IPAddress netMsk(255, 255, 255, 0);

DNSServer dnsServer;
const byte DNS_PORT = 53;

WiFiServer server(80);

// Variable to store the HTTP request
String header;

String ArmedState = "DISARMED";
String ModeState = "STABILIZE";

String StatusMessage = "";

bool pixhawk_heartbeat_message_geted = false;
bool pixhawk_ack_message_geted = false;

SoftwareSerial SerialMAV(5,16);

void setup() {
  
  Serial.begin(115200);
  
  // Connect to Wi-Fi network with SSID and password
  Serial.print("Setting AP (Access Point)…");
  
  WiFi.mode(WIFI_AP);
  WiFi.softAPConfig(apIP, apIP, netMsk);
  
  // Remove the password parameter, if you want the AP (Access Point) to be open
  WiFi.softAP(ssid, password);
  
  dnsServer.start(DNS_PORT, "*", apIP);
  
  IPAddress IP = WiFi.softAPIP();
  
  Serial.print("AP IP address: ");
  Serial.println(IP);
  
  server.begin();

  SerialMAV.begin(57600);
}

void loop(){
  
  WiFiClient client = server.available();   // Listen for incoming clients

  if (client) {                             // If a new client connects,
    Serial.println("New Client.");          // print a message out in the serial port
    String currentLine = "";                // make a String to hold incoming data from the client
    while (client.connected()) {            // loop while the client's connected
      if (client.available()) {             // if there's bytes to read from the client,
        char c = client.read();             // read a byte, then
        Serial.write(c);                    // print it out the serial monitor
        header += c;
        if (c == '\n') {                    // if the byte is a newline character
          // if the current line is blank, you got two newline characters in a row.
          // that's the end of the client HTTP request, so send a response:
          if (currentLine.length() == 0) {
            // HTTP headers always start with a response code (e.g. HTTP/1.1 200 OK)
            // and a content-type so the client knows what's coming, then a blank line:
            client.println("HTTP/1.1 200 OK");
            client.println("Content-type:text/html");
            client.println("Connection: close");
            client.println();

            if (header.indexOf("GET /arm") >= 0) {              
              pixhawk_send_arm();
              Serial.println("Request for ARM sended to Pixhawk.");
            } 
            else if (header.indexOf("GET /disarm") >= 0) {              
              pixhawk_send_disarm();
              Serial.println("Request for DISARM sended to Pixhawk.");
            } 
            else if (header.indexOf("GET /rtl") >= 0) {

              if(ArmedState == "ARMED") {
                pixhawk_set_rtl_mode();
                Serial.println("Request for RTL sended to Pixhawk.");
              }
              else Serial.println("Error: Can't set RTL when Disarmed!");
              
            }             
            else if(header.indexOf("GET /") >= 0) {
              Serial.println("Request for GET /");
            }           

            // Don't read_pixhawk_incoming_messages for /get_204 and /generate_204 requests;
            if(header.indexOf("GET /gen_204") >= 0 || header.indexOf("GET //generate_204") >= 0) {
              Serial.println("Don't read_pixhawk_incoming_messages for /get_204 and /generate_204 requests");
            }
            else 
            {
              // Read incoming messages from Pixwhawk.            
              // Задержка для ожидания ответа от Pixhawk
              // Тут нужен цикл пока не получу ответ.            
              int starttime = millis();
              int endtime = starttime;
                          
              while((endtime - starttime) <= 1000 || read_pixhawk_incoming_messages_finish) // do this loop for up to 1000mS
              {
                read_pixhawk_incoming_messages();
                endtime = millis();
              }            
            }
            
            // Start Rendering  HTML Web Page

            client.println("<!DOCTYPE html>");
            
            client.println("<html>");
            client.println("<head>");
            client.println("<meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">");
            client.println("<link rel=\"icon\" href=\"data:,\">");
            client.println("<style>");
            client.println("html { font-family: Helvetica; display: inline-block; margin: 0px auto; text-align: center;");
            client.println("text-decoration: none; font-size: 30px; margin: 2px; cursor: pointer; }");
            client.println(".button { background-color: #64FF64; border: none; color: white; padding: 16px 40px;}");
            client.println(".button2 { background-color: #FF6464; border: none; color: white; padding: 16px 40px;}");
            client.println(".button3 {background-color: #646464; border: none; color: white; padding: 16px 40px;}");
            client.println("</style>");
            client.println("</head>");
            
            client.println("<body>");
            client.println("<h2>HELI-E 4.2</h2>");
            
            // Show pixhawk call status 
            client.println("<h4 style='color: #FF0000'>");
            client.println(StatusMessage);
            client.println("</h4>");           
            
            if(ArmedState=="ARMED" && ModeState=="RTL"){
              client.println("<h2 style='color: #00FF00;'>RTL MODE</h2>");
            }
            else{    
              if (ArmedState=="DISARMED") {
                client.println("<h3 style='color: #FF0000;'>DISARMED</h3>");
                client.println("<p><a href=\"/arm\"><button class=\"button\">ARM</button></a></p>");
              } else if(ArmedState=="ARMED") {
                client.println("<h3 style='color: #00FF00;'>ARMED</h3>");
                client.println("<p><a href=\"/disarm\"><button class=\"button button2\">DISARM</button></a></p>");
                client.println("<p><a href=\"/rtl\"><button class=\"button button3\">RTL</button></a></p>");
              } 
            }
          
          //  client.println("<script>");
          //  client.println("setTimeout(function(){ window.location.replace('/'); }, 5000");
          //  client.println("</script>");
            
            client.println("</body>");
            client.println("</html>");
            
            // The HTTP response ends with another blank line
            client.println();
            
            // Break out of the while loop
            break;
          } 
          else { 
            // if you got a newline, then clear currentLine
            currentLine = "";
          }
        } else if (c != '\r') {  // if you got anything else but a carriage return character,
          currentLine += c;      // add it to the end of the currentLine
        }
      }
    }
    
    // Clear the header variable
    header = "";
    StatusMessage = "";
    
    // Close the connection
    client.stop();
    Serial.println("Client disconnected.");
    Serial.println("");
  }

  dnsServer.processNextRequest();
}

void pixhawk_send_arm() {
  
  mavlink_message_t msg;
  uint8_t buf[MAVLINK_MAX_PACKET_LEN];

  mavlink_msg_command_long_pack(0xFF, 0xBE, &msg, 1, 1, 400, 1,1.0,0,0,0,0,0,0);

  uint16_t len = mavlink_msg_to_send_buffer(buf, &msg);
  SerialMAV.write(buf, len);
}

void pixhawk_send_disarm() {
  
  mavlink_message_t msg;
  uint8_t buf[MAVLINK_MAX_PACKET_LEN];

  mavlink_msg_command_long_pack(0xFF, 0xBE, &msg, 1, 1, 400, 1,0.0,0,0,0,0,0,0);
  
  uint16_t len = mavlink_msg_to_send_buffer(buf, &msg);
  SerialMAV.write(buf, len);
}

void pixhawk_set_stabilize_mode() {
  
  mavlink_message_t msg;
  uint8_t buf[MAVLINK_MAX_PACKET_LEN];
  
  mavlink_msg_set_mode_pack(0xFF, 0xBE, &msg, 1, 209, 0);
  
  uint16_t len = mavlink_msg_to_send_buffer(buf, &msg);
  SerialMAV.write(buf, len);  
}

void pixhawk_set_rtl_mode() {
  
  mavlink_message_t msg;
  uint8_t buf[MAVLINK_MAX_PACKET_LEN];
  
  mavlink_msg_set_mode_pack(0xFF, 0xBE, &msg, 1, 209, 6);
  
  uint16_t len = mavlink_msg_to_send_buffer(buf, &msg);
  SerialMAV.write(buf, len);  
}

void pixhawk_set_alt_hold_mode() {
  
  mavlink_message_t msg;
  uint8_t buf[MAVLINK_MAX_PACKET_LEN];
  
  mavlink_msg_set_mode_pack(0xFF, 0xBE, &msg, 1, 209, 2);
  
  uint16_t len = mavlink_msg_to_send_buffer(buf, &msg);
  SerialMAV.write(buf, len);  
}

/* This function gets message from the APM and interprete for Mavlink common messages */
void read_pixhawk_incoming_messages() {
  
  mavlink_message_t msg;
  mavlink_status_t status;

  //Checks if drone is connected
  while(SerialMAV.available()) {
    
    uint8_t c = SerialMAV.read();
        
    // Try to get a new message
    if(mavlink_parse_char(MAVLINK_COMM_0, c, &msg, &status)) {
      
      switch(msg.msgid) {

        case MAVLINK_MSG_ID_HEARTBEAT:  // #0: Heartbeat
          {
            // E.g. read GCS heartbeat and go into
            // comm lost mode if timer times out
            //Serial.println("MAVLINK_MSG_ID_HEARTBEAT");
            mavlink_heartbeat_t hb;
            mavlink_msg_heartbeat_decode(&msg, &hb);

            Serial.print("BASE_MODE: ");
            Serial.println(hb.base_mode);
            Serial.print("STATUS: ");
            
            switch(hb.base_mode) {
              case 81:
              ArmedState = "DISARMED";
              Serial.println("DISARMED");
              break;
              case 89:
              ArmedState = "DISARMED";
              Serial.println("DISARMED");              
              break;
              case 217:
              ArmedState = "ARMED";
              Serial.println("ARMED");              
              break;
              case 209:
              ArmedState = "ARMED";
              Serial.println("ARMED");              
              break;
            }
            
            Serial.print("CUSTOM_MODE: ");
            Serial.println(hb.custom_mode);
            Serial.print("MODE: ");
            
            switch(hb.custom_mode) {
              case 0:
                ModeState = "STABILIZE";
                Serial.println("STABILIZE");
              break;
              case 3:
                ModeState = "AUTO";
                Serial.println("AUTO");
              break;
              case 6:
                ModeState = "RTL";
                Serial.println("RTL");
              break;              
              default:
                Serial.println("UNKNOWN MODE");
              break;
            }
            
          }
          break;

          case MAVLINK_MSG_ID_COMMAND_ACK: // #77
          {
            mavlink_command_ack_t cmd_ack;
            mavlink_msg_command_ack_decode(&msg, &cmd_ack);

            Serial.println("--------------------------------");
            Serial.println("MAVLINK_MSG_ID_COMMAND_ACK #77");
            
            Serial.print("command = ");
            Serial.println(cmd_ack.command);
            Serial.print("result = ");
            Serial.println(cmd_ack.result);
            Serial.println("--------------------------------");
            
            StatusMessage = "";                     

            if(cmd_ack.command == 11 && cmd_ack.result == 3){
              StatusMessage = "Flight mode change failed.";
            }
            else if(cmd_ack.command == 400 && cmd_ack.result == 0){
              StatusMessage = "";
            }
            else {
              StatusMessage.concat("CMD_ACK: CMD:");
              StatusMessage.concat(cmd_ack.command);
              StatusMessage.concat(" RSLT:");
              StatusMessage.concat(cmd_ack.result);
            }

            read_pixhawk_incoming_messages_finish = true;
          }
          
          break;
          
      }
    }
  }
}
