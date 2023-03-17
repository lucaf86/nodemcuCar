#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <ESPAsyncWebServer.h>
#include <AsyncElegantOTA.h>

#include <FS.h>
#include <SPIFFSEditor.h>

#include "wifimanager.h"

#if not defined(D6)
#define D0 16
#define D1 5
#define D2 4
#define D3 0
#define D4 2
#define D5 14
#define D6 12
#define D7 13
#define D8 15
#define D9 3
#define D10 1
#endif

#define JOY_ACTIVE_ZONE 25

#define motorPin1_1 D1
#define motorPin1_2 D2
#define motorPin2_1 D5      //D3
#define motorPin2_2 D6      //D4
#define loaderPIN D7        //D6
#define loaderBucketPin D3  //D7

//#define ISR_SERVO_LIBRARY
//#define SOFT_SERVO_LIBRARY  // not working properly!!!
#define SERVO_EASING_LIBRARY


#if defined(ISR_SERVO_LIBRARY)
#ifndef ESP8266
  #error This code is designed to run on ESP8266 platform! Please check your Tools->Board setting.
#endif

#define TIMER_INTERRUPT_DEBUG       0
#define ISR_SERVO_DEBUG             -1

#include "ESP8266_ISR_Servo.h"
#elif defined(SOFT_SERVO_LIBRARY)
#include "SoftServo.h" 

#elif defined(SERVO_EASING_LIBRARY)
/*
 * Specify which easings types should be available.
 * If no easing is defined, all easings are active.
 * This must be done before the #include "ServoEasing.hpp"
 */
//#define ENABLE_EASE_QUADRATIC
//#define ENABLE_EASE_CUBIC
//#define ENABLE_EASE_QUARTIC
//#define ENABLE_EASE_SINE
//#define ENABLE_EASE_CIRCULAR
//#define ENABLE_EASE_BACK
//#define ENABLE_EASE_ELASTIC
//#define ENABLE_EASE_BOUNCE
//#define ENABLE_EASE_PRECISION
//#define ENABLE_EASE_USER

  #include "ServoEasing.hpp"

#else
#include <Servo.h>
#endif


#define FILESYS SPIFFS
#define FILECMD_START
#define FILECMD_END


int LValue, RValue;

const char *MyHostname = "skidloader";  //dhcp hostname (if your router supports it) and Domain name for the mDNS responder

// wifi manager para
// Search for parameter in HTTP POST request
const char* PARAM_INPUT_1 = "ssid";
const char* PARAM_INPUT_2 = "pass";
const char* PARAM_INPUT_3 = "ip";
const char* PARAM_INPUT_4 = "gateway";

//Variables to save values from HTML form
String wifiManager_ssid;
String wifiManager_pass;
String wifiManager_ip;
String wifiManager_gateway;

// File paths to save input values permanently
const char* ssidPath = "/ssid.txt";
const char* passPath = "/pass.txt";
const char* ipPath = "/ip.txt";
const char* gatewayPath = "/gateway.txt";

IPAddress localIP;
//IPAddress localIP(192, 168, 1, 200); // hardcoded

// Set your Gateway IP address
IPAddress localGateway;
//IPAddress localGateway(192, 168, 1, 1); //hardcoded
IPAddress subnet(255, 255, 255, 0);

#if defined(ISR_SERVO_LIBRARY)
int loaderArm  = -1;
int loaderBucket  = -1;
#elif defined(SOFT_SERVO_LIBRARY)
SoftServo loaderArm, loaderBucket;
#elif defined(SERVO_EASING_LIBRARY)
ServoEasing loaderArm;
ServoEasing loaderBucket;
#else
Servo loaderArm, loaderBucket; 
#endif

#define TRACE_BEGIN() Serial.begin(115200)
#define TRACELN //Serial.println
#define TRACE   //Serial.print
#define TRACEF  //Serial.printf

// SKETCH BEGIN
AsyncWebServer webserver(80);
AsyncWebSocket ws("/ws");
//AsyncEventSource events("/events");
AsyncWebSocketClient* wsClient;





