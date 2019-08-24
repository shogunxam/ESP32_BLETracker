# ESP32 BLETracker
A simple example describing how to track a Bluetooth Low Energy device with an ESP32, the MQTT protocol and Home Assistant. Please note that the targeted device can't have a changing BLE address (normally called random instead of public address).  

Use PlatformIO to build and deploy this application.

You have to modify the config.h inserting the correct informations to connect to the WiFi and to the MQTT broker. 
The GATEWAY_NAME is used as Client ID to connect to the broker so be sure it's unique.

The application generates the following topics:
<LOCATION>/<GATEWAY_NAME>/<BLE_ADDRESS>/LWT payload: <online|offline>
<LOCATION>/<GATEWAY_NAME>/<BLE_ADDRESS> payload: { "state":<"on"|"off">,"rssi":<dBvalue>}
<LOCATION>/<GATEWAY_NAME>/<BLE_ADDRESS>/state payload: <"on"|"off">
<LOCATION>/<GATEWAY_NAME>/<BLE_ADDRESS>/rssi payload: <dBvalue>
<LOCATION>/<GATEWAY_NAME>/<BLE_ADDRESS>/sysinfo, payload: { "uptime":<timesinceboot>,"version":<versionnumber>,"SSID":<WiFiSSID>}

## Licence
> THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
  SOFTWARE.

*If you like the content of this repo, please add a star! Thank you!*
