#include <ESP8266SSDP.h>

#include <Arduino.h>
#include "Arduino.h"
#include <ArduinoWebsockets.h>
#include <ESP8266WiFi.h>
#include <ESPAsyncWebServer.h>
#include <AsyncElegantOTA.h>
#include <ESP8266mDNS.h>
#include <ESP8266SSDP.h>
#include <StreamString.h>
#include <FS.h>
//#include <ESP8266WebServer.h>
//#include <ESP8266HTTPUpdateServer.h>
#include <SPIFFSEditor.h>

#define TRACELN Serial.println
#define TRACE Serial.print

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
const char *ssid = "CASA";  //Enter SSID
//const char* ssid = "XT1562 3164"; //Enter SSID
const char *pw = "politecnico";  //Enter Password
#else
const char *ssid = "CASA";       //Enter SSID
const char *pw = "politecnico";  //Enter Password
#endif

#define JOY_ACTIVE_ZONE 25

#define motorPin1_1 D1
#define motorPin1_2 D2
#define motorPin2_1 D5      //D3
#define motorPin2_2 D6      //D4
#define loaderPIN D7        //D6
#define loaderBucketPin D3  //D7

static const char *ssdpTemplate =
  "<?xml version=\"1.0\"?>"
  "<root xmlns=\"urn:schemas-upnp-org:device-1-0\">"
  "<specVersion>"
  "<major>1</major>"
  "<minor>0</minor>"
  "</specVersion>"
  "<URLBase>http://%u.%u.%u.%u/</URLBase>"
  "<device>"
  "<deviceType>upnp:rootdevice</deviceType>"
  "<friendlyName>%s</friendlyName>"
  "<presentationURL>index.html</presentationURL>"
  "<serialNumber>%u</serialNumber>"
  "<modelName>%s</modelName>"
  "<modelNumber>%s</modelNumber>"
  "<modelURL>http://www.espressif.com</modelURL>"
  "<manufacturer>Espressif Systems</manufacturer>"
  "<manufacturerURL>http://www.espressif.com</manufacturerURL>"
  "<UDN>uuid:38323636-4558-4dda-9188-cda0e6%02x%02x%02x</UDN>"
  "</device>"
  "</root>\r\n"
  "\r\n";

using namespace websockets;
WebsocketsServer wsServer;
AsyncWebServer webserver(80);

//ESP8266WebServer auxServer(80);

const char *MyHostname = "skidloader";  //dhcp hostname (if your router supports it) and Domain name for the mDNS responder
//SSDP properties
const char *modelName = "ESP8266";
const char *modelNumber = "929000226503";

int LValue, RValue, commaIndex;

/** ESP32 robot tank with wifi and one joystick web control sketch. 
    Based on SMARS modular robot project using esp32 and tb6612.
    https://www.thingiverse.com/thing:2662828

    for complete complete program: https://github.com/nkmakes/SMARS-esp32

    Made by nkmakes.github.io, August 2020.

    -----------------------------------------
    Camera stream based upon:
    Based on Neil Kolban example for IDF: https://github.com/nkolban/esp32-snippets/blob/master/cpp_utils/tests/BLE%20Tests/SampleNotify.cpp
    Ported to Arduino ESP32 by Evandro Copercini
    Adapted by Manos Zeakis for ESP32 and TB6612FNG
*/












/// ### aux web server for spiffs editor ###

#define FILESYS SPIFFS
#define FILECMD_START
#define FILECMD_END

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


//holds the current upload
File fsUploadFile;

// void handleFileUpload(AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool final) {
//   if (request->url() != "/edit") return;
//   if (!index) {
//     Serial.printf("UploadStart: %s\n", filename.c_str());
//     if (!filename.startsWith("/")) filename = "/" + filename;
//     fsUploadFile = FILESYS.open(filename, "w");
//   }

//   if (fsUploadFile) {
//     for (size_t i = 0; i < len; i++) {
//       fsUploadFile.write(data[i]);
//     }
//   }
//   if (final) {
//     Serial.printf("UploadEnd: %s, %u B\n", filename.c_str(), index + len);
//     if (fsUploadFile) {
//       fsUploadFile.close();
//     }
//   }
// }