#define MAX_LOADER_ANGLE 70
#define STOP 0
#define UP 1
#define DOWN 2
#define LOADER_ARM_MOVE_DELAY_ms 30
#define ARM_MOVE_THR 60
int loaderArmAngle = 0;
int loaderArmMove = 0;

#if defined(ISR_SERVO_LIBRARY)
#define loaderArm_up() {if(loaderArmAngle < MAX_LOADER_ANGLE) { loaderArmAngle++; if(loaderArmAngle < 200){ISR_Servo.setPosition(loaderArm, loaderArmAngle);}else{ISR_Servo.setPulseWidth(loaderArm, loaderArmAngle);}}}
#define loaderArm_down() {if(loaderArmAngle > 0) { loaderArmAngle--; if(loaderArmAngle < 200){ISR_Servo.setPosition(loaderArm, loaderArmAngle);}else{ISR_Servo.setPulseWidth(loaderArm, loaderArmAngle);}}}
#elif defined(SOFT_SERVO_LIBRARY)
#define loaderArm_up() {if(loaderArmAngle < MAX_LOADER_ANGLE) { loaderArmAngle++; loaderArm.write(loaderArmAngle); loaderArm.refresh();}}
#define loaderArm_down() {if(loaderArmAngle > 0) { loaderArmAngle--; loaderArm.write(loaderArmAngle);loaderArm.refresh();}}
#else
#define loaderArm_up() {if(loaderArmAngle < MAX_LOADER_ANGLE) { loaderArmAngle++; loaderArm.write(loaderArmAngle);}}
#define loaderArm_down() {if(loaderArmAngle > 0) { loaderArmAngle--; loaderArm.write(loaderArmAngle);}}
#endif

#if defined(ISR_SERVO_LIBRARY)
    #define loaderArm_set(pos)  {if(pos < 200) {ISR_Servo.setPosition(loaderArm, pos);} else {ISR_Servo.setPulseWidth(loaderArm, pos);}}
    #define loaderBucket_set(pos)  {if(pos < 200){ISR_Servo.setPosition(loaderBucket, pos);}else{ISR_Servo.setPulseWidth(loaderBucket, pos);}}
#elif defined(SOFT_SERVO_LIBRARY)
    #define loaderArm_set(pos)  {loaderArm.write(pos);loaderArm.refresh();}
    #define loaderBucket_set(pos)  {loaderBucket.write(pos);loaderBucket.refresh();}
#else
    #define loaderArm_set(pos)  loaderArm.write(pos);
    #define loaderBucket_set(pos)  loaderBucket.write(pos);
#endif

int loaderArmNewPos = 2000;

// handle http messages
void handle_message(String& msg) {
    TRACE("ws msg received: ");
  TRACELN(msg.c_str());
  int commaIndex = msg.indexOf(',');
  String cmd = msg.substring(0, commaIndex);
  //TRACELN(cmd);
  if (cmd.equals("#L")) {
    int angle = msg.substring(commaIndex + 1).toInt();
    loaderArmNewPos = angle;
    //loaderArm.startEaseTo(loaderArmNewPos, 150);
  //  loaderArm_set(angle);
    //TRACE("Loader angle: ");
    //TRACELN(angle);
  }
  else if(cmd.equals("#B")) {
    int angle = msg.substring(commaIndex + 1).toInt();
    loaderBucket_set(angle);
    //TRACE("Bucket angle: ");
    //TRACELN(angle);
  }
  else if (cmd.equals("#M")) {
    String sub_msg = msg.substring(commaIndex + 1);
    commaIndex = sub_msg.indexOf(',');

    //swap Left and righ, why ????
    RValue = sub_msg.substring(0, commaIndex).toInt();   // range +255 ... -255
    LValue = sub_msg.substring(commaIndex + 1).toInt();  // range +255 ... -255
     TRACE("M1: ");
    TRACE(LValue);
    TRACE(" M2: ");
    TRACELN(RValue);
 }
}

