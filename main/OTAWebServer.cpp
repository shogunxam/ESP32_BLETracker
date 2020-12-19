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
#include "build_defs.h"
#include "macro_utility.h"
#include "settings.h"

extern std::vector<BLETrackedDevice> BLETrackedDevices;

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

//document.getElementById('trk_list').innerHTML = settings.trk_list.toString().split(',').join('\n');
const String configPage = F(R"~(<script src="https://ajax.googleapis.com/ajax/libs/jquery/3.5.1/jquery.min.js"></script>
<script>
$(document).ready(function(){
  $('#mqttsrvr').val(settings.mqtt_address);
  $('#mqttport').val(settings.mqtt_port);
  $('#mqttusr').val(settings.mqtt_usr);
  $('#mqttpwd').val(settings.mqtt_pwd);
  $('#whiteList').prop("checked", settings.whiteList);
  $('#trk_list').val(settings.trk_list.toString().split(',').join('\n'));
  $('#btry_list').val(settings.btry_list.toString().split(',').join('\n'));
});
$(function () {
  $('#configure').click(function(e) {
    var d = $('#form').serialize();
    $.ajax({
      type: "POST",
      url: "/updateconfig",
      data: d,
      success:function() {console.log('success!'); var ftimeout=setTimeout(null,5000);window.location='/reset';},
      error:function() {console.log('error!');},
    });
    e.preventDefault();
  });
  $('#factory').click(function(e) {
    window.location.href='/config?factory=true';
    e.preventDefault();
  });
  $('#undo').click(function(e) {
    window.location.href='/config';
    e.preventDefault();
  });
});
</script>
<form id="form" name="config" action="">
 <fieldset>
  <legend>MQTT Broker:</legend>
  <table>
  <tr>
  <td><label for="mqttsrvr">Address:</label></td>
  <td><input type="text" id="mqttsrvr" name="mqttsrvr"></td>
  </tr><tr>
  <td><label for="mqttport">Port:</label></td>
  <td><input type="number" id="mqttport" name="mqttport"></td>
 </tr><tr>
  <td><label for="mqttusr">User:</label></td>
  <td><input type="text" id="mqttusr" name="mqttusr"></td>
 </tr><tr>  
  <td><label for="mqttpwd">Password:</label></td>
  <td><input type="password" id="mqttpwd" name="mqttpwd"></td> 
   </tr><tr>
  </table>
  </fieldset>
 <fieldset>
  <legend>Devices:</legend>
  <input type="checkbox" id="whiteList" name="whiteList" style="width:auto;height:auto">
  <label for="whiteList">Enable Track Whitelist</label><br>
  <table style = "margin-left: auto; margin-right: auto;">
  <tr><td>
  <label for="trk_list">Track Whitelist:</label><br>
  <textarea id="trk_list" name="trk_list" rows="10" cols="13" style="overflow-y: scroll;"></textarea>
  </td>
  <td>
  <label for="btry_list">Battery Whitelist:</label><br>
  <textarea id="btry_list" name="btry_list" rows="10" cols="13" style="overflow-y: scroll;"></textarea>
  </td>
  </tr></table>
  </fieldset>
  <input style="width:49%" type="submit" id="undo" value="Undo Changes">
  <input style="width:49%" type="submit" id="factory" value="Factory Values">
  <input type="submit" id="configure" value="Submit">
  <br><input type=button onclick="window.open('/','_self')" class=btn value="Back">
  <br><br><label>by Shogunxam<label></form>
</form>)~");

const String homePage = F("<form name=indexForm>"
                   "<h1>"GATEWAY_NAME"</h1><h2>Index</h2>"
                   R"~~(<input type=button onclick="window.open('/serverinfo','_self')" class=btn value="System Information"><br>
                   <input type=button onclick="window.open('/otaupdate','_self')" class=btn value="OTA Update">
                   <br><br><label>by Shogunxam<label></form>)~~");

const String sysInfoPage = F(R"~(<script src="https://ajax.googleapis.com/ajax/libs/jquery/3.5.1/jquery.min.js"></script>
<script>
$(document).ready(function()
{
  $('#gateway').text(data.gateway);
  $('#firmware').text(data.firmware);
  $('#uptime').text(data.uptime);
  $('#ssid').text(data.ssid);
  if(data.build) $('#build').text(data.build);
  else $('#r2').hide();
  if(data.memory)	$('#memory').text(data.memory);
  else $('#r3').hide();
  if(!data.battery)
  $('#c1').hide();
  var table= $('#devices');
  data.devices.forEach(function(item, index) {
    tmac=$("<td/>").text(item.mac);
      trssi=$("<td/>").text(item.rssi);
      if(data.battery)
      tbtr=$("<td/>").text(item.battery);
      tstate=$("<td/>").text(item.state);
    var row = $("<tr/>").append(tmac)
      row.append(trssi); 
      if(data.battery)
        row.append(tbtr);
      row.append(tstate);
      table.append(row);
      console.log(table);
  });
});
</script>
<form name=indexForm>
<h1 id='gateway'></h1>
<h2>System Information</h2>
<table style='width:100%'>
<tr id='r1'><th>Firmware Version</th><td id="firmware"></td></tr>
<tr id='r2'><th>Build Time</th><td id="build"></td></tr>
<tr id='r3'><th>Free Memory</th><td id="memory"></td></tr>
<tr id='r4'><th>Uptime</th><td id="uptime"></td></tr>
<tr id='r5'><th>SSID</th><td id="ssid"></td></tr>
</table>
<br><h2>Devices</h2>
<table id="devices" style='width:100%'>
<tr><th>Device</th><th>RSSI</th><th id="c1">Battery</th><th>State</th></tr>
</table>
<input type=button onclick="window.open('/reset','_self')" class='btn' value="Reset">
<br><input type=button onclick="window.open('/','_self')" class=btn value="Back">
<br><br><label>by Shogunxam<label></form>
</form>
)~");

