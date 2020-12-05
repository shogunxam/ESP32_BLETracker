#include "main.h"
#include "config.h"

#if ENABLE_OTA_WEBSERVER

#include <WiFi.h>
#include <WiFiClient.h>
#include <ESPmDNS.h>
#include <Update.h>
#include <string>

#include "WiFiManager.h"
#include "DebugPrint.h"
#include "OTAWebServer.h"

extern uint8_t NB_OF_BLE_DISCOVERED_DEVICES;
extern BLETrackedDevice BLETrackedDevices[99];

/* Style */
String style = F(
"<style>#file-input,input{width:100%;height:44px;border-radius:4px;margin:10px auto;font-size:15px}"
"input{background:#f1f1f1;border:0;padding:0 15px}body{background:#3498db;font-family:sans-serif;font-size:14px;color:#777}"
"#file-input{padding:0;border:1px solid #ddd;line-height:44px;text-align:left;display:block;cursor:pointer}"
"#bar,#prgbar{background-color:#f1f1f1;border-radius:10px}#bar{background-color:#3498db;width:0%;height:10px}"
"form{background:#fff;max-width:358px;margin:75px auto;padding:30px;border-radius:5px;text-align:center}"
".btn{background:#3498db;color:#fff;cursor:pointer}</style>");

/* Server Index Page */
String otaUpdate = style + F(
"<script src='https://ajax.googleapis.com/ajax/libs/jquery/3.2.1/jquery.min.js'></script>"
"<form method='POST' action='#' enctype='multipart/form-data' id='upload_form'>"
"<input type='file' name='update' id='file' onchange='sub(this)' style=display:none>"
"<label id='file-input' for='file'>   Choose file...</label>"
"<input type='submit' class=btn value='Update'>"
"<br><br>"
"<div id='prg'></div>"
"<br><div id='prgbar'><div id='bar'></div></div><br><br><br><label>by Shogunxam<label></form>"
"<script>"
"function sub(obj){"
"var fileName = obj.value.split('\\\\');"
"document.getElementById('file-input').innerHTML = '   '+ fileName[fileName.length-1];"
"};"
"$('form').submit(function(e){"
"e.preventDefault();"
"var form = $('#upload_form')[0];"
"var data = new FormData(form);"
"$.ajax({"
"url: '/update',"
"type: 'POST',"
"data: data,"
"contentType: false,"
"processData:false,"
"xhr: function() {"
"var xhr = new window.XMLHttpRequest();"
"xhr.upload.addEventListener('progress', function(evt) {"
"if (evt.lengthComputable) {"
"var per = evt.loaded / evt.total;"
"$('#prg').html('progress: ' + Math.round(per*100) + '%');"
"$('#bar').css('width',Math.round(per*100) + '%');"
"}"
"}, false);"
"return xhr;"
"},"
"success:function(d, s) {"
"console.log('success!') "
"},"
"error: function (a, b, c) {"
"}"
"});"
"});"
"</script>");

#define _CONTENT_DELAY_ 20
#define SEND_CONTENT(x) server.sendContent(x); /*delay(_CONTENT_DELAY_);*/
#define SEND_CONTENT_P(x) server.sendContent_P(x); /*delay(_CONTENT_DELAY_);*/

OTAWebServer::OTAWebServer()
  :server(80)
  ,serverRunning(false)
{

}

/*
 * setup function
 */
