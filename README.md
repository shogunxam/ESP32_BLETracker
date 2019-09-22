# ESP32 BLETracker
A simple example describing how to track a Bluetooth Low Energy device with an ESP32, the MQTT protocol and Home Assistant. Please note that the targeted device can't have a changing BLE address (normally called random instead of public address).  

Use PlatformIO to build and deploy this application.<br>
You have to modify the config.h inserting the correct informations to connect to the WiFi and to the MQTT broker.<br>
The GATEWAY_NAME is used as Client ID to connect to the broker so be sure it's unique.<br>
If many devices are discovered the battery level check can be very slow causing frequent Wi-Fi disconnection so that I have introduced a whitelist containing the Mac Address of the devices to check. The whitelist is in the form:<br>
BLE_BATTERY_WHITELIST       "XXXXXXXXX","YYYYYYYY"<br>
Mac Address have to be uppercase without ":" or "-" i.e "CA67347FD139"

The application generates the following topics:<br>
&lt;LOCATION&gt;/&lt;GATEWAY_NAME&gt;/&lt;BLE_ADDRESS&gt;/LWT payload: &lt;online|offline&gt;<br>
&lt;LOCATION&gt;/&lt;GATEWAY_NAME&gt;/&lt;BLE_ADDRESS payload: { "state":&lt;"on"|"off"&gt;,"rssi":&lt;dBvalue&gt;,"battery":&lt;batterylevel&gt;}<br>
&lt;LOCATION&gt;/&lt;GATEWAY_NAME&gt;/&lt;BLE_ADDRESS&gt;/state payload: &lt;"on"|"off"&gt;<br>
&lt;LOCATION&gt;/&lt;GATEWAY_NAME&gt;/&lt;BLE_ADDRESS&gt;/rssi payload: &lt;dBvalue&gt;<br>
&lt;LOCATION&gt;/&lt;GATEWAY_NAME&gt;/&lt;BLE_ADDRESS&gt;/battery payload: &lt;batterylevel&gt;<br>
&lt;LOCATION&gt;/&lt;GATEWAY_NAME&gt;/&lt;BLE_ADDRESS&gt;/sysinfo, payload: { "uptime":&lt;timesinceboot&gt;,"version":&lt;versionnumber&gt;,"SSID":&lt;WiFiSSID&gt;}

## Licence
> THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
  SOFTWARE.

*If you like the content of this repo, please add a star! Thank you!*