#define _CONTENT_DELAY_ 20
#define SEND_CONTENT(x) server.sendContent(x) /*;delay(_CONTENT_DELAY_)*/
#define SEND_CONTENT_P(x) server.sendContent_P(x) /*;delay(_CONTENT_DELAY_)*/

#if DEVELOPER_MODE
static const char BuildTime[] =
    {
        BUILD_YEAR_CH0, BUILD_YEAR_CH1, BUILD_YEAR_CH2, BUILD_YEAR_CH3,
        '-',
        BUILD_MONTH_CH0, BUILD_MONTH_CH1,
        '-',
        BUILD_DAY_CH0, BUILD_DAY_CH1,
        'T',
        BUILD_HOUR_CH0, BUILD_HOUR_CH1,
        ':',
        BUILD_MIN_CH0, BUILD_MIN_CH1,
        ':',
        BUILD_SEC_CH0, BUILD_SEC_CH1,
        '\0'};
#endif

OTAWebServer::OTAWebServer()
  :server(80)
  ,serverRunning(false)
{

}

void OTAWebServer::resetESP32Page()
{
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
    SEND_CONTENT(homePage);
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

  server.on(F("/config"), HTTP_GET, [&]() {

    if (!server.authenticate(WEBSERVER_USER, WEBSERVER_PASSWORD)) {
      return server.requestAuthentication();
    }

    String data ="<script> let settings = ";
    if(server.args() > 0 && server.hasArg("factory") && server.arg("factory")=="true")
    {
      Settings factory;
      data += factory.toJavaScriptObj();
    }
    else
        data += SettingsMngr.toJavaScriptObj();

    data +="</script>";
    server.sendHeader(F("Connection"), F("close"));
    server.setContentLength(CONTENT_LENGTH_UNKNOWN);
    server.send(200, F("text/html"), "");
    SEND_CONTENT(style);

    SEND_CONTENT(data);
    SEND_CONTENT(configPage);
  });

  server.on(F("/updateconfig"), HTTP_POST, [&]() {
    if (!server.authenticate(WEBSERVER_USER, WEBSERVER_PASSWORD)) {
      return server.requestAuthentication();
    }
    Settings newSettings;
    newSettings.mqttServer = server.arg("mqttsrvr");
    newSettings.mqttPort = server.arg("mqttport").toInt();
    newSettings.mqttUser =  server.arg("mqttusr");
    newSettings.mqttPwd = server.arg("mqttpwd");
    newSettings.trackWhiteList.clear();
    Settings::StringListToArray(server.arg("trk_list"), newSettings.trackWhiteList);
    newSettings.batteryWhiteList.clear();
    Settings::StringListToArray(server.arg("btry_list"), newSettings.batteryWhiteList);
    newSettings.enableWhiteList = server.arg("whiteList") == "on";
    newSettings.SettingsFile(SettingsMngr.GetSettingsFile());

    DEBUG_PRINTLN(newSettings.toJavaScriptObj().c_str());

    server.sendHeader(F("Connection"), F("close"));
    server.setContentLength(CONTENT_LENGTH_UNKNOWN);
    if(newSettings.Save())
      server.send(200, F("text/html"), "Ok");
    else
      server.send(500, F("text/html"), "Error saving settings");
    server.client().flush();
    server.client().stop();
  });

  server.on(F("/serverinfo"), HTTP_GET, [&]() {
    if (!server.authenticate(WEBSERVER_USER, WEBSERVER_PASSWORD)) {
      return server.requestAuthentication();
    }

    server.sendHeader(F("Connection"), F("close"));
    server.setContentLength(CONTENT_LENGTH_UNKNOWN);
    server.send(200, F("text/html"), "");
    SEND_CONTENT(style);
    String data ="<script> let data = {";
    data+= R"(gateway : ")" GATEWAY_NAME R"(",)";
    data+= R"(firmware : ")" VERSION R"(",)";
#if DEVELOPER_MODE
    data+= R"(build: ")" + String(BuildTime) + R"(",)";
    data+= R"(memory: ")" + String(xPortGetFreeHeapSize()) +R"( bytes",)";
#endif
    data+= R"(uptime: ")" + formatMillis(millis()) +R"(",)";
    data+= R"(ssid: ")" WIFI_SSID R"(",)";
    data+= R"(battery: )" xstr(PUBLISH_BATTERY_LEVEL) R"(,)";
    data+= R"(devices: [)";
    SEND_CONTENT(data);
    bool first = true;
    for (auto& trackedDevice : BLETrackedDevices)
    {
      data = first ? "" : ","; first = false;
      data += R"({mac : ")" + trackedDevice.address + R"(",)";
      data += R"(rssi : )" + trackedDevice.rssi + R"(,)";
      #if PUBLISH_BATTERY_LEVEL
      data += R"(battery : )" + String(trackedDevice.batteryLevel) + R"(,)";
      #endif
      data += R"(state : ")" + String(trackedDevice.isDiscovered ? F("On"): F("Off")) + R"("})";
      SEND_CONTENT(data);
      
    }
    SEND_CONTENT(R"(]}</script>)");
    SEND_CONTENT(sysInfoPage);
    server.client().flush();
    server.client().stop();
  });

  server.on(F("/reset"), HTTP_GET, [&]() {
    if (!server.authenticate(WEBSERVER_USER, WEBSERVER_PASSWORD)) {
      return server.requestAuthentication();
    }
    resetESP32Page();
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
