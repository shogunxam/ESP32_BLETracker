# ESP32 BLETracker
A simple example describing how to track a Bluetooth Low Energy device with an ESP32, the MQTT protocol and Home Assistant. Please note that the targeted device can't have a changing BLE address (normally called random instead of public address).  

Use [PlatformIO](https://platformio.org/) to build and deploy this application, remember to install [git](https://git-scm.com/downloads) in order to allow PlatformIO to download automatically all the required dependencies.<br>
You have to modify the user_config.h file inserting the correct informations to connect to the WiFi and to the MQTT broker.<br>
The GATEWAY_NAME is used as Client ID to connect to the broker so be sure it's unique.<br>
The battery level can be read from the devices providing the Battery Service (0x180F) and the Battery Level characteristic (0x2A19), check the avaiability using a nRF Sniffer i.e. [nRF Connect](https://play.google.com/store/apps/details?id=no.nordicsemi.android.mcp)<br>
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

# Arduino IDE Notes
You can build this skatch using Arduino IDE, but be sure to install the required dependencies:<br>
* You have to install the esp32 Board in Arduino IDE. You can find a tutorial following this [link]( https://randomnerdtutorials.com/installing-the-esp32-board-in-arduino-ide-windows-instructions/)
* You have to install the library PubSubClient v2.7
* You have to replace the BLE library provided by arduino-esp32 with this one https://github.com/shogunxam/ESP32-BLE-Arduino. The libray to replace should be located in the folder ~/.arduino15/packages/esp32/hardware/esp32/x.x.x/libraries/BLE for Unix users and in C:\Users\YourUserName\AppData\Local\Arduino15\packages\esp32\hardware\esp32\x.x.x\libraries\BLE for Windows users<br>
Build using the *Minimal SPIFFS* partition schema

# Home Assistant integration
This is a simple example of a package to manage a Nut Traker device.<br>
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
#Battery Sensor to resolve unknown device state
- platform: template
  sensors:
    bt_nut_battery:
      unit_of_measurement: '%'
      value_template: >
          {% if (is_state('sensor.bt_nut_upstairs_battery', 'unknown') or is_state('sensor.bt_nut_upstairs_battery', '-1')) %}
            unknown
          {% else %}
            {{ states('sensor.bt_nut_upstairs_battery')|int }}
          {% endif %}
      icon_template: >
        {% set battery_level = states.sensor.bt_nut_battery.state|default(0)|int %}
        {% set battery_round = (battery_level / 10) |int * 10 %}
        {% if battery_round >= 100 %}
          mdi:battery
        {% elif battery_round > 0 %}
          mdi:battery-{{ battery_round }}
        {% else %}
          mdi:battery-alert
        {% endif %}


###############################################
#Binary Sensors to resolve unknown device state

binary_sensor:
- platform: template
  sensors:
    ble_tracker_nut_mario:
      friendly_name: 'ble_tracker_nut'
      value_template: >
        {% if (is_state('sensor.bt_nut_upstairs_state', 'unknown') or is_state('sensor.bt_nut_upstairs_state', 'off')) %}
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
      {{ ((states.sensor.bt_nut_battery.state | int ) <= 20 ) and ( states.sensor.bt_nut_battery.state != 'unknown') and ( states.sensor.bt_nut_battery.state | int >= 0) }}
  action:
  - service: notify.telegram
    data_template:
      title: BLETracker
      message: "Nut battery at {{states.sensor.bt_nut_battery.state}}%"

```

## Licence
> THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
  SOFTWARE.

*If you like the content of this repo, please add a star! Thank you!*

# Support my work
If you like my work, please consider buying me a coffee. Thank you for your support! :grin:<br><br>
[![Buy me a coffee][buymeacoffee-shield]][buymeacoffee]

[buymeacoffee-shield]: https://www.buymeacoffee.com/assets/img/guidelines/download-assets-sm-2.svg
[buymeacoffee]: https://www.buymeacoffee.com/shogunxam