// void handleFileDelete(AsyncWebServerRequest *request) {
//   if (request->args() == 0) return request->send(500, "text/plain", "BAD ARGS");
//   size_t argIdx = 0;
//   String path = request->arg(argIdx);
//   Serial.printf_P(PSTR("handleFileDelete: %s\r\n"), path.c_str());
//   if (path == "/")
//     return request->send(500, "text/plain", "BAD PATH");
//   FILECMD_START
//   if (!FILESYS.exists(path))
//     return request->send(404, "text/plain", "FileNotFound");
//   FILESYS.remove(path);
//   FILECMD_END
//   request->send(200, "text/plain", "");
//   path = String();
// }

// void handleFileCreate(AsyncWebServerRequest *request) {
//   String status = "";
//   int ret;
//   if (request->args() == 0)
//     return request->send(500, "text/plain", "BAD ARGS");
//   size_t argIdx = 0;
//   String path = request->arg(argIdx);
//   Serial.printf_P(PSTR("handleFileCreate: %s\r\n"), path.c_str());
//   if (path == "/")
//     return request->send(500, "text/plain", "BAD PATH");
//   FILECMD_START;
//   if (FILESYS.exists(path)) {
//     status = "FILE EXISTS";
//     ret = 500;
//   } else {
//     File file = FILESYS.open(path, "w");
//     if (file) {
//       file.close();
//       ret = 200;
//     } else {
//       status = "CREATE FAILED";
//       ret = 500;
//     }
//   }
//   FILECMD_END;
//   request->send(ret, "text/plain", status);
// }

// void handleFileList(AsyncWebServerRequest *request) {
//   if (!request->hasArg("dir")) {
//     request->send(500, "text/plain", "BAD ARGS");
//     return;
//   }

//   String path = request->arg("dir");
//   Serial.printf_P(PSTR("handleFileList: %s\r\n"), path.c_str());
//   FILECMD_START
//   Dir dir = FILESYS.openDir(path);
//   path = String();

//   String output = "[";
//   while (dir.next()) {
//     File entry = dir.openFile("r");
//     if (output != "[") output += ',';
//     bool isDir = false;
//     output += "{\"type\":\"";
//     output += (isDir) ? "dir" : "file";
//     output += "\",\"name\":\"";
//     output += String(entry.name()).substring(1);
//     output += "\"}";
//     entry.close();
//   }
//   output += "]";
//   FILECMD_END
//   request->send(200, "text/json", output);
// }

void handleFileSysFormat(AsyncWebServerRequest *request) {
  Serial.println(F("format start"));
  FILECMD_START
  FILESYS.format();
  FILECMD_END
  Serial.println(F("format complete"));
  request->send(200, "text/json", "format complete");
}

/// ### aux web server for spiffs editor ###  -  END








String formatBytes(size_t bytes) {  // convert sizes in bytes to KB and MB
  if (bytes < 1024) {
    return String(bytes) + "B";
  } else if (bytes < (1024 * 1024)) {
    return String(bytes / 1024.0) + "KB";
  } else if (bytes < (1024 * 1024 * 1024)) {
    return String(bytes / 1024.0 / 1024.0) + "MB";
  }
}


void startFILESYS() {     // Start the FILESYS and list all contents
  if (!FILESYS.begin())  // Start the SPI Flash File System (FILESYS)
  {
    Serial.println("FILESYS begin error!");
    if (FILESYS.format()) {
      FILESYS.begin();
    } else {
      Serial.println(F("No FILESYS found"));
    }
  }
  Serial.println("FILESYS started. Contents:");
  {
    Dir dir = FILESYS.openDir("/");
    while (dir.next()) {  // List the file system contents
      String fileName = dir.fileName();
      size_t fileSize = dir.fileSize();
      Serial.printf("\tFS File: %s, size: %s\r\n", fileName.c_str(), formatBytes(fileSize).c_str());
    }
    Serial.printf("\n");
  }
}