void onWsEvent(AsyncWebSocket * server, AsyncWebSocketClient * client, AwsEventType type, void * arg, uint8_t *data, size_t len){
  if(type == WS_EVT_CONNECT){
    wsClient = client;
  }
  else if(type == WS_EVT_DISCONNECT){
    wsClient = nullptr;
  }
  else if(type == WS_EVT_PONG){
    //pong message was received (in response to a ping request maybe)
    TRACEF("ws[%s][%u] pong[%u]: %s\n", server->url(), client->id(), len, (len)?(char*)data:"");
  }
  else if(type == WS_EVT_DATA){
    //data packet
    AwsFrameInfo * info = (AwsFrameInfo*)arg;
    if(info->final && info->index == 0 && info->len == len){
      //the whole message is in a single frame and we got all of it's data
      TRACEF("ws[%s][%u] %s-message[%llu]: ", server->url(), client->id(), (info->opcode == WS_TEXT)?"text":"binary", info->len);
      if(info->opcode == WS_TEXT){
        data[len] = 0;
        TRACEF("%s\n", (char*)data);
        //std::string msg((char*)data);
        String msg((char*)data);
        handle_message(msg);
        
      } else {
        for(size_t i=0; i < info->len; i++){
          TRACEF("%02x ", data[i]);
        }
        TRACEF("\n");
      }
      if(info->opcode == WS_TEXT){ 
        //client->text("I got your text message");
      }else {
        //client->binary("I got your binary message");
      }
    } else {
      //message is comprised of multiple frames or the frame is split into multiple packets
      if(info->index == 0){
        if(info->num == 0)
          TRACEF("ws[%s][%u] %s-message start\n", server->url(), client->id(), (info->message_opcode == WS_TEXT)?"text":"binary");
        TRACEF("ws[%s][%u] frame[%u] start[%llu]\n", server->url(), client->id(), info->num, info->len);
      }

      TRACEF("ws[%s][%u] frame[%u] %s[%llu - %llu]: ", server->url(), client->id(), info->num, (info->message_opcode == WS_TEXT)?"text":"binary", info->index, info->index + len);
      if(info->message_opcode == WS_TEXT){
        data[len] = 0;
        TRACEF("%s\n", (char*)data);
      } else {
        for(size_t i=0; i < len; i++){
          TRACEF("%02x ", data[i]);
        }
        TRACEF("\n");
      }

      if((info->index + len) == info->len){
        TRACEF("ws[%s][%u] frame[%u] end[%llu]\n", server->url(), client->id(), info->num, info->len);
        if(info->final){
          TRACEF("ws[%s][%u] %s-message end\n", server->url(), client->id(), (info->message_opcode == WS_TEXT)?"text":"binary");
          if(info->message_opcode == WS_TEXT){
            //client->text("I got your text message");
          }else {
            //client->binary("I got your binary message");
          }
        }
      }
    }
  }
}


const char* ssid = "D_Guest";
const char* password = "politecnico";
const char * hostName = "skidloader";
const char* http_username = "admin";
const char* http_password = "admin";




// Initialize WiFi
bool initWiFi(const String& _ssid, const String& _pass, const String& _ip, const String& _gateway) {
  if( _ssid.isEmpty() && _ip.isEmpty()){
    TRACELN(_ssid);
    TRACELN(_ip);
    TRACELN("Undefined SSID or IP address.");
    return false;
  }

  //WiFi.config(INADDR_NONE, INADDR_NONE, INADDR_NONE);
  //erase WIFI stored credential
  WiFi.disconnect(true);
  ESP.eraseConfig();

  WiFi.mode(WIFI_STA);
  localIP.fromString( _ip.c_str());
  localGateway.fromString( _gateway.c_str());

  if(!_ip.isEmpty() && !_gateway.isEmpty()) {
    if (!WiFi.config(localIP, localGateway, subnet)){
       TRACELN("STA Failed to configure");
      return false;
    }
  }
  WiFi.hostname(MyHostname);
  WiFi.begin( _ssid.c_str(),  _pass.c_str());
  TRACELN("Connecting to WiFi...");

  unsigned long currentMillis = millis();
  unsigned long previousMillis = currentMillis;
  const long interval = 10000;

  while(WiFi.status() != WL_CONNECTED) {
   TRACELN/* Serial.println*/("... waiting WiFi ...");
    delay(100);
    currentMillis = millis();
    if (currentMillis - previousMillis >= interval) {
      TRACELN("Failed to connect.");
      return false;
    }
  }
 TRACELN("Connected to WiFi");

  TRACELN(WiFi.localIP());
  return true;
}