void OTAWebServer::setup(const String& hN, const String& _ssid_, const String& _password_) {

  hostName = hN;
  ssid = _ssid_;
  password =_password_;

  DEBUG_PRINTLN("WebServer Setup");
  WiFiConnect(ssid, password);

  /*use mdns for host name resolution*/
  if (!MDNS.begin(hostName.c_str())) { //http://bletracker
    DEBUG_PRINTLN("Error setting up MDNS responder!");
  }
  else
    DEBUG_PRINTLN("mDNS responder started");

  /*return index page which is stored in serverIndex */
  server.on(F("/"), HTTP_GET, [&]() {
    if (!server.authenticate(WEBSERVER_USER, WEBSERVER_PASSWORD)) {
        return server.requestAuthentication();
        }
    server.sendHeader(F("Connection"), F("close"));
    server.setContentLength(CONTENT_LENGTH_UNKNOWN);
    server.send(200, F("text/html"), "");
    SEND_CONTENT(style);
    SEND_CONTENT(F("<form name=indexForm>"
                   "<h1>"GATEWAY_NAME"</h1><h2>Index</h2>"
                   R"~~(<input type=button onclick="window.open('/serverinfo','_self')" class=btn value="System Information"><br>
                   <input type=button onclick="window.open('/otaupdate','_self')" class=btn value="OTA Update">
                   <br><br><label>by Shogunxam<label></form>)~~"));
    server.client().flush();
    server.client().stop();
  });


  server.on(F("/otaupdate"), HTTP_GET, [&]() {
    if (!server.authenticate(WEBSERVER_USER, WEBSERVER_PASSWORD)) {
        return server.requestAuthentication();
        }    
    server.sendHeader(F("Connection"), F("close"));
    server.send(200, F("text/html"), otaUpdate);
  });

  server.on(F("/serverinfo"), HTTP_GET, [&]() {
    if (!server.authenticate(WEBSERVER_USER, WEBSERVER_PASSWORD)) {
      return server.requestAuthentication();
    }

    String uptime = formatMillis(millis());
    server.sendHeader(F("Connection"), F("close"));
    server.setContentLength(CONTENT_LENGTH_UNKNOWN);
    server.send(200, F("text/html"), "");
    SEND_CONTENT(style);
    SEND_CONTENT(F("<form name=indexForm>"
                   "<h1>"GATEWAY_NAME"<h1>"
                   "<h2>System Information</h2>"
                   "<table style='width:100%'>"
                   "<tr><th>Firmware Version<th><td>"VERSION"</td></tr>"
                  "<tr><th>Uptime<th><td>"));
    SEND_CONTENT(uptime);
    SEND_CONTENT(F("</td></tr>"
                   "<tr><th>SSID<th><td>"WIFI_SSID"</td></tr>"
                   "</table>"
                   "<br><h2>Devices</h2>"
                   "<table style='width:100%'>"
                   "<tr><th>Device</th><th>RSSI</th><th>Battery</th><th>State</th></tr>"));

    //Perhaps we need a mutex here to protect BLETrackedDevices but we are in readonly mode
    std::ostringstream row;
    const String rowTagOpen = F(R"(<td style="text-align:center">)");
    const String rowTagClose = F("</td>");
    for (int i=0;i<NB_OF_BLE_DISCOVERED_DEVICES; i++)
    {
      SEND_CONTENT(F("<tr><td>"));
      SEND_CONTENT(BLETrackedDevices[i].address);
      SEND_CONTENT(rowTagClose);
      SEND_CONTENT(rowTagOpen);
      SEND_CONTENT_P(BLETrackedDevices[i].rssi);
      SEND_CONTENT(rowTagClose);
      SEND_CONTENT(rowTagOpen); 
      SEND_CONTENT(String(BLETrackedDevices[i].batteryLevel));
      SEND_CONTENT(rowTagClose);
      SEND_CONTENT(rowTagOpen);
      SEND_CONTENT(BLETrackedDevices[i].isDiscovered ? F("On"): F("Off")) ;
      SEND_CONTENT(F("</tr></td>"));
    }

    SEND_CONTENT(F(R"~~(</table>
    <input type=button onclick="window.open('/reset','_self')" class='btn' value="Reset">
    <br><input type=button onclick="window.open('/','_self')" class=btn value="Back">
    <br><br><label>by Shogunxam<label></form>
    <script>function timedRefresh(timeoutPeriod){setTimeout("location.reload(true);",timeoutPeriod);}window.onload = timedRefresh(10000);</script>)~~"));

    server.client().flush();
    server.client().stop();

  });

  server.on(F("/reset"), HTTP_GET, [&]() {
    if (!server.authenticate(WEBSERVER_USER, WEBSERVER_PASSWORD)) {
      return server.requestAuthentication();
    }
    server.client().setNoDelay(true);
    server.client().setTimeout(30);
    server.sendHeader(F("Connection"), F("close"));
    server.setContentLength(CONTENT_LENGTH_UNKNOWN);
    server.send(200, F("text/html"), "");
    SEND_CONTENT(F("<div align='center'>Resetting...<br><progress id='g' class='y' value='0' max='100' style='align-self:center; text-align:center;'/></div>"
                   "<script>window.onload=function(){};var progval=0;var myVar=setInterval(Prog,80);"
                   "function Prog(){progval++;document.getElementById('g').value=progval;"
                   "if (progval==100){clearInterval(myVar);setTimeout(Check, 3000);}}"
                   "function Check(){if (progval==100){clearInterval(myVar);var ftimeout=setTimeout(null,5000);"
                   "window.location='/';}}</script>"));
    server.client().flush();
    server.client().stop();
    ESP.restart();
  });

  /*handling uploading firmware file */
  server.on(F("/update"), HTTP_POST, [&]() {
    server.sendHeader(F("Connection"), F("close"));
    server.send(200, F("text/plain"), (Update.hasError()) ? F("FAIL") : F("OK"));
    ESP.restart();
  }, [&]() {
    HTTPUpload& upload = server.upload();
    if (upload.status == UPLOAD_FILE_START) {
      DEBUG_PRINTF("Update: %s\n", upload.filename.c_str());
      if (!Update.begin(UPDATE_SIZE_UNKNOWN)) { //start with max available size
        Update.printError(Serial);
      }
    } else if (upload.status == UPLOAD_FILE_WRITE) {
      /* flashing firmware to ESP*/
      if (Update.write(upload.buf, upload.currentSize) != upload.currentSize) {
        Update.printError(Serial);
      }
    } else if (upload.status == UPLOAD_FILE_END) {
      if (Update.end(true)) { //true to set the size to the current progress
        DEBUG_PRINTF("Update Success: %u\nRebooting...\n", upload.totalSize);
      } else {
        Update.printError(Serial);
      }
    }
  });
  server.begin();
}

void WebServerLoop(void* param)
{
  if (param == NULL)
    return;

  WebServer* server = (WebServer* ) param;
  while(true)
  {
      server->handleClient();
      delay(100);
  }
}

void OTAWebServer::loop(void) {

  if(serverRunning)
    return;

  xTaskCreatePinnedToCore(
  WebServerLoop,               /* Function to implement the task */
  "WebServerLoop",              /* Name of the task */
  10000,                   /* Stack size in words */
  (void*)&server,         /* Task input parameter */
  20,                      /* Priority of the task */
  NULL,                    /* Task handle. */
  1);

  serverRunning = true;
}
#endif /*ENABLE_OTA_WEBSERVER*/