bool handleFileRead(AsyncWebServerRequest *request, String path) {  // send the right file to the client (if it exists)
  //String path = request->url();
  Serial.println("handleFileRead: " + path);
  if (path.endsWith("/")) path += "index.html";        // If a folder is requested, send the index file
  String contentType = getContentType(request, path);  // Get the MIME type
  String pathWithGz = path + ".gz";
  if (FILESYS.exists(pathWithGz) || FILESYS.exists(path)) {  // If the file exists, either as a compressed archive, or normal
    if (FILESYS.exists(pathWithGz)) {                         // If there's a compressed version available
      path += ".gz";                                       // Use the compressed verion
    }
    File file = FILESYS.open(path, "r");                    // Open the file
    // size_t sent = webserver.streamFile(file, contentType);    // Send it to the client
    // request->send(FILESYS, path, String(), true);
    //Send file as text
    request->send(FILESYS, path, contentType);
    //Download file
    //request->send(FILESYS, "/index.htm", String(), true);
    file.close();  // Close the file again
    Serial.println(String("\tSent file: ") + path);
    return true;
  }
  Serial.println(String("\tFile Not Found: ") + path);  // If the file doesn't exist, return false
  return false;
}

void setup() {
  Serial.begin(115200);

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

  delay(200);

  Serial.println("wifi connecting...");
#ifdef WIFI_AP_ENABLE
  WiFi.softAPConfig(ip, ip, netmask);  // configure ip address for softAP
  WiFi.softAP(ssid, pw);               // configure ssid and password for softAP
#else
  /* Explicitly set the ESP8266 to be a WiFi-client, otherwise, it by default,
     would try to act as both a client and an access-point and could cause
     network-issues with your other WiFi-devices on your WiFi-network. */
  WiFi.disconnect();
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, pw);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
  }
#endif

  WiFi.hostname(MyHostname);

  Serial.println(WiFi.localIP());


  startFILESYS();



  // HTTP handler assignment
  //webserver.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
  //  AsyncWebServerResponse *response = request->beginResponse_P(200, "text/html", index_html_gz, sizeof(index_html_gz));
  //  response->addHeader("Content-Encoding", "gzip");
  //  request->send(response);
  //});

  // Route for root / web page
   webserver.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
    // request->send(FILESYS, "/index.html", String(), false, processor);
     if (!handleFileRead(request, "/index.html")) request->send(404, "text/plain", "index.html NotFound");
   });

  webserver.onNotFound([](AsyncWebServerRequest *request) {
    Serial.print("onNotFound ");
    Serial.println(request->url().c_str());
    if (!handleFileRead(request, request->url())) {  // check if the file exists in the flash memory (FILESYS), if so, send it
      request->send(404, "text/plain", "404: File Not Found");
    }
  });






  ///////////////

  // send a file when /index is requested
  //webserver.on("/format", HTTP_ANY, handleFileSysFormat);

  //webserver.on("/list", HTTP_GET, handleFileList);
  //load editor
  // webserver.on("/edit", HTTP_GET, [](AsyncWebServerRequest *request) {
  //   Serial.println("on edit GET");
  //   if (!handleFileRead(request, "/edit.htm")) request->send(404, "text/plain", "FileNotFound");
  // });
  // //create file
  // webserver.on("/edit", HTTP_PUT, handleFileCreate);
  // //delete file
  // webserver.on("/edit", HTTP_DELETE, handleFileDelete);


  // webserver.on(
  //   "/edit", HTTP_POST, [](AsyncWebServerRequest *request) {
  //     Serial.println("on edit POST");
  //     request->send(200);
  //   },
  //   handleFileUpload);

  /////////////////

   //const fs::FS& fs=SPIFFS

    AsyncWebHandler *editHandler = (nullptr);
    if (editHandler != nullptr) webserver.removeHandler(editHandler);
    editHandler = &webserver.addHandler(new SPIFFSEditor("","",FILESYS));




  AsyncElegantOTA.begin(&webserver);  // Start ElegantOTA


  // Cache responses for 10 minutes (600 seconds)
  webserver.serveStatic("/", FILESYS, "/").setCacheControl("max-age=600");
  // start server
  webserver.begin();
  wsServer.listen(82);
  Serial.print("Is server live? ");

  MDNS.begin(MyHostname);  // start the multicast domain name server
  Serial.print("mDNS responder started: http://");
  Serial.print(MyHostname);
  Serial.println(".local");

  //NBNS.begin(MyHostname);
