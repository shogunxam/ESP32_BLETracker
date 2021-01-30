# ESP32 BLETracker
A simple example describing how to track a Bluetooth Low Energy device with an ESP32, the MQTT protocol and Home Assistant. Please note that the targeted device can't have a changing BLE address (normally called random instead of public address).  

Use [PlatformIO](https://platformio.org/) to build and deploy this application, remember to install [git](https://git-scm.com/downloads) in order to allow PlatformIO to download automatically all the required dependencies.<br>
You have to modify the **user_config.h** file inserting the correct informations to connect to the WiFi and to the MQTT broker.<br>
The GATEWAY_NAME is used as Client ID to connect to the broker so be sure it's unique.<br>
The battery level can be read from the devices providing the Battery Service (0x180F) and the Battery Level characteristic (0x2A19), check the avaiability using a nRF Sniffer i.e. [nRF Connect](https://play.google.com/store/apps/details?id=no.nordicsemi.android.mcp)<br>
This feature was succesfully tested with a Nut Mini, using other devices you could have connection problems.<br>
If many devices are discovered the battery level check can be very slow causing frequent Wi-Fi disconnection so that I have introduced a whitelist containing the Mac Address of the devices to check. The whitelist is in the form:<br>
BLE_BATTERY_WHITELIST       "XXXXXXXXX","YYYYYYYY"<br>
Mac Addresses have to be uppercase without ":" or "-" i.e "BA683F7EC159"

The application can generate the following topics:<br>
A topic for the BLETracker state:<br>
&lt;LOCATION&gt;/&lt;GATEWAY_NAME&gt;/&lt;BLE_ADDRESS&gt;/LWT payload: &lt;online|offline&gt;<br><br>
A single topic with the palyload in JSON format containing all the items retruned by the device (this is the default):<br>
&lt;LOCATION&gt;/&lt;GATEWAY_NAME&gt;/&lt;BLE_ADDRESS payload: { "state":&lt;"on"|"off"&gt;,"rssi":&lt;dBvalue&gt;,"battery":&lt;batterylevel&gt;}<br><br>
A topic for each item returned by the advertised device:<br>
&lt;LOCATION&gt;/&lt;GATEWAY_NAME&gt;/&lt;BLE_ADDRESS&gt;/state payload: &lt;"on"|"off"&gt;<br>
&lt;LOCATION&gt;/&lt;GATEWAY_NAME&gt;/&lt;BLE_ADDRESS&gt;/rssi payload: &lt;dBvalue&gt;<br>
&lt;LOCATION&gt;/&lt;GATEWAY_NAME&gt;/&lt;BLE_ADDRESS&gt;/battery payload: &lt;batterylevel&gt;<br><br>
A topic with helpfull system information:<br>
&lt;LOCATION&gt;/&lt;GATEWAY_NAME&gt;/&lt;BLE_ADDRESS&gt;/sysinfo, payload: { "uptime":&lt;timesinceboot&gt;,"version":&lt;versionnumber&gt;,"SSID":&lt;WiFiSSID&gt;,"IP":&lt;ipnumber&gt;}

### WEB Server
A WEB server is integrated into the BLETracker, it can be accessed using a web browser and the ip or the network name of the traker.<br>
The WEB server can be used to see some system informations and to update the firmware using an **OTA Update**, simply choosing the .bin file to upload.<br>
Default credential to access the WEB Server are:<br>
user: admin<br>
password: admin<br>
<br>
The new WEB server interface allow to customize the list of devices to be tracked, the scan period, and the MQTT broker paramters.<br>
The new interface allows also to monitor some logs. The number of logs is limited and when the maxium capacity is reached the oldest are removed.<br>
<br>

### FHEM Support
If properly configured the BLETracker can be integrate in your FHEM environment. Instead publishing MQTT tokens the BLETracker can be configured to act as a lepresenced daemon.
A server will run on the port 5333 listening for incoming connection generated by the collectord. The FHEM support and the MQTT support are mutually exclusive because of memory issues. The FHEM support can be easly enabled using Platform.io simply choosing the **esp32dev-ble-fhem-release** build variant.<br>
The values of *presence_timeout* and *absence_timeout* stored in the collectord.conf cannot be less than or equal to the BLE_SCANNING_PERIOD (default is 10 seconds).

# Arduino IDE Notes
You can build this skatch using Arduino IDE (currently it's using arduino-esp32 v1.0.4), but be sure to install the required dependencies:<br>
* You have to install the esp32 Board in Arduino IDE. You can find a tutorial following this [link]( https://randomnerdtutorials.com/installing-the-esp32-board-in-arduino-ide-windows-instructions/)
* You have to install the library PubSubClient v2.8 (not needed if you want enable FHEM support)
* To enable the FHEM support you have to:
    * install the Regexp library (https://www.arduino.cc/reference/en/libraries/regexp/)
    * set USE_MQTT to false and USE_FHEM_LEPRESENCE_SERVER to true in the config.h
* Because a bug in the BLE library provided by arduino-esp32 v1.0.4 you have to replace it with this one https://github.com/shogunxam/ESP32-BLE-Arduino.<br>
 **Since the release v2.0 this one must be used https://github.com/espressif/arduino-esp32/tree/6b0114366baf986c155e8173ab7c22bc0c5fcedc/libraries/BLE.**<br>
 The libray to replace should be located in the folder ~/.arduino15/packages/esp32/hardware/esp32/x.x.x/libraries/BLE for Unix users and in C:\Users\YourUserName\AppData\Local\Arduino15\packages\esp32\hardware\esp32\x.x.x\libraries\BLE for Windows users.<br>

Build using the *Minimal SPIFFS* partition schema.

# Tested BLE Devices
| BLE Device | Discovery | Battery |
|------------|----------|---------|
|Nut mini   |  :heavy_check_mark:|  :heavy_check_mark:|
|Nut2        | :heavy_check_mark: |:heavy_exclamation_mark:|
|Remote Shutter   |  :heavy_check_mark:|  :heavy_check_mark:|
|Xiomi Amazfit Bip|:heavy_check_mark:|:x:|
|REDMOND RFT-08S|:heavy_check_mark:|:x:|
|Xiomi Mi Smart Band 4|:heavy_check_mark:|:x:|
|Fitness Band GT101|:heavy_check_mark:|:x:|


# Home Assistant integration
This is a simple example of a package to manage a Nut Traker device.<br>
A more complex example combining more BLETrackes can be found inside the Doc folder.<br>
![Alt text](/image.png?raw=true "Screenshot")
```
###################################################
#Sensors
sensor:
- platform: mqtt
  state_topic: 'home/BLETracker/XXXXXXXXXXXX'
  name: 'bt_nut_upstairs_state' 
  value_template: "{{ value_json.state }}"
  expire_after: 120 
  force_update: true

- platform: mqtt
  state_topic: 'home/BLETracker/XXXXXXXXXXXX'
  name: 'bt_nut_upstairs_battery' 
  value_template: "{{ value_json.battery }}"
  force_update: true  

###############################################
#Battery Sensor to resolve unknown/unavailable device state
- platform: template
  sensors:
    bt_nut_battery:
      unit_of_measurement: '%'
      value_template: >
          {% if (is_state('sensor.bt_nut_upstairs_battery', 'unknown') or is_state('sensor.bt_nut_upstairs_battery', 'unavailable') or is_state('sensor.bt_nut_upstairs_battery', '-1')) %}
            unknown
          {% else %}
            {{ states('sensor.bt_nut_upstairs_battery')|int }}
          {% endif %}
      icon_template: >
        {% set battery_level = states('sensor.bt_nut_battery.state')|default(0)|int %}
        {% set battery_round = (battery_level / 10) |int * 10 %}
        {% if battery_round >= 100 %}
          mdi:battery
        {% elif battery_round > 0 %}
          mdi:battery-{{ battery_round }}
        {% else %}
          mdi:battery-alert
        {% endif %}


###############################################
#Binary Sensors to resolve unknown/unavailable device state

binary_sensor:
- platform: template
  sensors:
    ble_tracker_nut_mario:
      friendly_name: 'ble_tracker_nut'
      value_template: >
        {% if (is_state('sensor.bt_nut_upstairs_state', 'unknown') or is_state('sensor.bt_nut_upstairs_state', 'unavailable') or is_state('sensor.bt_nut_upstairs_state', 'off')) %}
          false
        {% else %}
          true
        {% endif %}
      device_class: presence
      delay_off: 
        minutes: 1
        
#######################
#Check BLETracker state
- platform: mqtt
  name: BLETracker_state
  state_topic: 'home/BLETracker/LWT'
  payload_on: "online"
  payload_off: "offline"
  device_class: "connectivity"

########################
#Automations
automation:
- id: notify_BLETracker_state
  alias: Notify BLETracker offline
  initial_state: true
  trigger:
  - platform: state
    entity_id: binary_sensor.BLETracker_state
    to: 'off'
    for:
      minutes: 5 
  action:
  - service: notify.telegram
    data:
      title: BLETracker
      message: "BLETracker offline"

- id: notify_nut_battery
  alias: Notify Battery low
  initial_state: true
  trigger:
  - platform: template
    value_template: >
      {{ ((states('sensor.bt_nut_battery') | int ) <= 20 ) and ( is_state('sensor.bt_nut_battery', 'unknown') or  is_state('sensor.bt_nut_battery', 'unavailable')) and ( states('sensor.bt_nut_battery') | int >= 0) }}
  action:
  - service: notify.telegram
    data_template:
      title: BLETracker
      message: "Nut battery at {{states('sensor.bt_nut_battery')}}%"

```
Alternatively you can use the single topic returning the state in the following way:
``` 
- platform: mqtt
  name: "bt_nut_upstairs_stat"
  state_topic: "home/BLETracker/XXXXXXXXXXXX/state"
  availability_topic: "home/BLETracker/LWT"
  expire_after: 300
  value_template: >-
     {% if value == 'on' %}
        home
     {% elif value == 'off' %}
        away
     {% else %}
        away
     {% endif %}
```

## Licence

Permission is hereby granted, free of charge, to any person obtaining a copy<br>
of this software and associated documentation files (the "Software"), to deal<br>
in the Software without restriction, including without limitation the rights<br>
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell<br>
copies of the Software, and to permit persons to whom the Software is<br>
furnished to do so, subject to the following conditions:<br>
<br>
The above copyright notice and this permission notice shall be included in all<br>
copies or substantial portions of the Software.<br>
<br>
THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR<br>
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,<br>
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE<br>
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER<br>
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,<br>
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE<br>
SOFTWARE.<br>

*If you like the content of this repo, please add a star! Thank you!*

# Support my work
If you like my work, please consider buying me a coffee. Thank you for your support! :grin:<br><br>
[![Buy me a coffee][buymeacoffee-shield]][buymeacoffee]

[buymeacoffee-shield]: https://www.buymeacoffee.com/assets/img/guidelines/download-assets-sm-2.svg
[buymeacoffee]: https://www.buymeacoffee.com/shogunxam