void UpdateSlider(AsyncWebServerRequest *request) {

  // many I hate strings, but wifi lib uses them...
  String t_state = request->arg("VALUE");

  // conver the string sent from the web page to an int
  int FanSpeed = t_state.toInt();
  TRACE("UpdateSlider"); TRACELN(FanSpeed);
  // YOU MUST SEND SOMETHING BACK TO THE WEB PAGE--BASICALLY TO KEEP IT LIVE

  // option 1: send no information back, but at least keep the page live
  // just send nothing back
  // server.send(200, "text/plain", ""); //Send web page

  // option 2: send something back immediately, maybe a pass/fail indication, maybe a measured value
  // here is how you send data back immediately and NOT through the general XML page update code
  // my simple example guesses at fan speed--ideally measure it and send back real data
  // i avoid strings at all caost, hence all the code to start with "" in the buffer and build a
  // simple piece of data
  int FanRPM = map(FanSpeed, 0, 255, 0, 2400);
  // just some buffer holder for char operations
  char buf[32];
  strcpy(buf, "");
  sprintf(buf, "%d", FanRPM);
  sprintf(buf, buf);
  // now send it back
  request->send(200, "text/plain", buf); //Send web page

}

String processor(const String& var){
  /*getSensorReadings();
  //TRACELN(var);
  if(var == "TEMPERATURE"){
    return String(temperature);
  }
  else if(var == "HUMIDITY"){
    return String(humidity);
  }
  else if(var == "PRESSURE"){
    return String(pressure);
  }*/
  TRACE("processor: ");
  TRACELN(var);
}



void startFILESYS() {    // Start the FILESYS and list all contents
  if (!FILESYS.begin())  // Start the SPI Flash File System (FILESYS)
  {
    TRACELN("FILESYS begin error!");
    if (FILESYS.format()) {
      FILESYS.begin();
    } else {
      TRACELN(F("No FILESYS found"));
    }
  }
  TRACELN("FILESYS started. Contents:");
  {
    Dir dir = FILESYS.openDir("/");
    while (dir.next()) {  // List the file system contents
      String fileName = dir.fileName();
      size_t fileSize = dir.fileSize();
      TRACEF("\tFS File: %s, size: %s\r\n", fileName.c_str(), formatBytes(fileSize).c_str());
    }
    TRACEF("\n");
  }
}

String getContentType(AsyncWebServerRequest *request, String filename) {
  /* if(webserver.hasArg("download")) return "application/octet-stream"; */
  if (request->hasArg("download")) return "application/octet-stream";
  else if (filename.endsWith(".htm")) return "text/html";
  else if (filename.endsWith(".html")) return "text/html";
  else if (filename.endsWith(".css")) return "text/css";
  else if (filename.endsWith(".js")) return "application/javascript";
  else if (filename.endsWith(".png")) return "image/png";
  else if (filename.endsWith(".gif")) return "image/gif";
  else if (filename.endsWith(".jpg")) return "image/jpeg";
  else if (filename.endsWith(".ico")) return "image/x-icon";
  else if (filename.endsWith(".xml")) return "text/xml";
  else if (filename.endsWith(".pdf")) return "application/x-pdf";
  else if (filename.endsWith(".zip")) return "application/x-zip";
  else if (filename.endsWith(".gz")) return "application/x-gzip";
  return "text/plain";
}

bool handleFileRead(AsyncWebServerRequest *request, String path) {  // send the right file to the client (if it exists)
  //String path = request->url();
  TRACELN("handleFileRead: " + path);
  if (path.endsWith("/")) path += "index.html";        // If a folder is requested, send the index file
  String contentType = getContentType(request, path);  // Get the MIME type
  String pathWithGz = path + ".gz";
  if (FILESYS.exists(pathWithGz) || FILESYS.exists(path)) {  // If the file exists, either as a compressed archive, or normal
    if (FILESYS.exists(pathWithGz)) {                        // If there's a compressed version available
      path += ".gz";                                         // Use the compressed verion
    }
    File file = FILESYS.open(path, "r");  // Open the file
    // size_t sent = webserver.streamFile(file, contentType);    // Send it to the client
    // request->send(FILESYS, path, String(), true);
    //Send file as text
    request->send(FILESYS, path, contentType);
    //Download file
    //request->send(FILESYS, "/index.htm", String(), true);
    file.close();  // Close the file again
    TRACELN(String("\tSent file: ") + path);
    return true;
  }
  TRACELN(String("\tFile Not Found: ") + path);  // If the file doesn't exist, return false
  return false;
}

