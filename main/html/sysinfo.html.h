R"=====(
<form name='indexForm'>
  <h1 id='gateway'></h1>
  <h2>System Information</h2>
  <table style='width:100%'>
    <tr id='r1'>
      <th>Firmware Version</th>
      <td id="firmware"></td>
    </tr>
    <tr id='r2'>
      <th>Build Time</th>
      <td id="build"></td>
    </tr>
    <tr id='r3'>
      <th>Free Memory</th>
      <td id="memory"></td>
    </tr>
    <tr id='r4'>
      <th>Uptime</th>
      <td id="uptime"></td>
    </tr>
    <tr id='r5'>
      <th>SSID</th>
      <td id="ssid"></td>
    </tr>
  </table>
  <br>
  <h2>Devices</h2>
  <table id="devices" style='width:100%'>
    <tr>
      <th>Device</th>
      <th>RSSI</th>
      <th id="c1">Battery</th>
      <th>State</th>
    </tr>
  </table>
  <input type=button onclick="window.open('/reset','_self')" class='btn' value="Reset">
  <br><input type=button onclick="window.open('/','_self')" class=btn value="Back">
  <br><br><label>by Shogunxam</label>
</form>
)====="