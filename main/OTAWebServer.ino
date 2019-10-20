#include "main.h"
#include "config.h"

#if ENABLE_OTA_WEBSERVER

#include <WiFi.h>
#include <WiFiClient.h>
#include <ESPmDNS.h>
#include <Update.h>

#include "DebugPrint.h"
#include "OTAWebServer.h"

extern uint8_t NB_OF_BLE_DISCOVERED_DEVICES;
extern BLETrackedDevice BLETrackedDevices[99];

/* Style */
String style =
"<style>#file-input,input{width:100%;height:44px;border-radius:4px;margin:10px auto;font-size:15px}"
"input{background:#f1f1f1;border:0;padding:0 15px}body{background:#3498db;font-family:sans-serif;font-size:14px;color:#777}"
"#file-input{padding:0;border:1px solid #ddd;line-height:44px;text-align:left;display:block;cursor:pointer}"
"#bar,#prgbar{background-color:#f1f1f1;border-radius:10px}#bar{background-color:#3498db;width:0%;height:10px}"
"form{background:#fff;max-width:358px;margin:75px auto;padding:30px;border-radius:5px;text-align:center}"
".btn{background:#3498db;color:#fff;cursor:pointer}</style>";

/* Server Index Page */
String otaUpdate = 
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
"</script>" + style;

String serverIndex = 
"<form name=indexForm>"
"<h1>"GATEWAY_NAME"</h1><h2>Index</h2>"
"<input type=button onclick=\"window.open('/serverinfo','_self')\" class=btn value=\"System Information\"><br>"
"<input type=button onclick=\"window.open('/otaupdate','_self')\" class=btn value=\"OTA Update\">"
"<br><br><label>by Shogunxam<label></form>" + style ;

OTAWebServer::OTAWebServer()
  :server(80)
{

}

/*
 * setup function
 */
void OTAWebServer::setup(const String& hN, const String& _ssid_, const String& _password_) {

  hostName = hN;
  ssid = _ssid_;
  password =_password_;

  if (WiFi.status() != WL_CONNECTED)
  {
    // Connect to WiFi network
    WiFi.begin(ssid.c_str(), password.c_str());
    DEBUG_PRINTLN("");

    // Wait for connection
    while (WiFi.status() != WL_CONNECTED) {
      delay(500);
      DEBUG_PRINT(".");
    }
  }

  DEBUG_PRINTLN("");
  DEBUG_PRINT("Connected to ");
  DEBUG_PRINTLN(ssid.c_str());
  DEBUG_PRINT("IP address: ");
  DEBUG_PRINTLN(WiFi.localIP());

  /*use mdns for host name resolution*/
  if (!MDNS.begin(hostName.c_str())) { //http://esp32.local
    DEBUG_PRINTLN("Error setting up MDNS responder!");
    while (1) {
      delay(1000);
    }
  }
  DEBUG_PRINTLN("mDNS responder started");

  /*return index page which is stored in serverIndex */
  server.on("/", HTTP_GET, [&]() {
    if (!server.authenticate(OTA_USER, OTA_PASSWORD)) {
        return server.requestAuthentication();
        }
    server.sendHeader("Connection", "close");
    server.send(200, "text/html", serverIndex);
  });


  server.on("/otaupdate", HTTP_GET, [&]() {
    if (!server.authenticate(OTA_USER, OTA_PASSWORD)) {
        return server.requestAuthentication();
        }    
    server.sendHeader("Connection", "close");
    server.send(200, "text/html", otaUpdate);
  });

  server.on("/serverinfo", HTTP_GET, [&]() {
    String serverInfo;
    serverInfo+="<form name=indexForm>";
    serverInfo+="<h1>"GATEWAY_NAME"<h1>";
    serverInfo+="<h2>System Information</h2>";
    serverInfo+="<table style='width:100%'>";
    serverInfo+="<tr><th>Program Version<th><td>"VERSION"</td></tr>";
    std::string uptime = formatMillis(millis());
    serverInfo+="<tr><th>Uptime<th><td>"; serverInfo+=uptime.c_str(); serverInfo+=+"</td></tr>";
    serverInfo+="<tr><th>SSID<th><td>"WIFI_SSID"</td></tr>";
    serverInfo+="</table>";
    serverInfo+="<br><h2>Devices</h2>";
    serverInfo+="<table style='width:100%'>";
    serverInfo+="<tr><th>Device</th><th>RSSI</th><th>Battery</th><th>State</th></tr>";

    for (int i=0;i<NB_OF_BLE_DISCOVERED_DEVICES; i++)
    {
      std::ostringstream row;
      row <<"<tr><td>"<<BLETrackedDevices[i].address.c_str()<<"</td>";
      row <<"<td style=\"text-align:center\">"<<BLETrackedDevices[i].rssi<<"</td>";
      row <<"<td style=\"text-align:center\">"<<BLETrackedDevices[i].batteryLevel<<"</td>";
      row <<"<td style=\"text-align:center\">"<<(BLETrackedDevices[i].isDiscovered ? "On": "Off") <<"</td></tr>";
      serverInfo += row.str().c_str();
    }
    serverInfo +="</table>";
    serverInfo +="<br><input type=button onclick=\"window.open('/','_self')\" class=btn value=\"Back\">";
    serverInfo +="<br><br><label>by Shogunxam<label></form>";
    serverInfo += style + "<style>th,td{text-align:left;}</style>";
    if (!server.authenticate(OTA_USER, OTA_PASSWORD)) {
    return server.requestAuthentication();
    }
    server.sendHeader("Connection", "close");
    server.send(200, "text/html", serverInfo);
  });

  /*handling uploading firmware file */
  server.on("/update", HTTP_POST, [&]() {
    server.sendHeader("Connection", "close");
    server.send(200, "text/plain", (Update.hasError()) ? "FAIL" : "OK");
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

void OTAWebServer::loop(void) {
  server.handleClient();
  delay(1);
}
#endif /*ENABLE_OTA_WEBSERVER*/