// Read File from SPIFFS
String readFile(const char * path){
  TRACEF("Reading file: %s\r\n", path);

  File file = FILESYS.open(path,"r");
  if(!file || file.isDirectory()){
    TRACELN("- failed to open file for reading");
    return String();
  }
  
  String fileContent;
  while(file.available()){
    fileContent = file.readStringUntil('\n');
    break;     
  }
  return fileContent;
}

// Write file to SPIFFS
void writeFile(const char * path, const char * message){
  TRACEF("Writing file: %s\r\n", path);

  File file = FILESYS.open(path, "w");
  if(!file){
    TRACELN("- failed to open file for writing");
    return;
  }
  if(file.print(message)){
    TRACELN("- file written");
  } else {
    TRACELN("- frite failed");
  }
}


String formatBytes(size_t bytes) {  // convert sizes in bytes to KB and MB
  if (bytes < 1024) {
    return String(bytes) + "B";
  } else if (bytes < (1024 * 1024)) {
    return String(bytes / 1024.0) + "KB";
  } else if (bytes < (1024 * 1024 * 1024)) {
    return String(bytes / 1024.0 / 1024.0) + "MB";
  }
}



void setup(){
  TRACE_BEGIN(); //Serial.begin(115200);
  Serial.setDebugOutput(true);

    pinMode(motorPin1_1, OUTPUT);
  pinMode(motorPin1_2, OUTPUT);

  pinMode(motorPin2_1, OUTPUT);
  pinMode(motorPin2_2, OUTPUT);

  analogWrite(motorPin1_1, 0);
  analogWrite(motorPin1_2, 0);

  analogWrite(motorPin2_1, 0);
  analogWrite(motorPin2_2, 0);

  analogWriteFreq(200);  // PWM frequency in Hz
  analogWriteRange(255);

#if defined(ISR_SERVO_LIBRARY)
  loaderArm = ISR_Servo.setupServo(loaderPIN, 544, 2400);
  ISR_Servo.setPosition(loaderArm, 0);
  loaderBucket = ISR_Servo.setupServo(loaderBucketPin, 500, 2500);
  ISR_Servo.setPosition(loaderBucket, 90);
#elif defined(SOFT_SERVO_LIBRARY)
  loaderArm.attach(loaderPIN, 544, 2400, 0);
  loaderArm.write(0);
  loaderArm.refresh();

  loaderBucket.attach(loaderBucketPin, 500, 2500, 90);
  loaderBucket.write(90);
  loaderBucket.refresh();
#else

  loaderArm.attach(loaderPIN, 544, 2400);

  //loaderArm.setEaseTo(EASE_QUADRATIC_IN_OUT);
//  loaderArm.setEaseTo(EASE_CUBIC_IN);
//loaderArm.setEaseTo(EASE_QUARTIC_IN);
loaderArm.setEaseTo(EASE_CUBIC_IN_OUT);

  loaderArm.write(2000);

  loaderBucket.attach(loaderBucketPin, 500, 2500);
  loaderBucket.write(90);
#endif


  startFILESYS();


#ifdef STATIC_STA_WIFI
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  if (WiFi.waitForConnectResult() != WL_CONNECTED) {
    Serial.printf("STA: Failed!\n");
    WiFi.disconnect(false);
    delay(1000);
    WiFi.begin(ssid, password);
  }
#else

    // Load values saved in SPIFFS
  wifiManager_ssid.clear();
  wifiManager_ssid = readFile( ssidPath);
  wifiManager_pass.clear();
  wifiManager_pass = readFile( passPath);
  wifiManager_ip.clear();
  wifiManager_ip = readFile( ipPath);
  wifiManager_gateway.clear();
  wifiManager_gateway = readFile ( gatewayPath);
  TRACELN(wifiManager_ssid);
  TRACELN(wifiManager_pass);
  TRACELN(wifiManager_ip);
  TRACELN(wifiManager_gateway);

  WiFi.hostname(MyHostname);

  if(initWiFi(wifiManager_ssid, wifiManager_pass, wifiManager_ip, wifiManager_gateway)) {
      TRACELN("initWiFi done SUCCESS");
    // Route for root / web page
    //server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
    //  request->send(FILESYS, "/index.html", "text/html", false, processor);
    //});
    //server.serveStatic("/", FILESYS, "/");
  }
  else {
    // Connect to Wi-Fi network with SSID and password
    TRACELN("Setting AP (Access Point)");
    // NULL sets an open Access Point
    WiFi.mode(WIFI_AP);
    WiFi.softAP("ESP-WIFI-MANAGER", NULL);

    IPAddress IP = WiFi.softAPIP();
    TRACE("AP IP address: ");
    TRACELN(IP); 

    // Web Server Root URL
    webserver.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
      TRACELN("webser on root HTTP_GET");
      //request->send(FILESYS, "/wifimanager.html", "text/html");
      //if (!handleFileRead(request, "/wifimanager.html")) request->send(404, "text/plain", "wifimanager.html NotFound");
      request->send_P(200, "text/html", wifimanager_html, processor);
    });
    
    webserver.serveStatic("/", FILESYS, "/");
    
    webserver.on("/", HTTP_POST, [](AsyncWebServerRequest *request) {
      int params = request->params();
      for(int i=0;i<params;i++){
        AsyncWebParameter* p = request->getParam(i);
        if(p->isPost()){
          // HTTP POST ssid value
          if (p->name() == PARAM_INPUT_1) {
            wifiManager_ssid = p->value().c_str();
            TRACE("SSID set to: ");
            TRACELN(wifiManager_ssid);
            // Write file to save value
            writeFile( ssidPath, wifiManager_ssid.c_str());
          }
          // HTTP POST pass value
          if (p->name() == PARAM_INPUT_2) {
            wifiManager_pass = p->value().c_str();
            TRACE("Password set to: ");
            TRACELN(wifiManager_pass);
            // Write file to save value
            writeFile( passPath, wifiManager_pass.c_str());
          }
          // HTTP POST ip value
          if (p->name() == PARAM_INPUT_3) {
            wifiManager_ip = p->value().c_str();
            TRACE("IP Address set to: ");
            TRACELN(wifiManager_ip);
            // Write file to save value
            writeFile( ipPath, wifiManager_ip.c_str());
          }
          // HTTP POST gateway value
          if (p->name() == PARAM_INPUT_4) {
            wifiManager_gateway = p->value().c_str();
            TRACE("Gateway set to: ");
            TRACELN(wifiManager_gateway);
            // Write file to save value
            writeFile( gatewayPath, wifiManager_gateway.c_str());
          }
          //TRACEF("POST[%s]: %s\n", p->name().c_str(), p->value().c_str());
        }
      }
      request->send(200, "text/plain", "Done. ESP will restart, connect to your router and go to IP address: " + wifiManager_ip);
      delay(3000);
      ESP.restart();
    });

    AsyncWebHandler *editHandler = (nullptr);
    if (editHandler != nullptr)
    {
        webserver.removeHandler(editHandler);
    }
    editHandler = &webserver.addHandler(new SPIFFSEditor("", "", FILESYS));

    AsyncElegantOTA.begin(&webserver);  // Start ElegantOTA

    webserver.begin();
    return;
  }