#if 0
 Serial.printf("Starting SSDP...\n");
    SSDP.setSchemaURL("description.xml");
    SSDP.setHTTPPort(80);
    SSDP.setName("Philips hue clone");
    SSDP.setSerialNumber("001788102201");
    SSDP.setURL("index.html");
    SSDP.setModelName("Philips hue bridge 2012");
    SSDP.setModelNumber("929000226503");
    SSDP.setModelURL("http://www.meethue.com");
    SSDP.setManufacturer("Royal Philips Electronics");
    SSDP.setManufacturerURL("http://www.philips.com");
    SSDP.begin();

    webserver.on("/description.xml", HTTP_GET, [](AsyncWebServerRequest *request){
      StreamString output;
      if(output.reserve(1024)){
        uint32_t ip = WiFi.localIP();
        uint32_t chipId = ESP.getChipId();
        output.printf(ssdpTemplate,
          IP2STR(&ip),
          MyHostname,
          chipId,
          modelName,
          modelNumber,
          (uint8_t) ((chipId >> 16) & 0xff),
          (uint8_t) ((chipId >>  8) & 0xff),
          (uint8_t)   chipId        & 0xff
        );
        request->send(200, "text/xml", (String)output);
      } else {
        request->send(500);
      }
  });
#endif
}


// handle http messages
void handle_message(WebsocketsMessage msg) {
  if(msg.data().charAt(0) != '#')
  {
    //not a command
    return;
  }

    commaIndex = msg.data().indexOf(',');

  if (msg.data().charAt(1) == 'L') {
    
          int angle = msg.data().substring(commaIndex+1).toInt();  
      TRACE("Loader angle: ");
    TRACELN(angle);
  }
  else if(msg.data().charAt(1) == 'M')
  {
  int nextCommaIndex = msg.data().indexOf(',', commaIndex + 1);

  //swap Left and righ, why ????
  RValue = msg.data().substring(commaIndex + 1, nextCommaIndex).toInt();   // range +255 ... -255
  LValue = msg.data().substring(nextCommaIndex + 1).toInt();  // range +255 ... -255

  //****** motor1.drive(LValue);
  //******  motor2.drive(RValue);
  if (RValue > JOY_ACTIVE_ZONE) {
    analogWrite(motorPin1_1, RValue);
    analogWrite(motorPin1_2, 0);
    //direction_ascii_char_right = '>'; // ascii code for upper arrow
  } else if (RValue < -JOY_ACTIVE_ZONE) {
    analogWrite(motorPin1_1, 0);
    analogWrite(motorPin1_2, -RValue);
    // direction_ascii_char_right = '<'; // ascii code for down arrow
  } else {
    digitalWrite(motorPin1_1, 0);
    digitalWrite(motorPin1_2, 0);
  }

  if (LValue > JOY_ACTIVE_ZONE) {
    analogWrite(motorPin2_1, LValue);
    analogWrite(motorPin2_2, 0);
    //  direction_ascii_char_left = '<'; // ascii code for down arrow

  } else if (LValue < -JOY_ACTIVE_ZONE) {

    analogWrite(motorPin2_1, 0);
    analogWrite(motorPin2_2, -LValue);
    // direction_ascii_char_left = '>'; // ascii code for upper arrow
  } else {
    digitalWrite(motorPin2_1, 0);
    digitalWrite(motorPin2_2, 0);
  }

  TRACE("M1: ");
  TRACE(LValue);
  TRACE(" M2: ");
  TRACELN(RValue);
  }
}

void loop() {
  // webserver.handleClient();
  auto client = wsServer.accept();
  client.onMessage(handle_message);
  while (client.available()) {
    client.poll();
  }
}