#endif

  TRACELN(WiFi.localIP());


  ws.onEvent(onWsEvent);
  webserver.addHandler(&ws);

  //events.onConnect([](AsyncEventSourceClient *client){
  //  client->send("hello!",NULL,millis(),1000);
  //});
  //server.addHandler(&events);

  //webserver.addHandler(new SPIFFSEditor(http_username,http_password));
  AsyncWebHandler *editHandler = (nullptr);
  if (editHandler != nullptr) webserver.removeHandler(editHandler);
  editHandler = &webserver.addHandler(new SPIFFSEditor("", "", FILESYS));




  webserver.on("/heap", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(200, "text/plain", String(ESP.getFreeHeap()));
  });

  // Cache responses for 10 minutes (600 seconds)
 // webserver.serveStatic("/", FILESYS, "/").setCacheControl("max-age=600");
 //webserver.serveStatic("/", FILESYS, "/").setDefaultFile("index.html");
 //webserver.serveStatic("/", FILESYS, "/");
  webserver.serveStatic("/", SPIFFS, "/").setDefaultFile("index.html");

// Route for root / web page
//  webserver.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
//    // request->send(FILESYS, "/index.html", String(), false, processor);
//    if (!handleFileRead(request, "/index.html")) request->send(404, "text/plain", "index.html NotFound");
//  });


  webserver.on("/UPDATE_SLIDER", [](AsyncWebServerRequest *request) {
    UpdateSlider(request);
  });

  AsyncElegantOTA.begin(&webserver);  // Start ElegantOTA





#if 1
  webserver.onNotFound([](AsyncWebServerRequest *request) {
    TRACE("onNotFound ");
    TRACELN(request->url().c_str());
    if (!handleFileRead(request, request->url())) {  // check if the file exists in the flash memory (FILESYS), if so, send it
      request->send(404, "text/plain", "404: File Not Found");
    }
  });

#else

  webserver.onNotFound([](AsyncWebServerRequest *request){
    Serial.printf("NOT_FOUND: ");
    if(request->method() == HTTP_GET)
      Serial.printf("GET");
    else if(request->method() == HTTP_POST)
      Serial.printf("POST");
    else if(request->method() == HTTP_DELETE)
      Serial.printf("DELETE");
    else if(request->method() == HTTP_PUT)
      Serial.printf("PUT");
    else if(request->method() == HTTP_PATCH)
      Serial.printf("PATCH");
    else if(request->method() == HTTP_HEAD)
      Serial.printf("HEAD");
    else if(request->method() == HTTP_OPTIONS)
      Serial.printf("OPTIONS");
    else
      Serial.printf("UNKNOWN");
    Serial.printf(" http://%s%s\n", request->host().c_str(), request->url().c_str());

    if(request->contentLength()){
      Serial.printf("_CONTENT_TYPE: %s\n", request->contentType().c_str());
      Serial.printf("_CONTENT_LENGTH: %u\n", request->contentLength());
    }

    int headers = request->headers();
    int i;
    for(i=0;i<headers;i++){
      AsyncWebHeader* h = request->getHeader(i);
      Serial.printf("_HEADER[%s]: %s\n", h->name().c_str(), h->value().c_str());
    }

    int params = request->params();
    for(i=0;i<params;i++){
      AsyncWebParameter* p = request->getParam(i);
      if(p->isFile()){
        Serial.printf("_FILE[%s]: %s, size: %u\n", p->name().c_str(), p->value().c_str(), p->size());
      } else if(p->isPost()){
        Serial.printf("_POST[%s]: %s\n", p->name().c_str(), p->value().c_str());
      } else {
        Serial.printf("_GET[%s]: %s\n", p->name().c_str(), p->value().c_str());
      }
    }

    request->send(404);
  });
  webserver.onFileUpload([](AsyncWebServerRequest *request, const String& filename, size_t index, uint8_t *data, size_t len, bool final){
    if(!index)
      Serial.printf("UploadStart: %s\n", filename.c_str());
    Serial.printf("%s", (const char*)data);
    if(final)
      Serial.printf("UploadEnd: %s (%u)\n", filename.c_str(), index+len);
  });
  webserver.onRequestBody([](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total){
    if(!index)
      Serial.printf("BodyStart: %u\n", total);
    Serial.printf("%s", (const char*)data);
    if(index + len == total)
      Serial.printf("BodyEnd: %u\n", total);
  });
#endif

  webserver.begin();
}


void loop(){
  static int loaderArmPos = 2000;
  ws.cleanupClients();
  if (loaderArmNewPos != loaderArmPos)
  {
    //loaderArm_set(loaderArmNewPos);
    loaderArm.startEaseTo(loaderArmNewPos, 150, START_UPDATE_BY_INTERRUPT);
    loaderArmPos = loaderArmNewPos;
  }
  //delay(10);
}
