R"=====(
<form name='indexForm'>
  <h1 id='gateway'></h1>
  <h2>System Information</h2>
  <div class="grid-container">
    <div class="grid-label">Firmware Version</div>
    <div class="grid-value" id="firmware"></div>
    <div class="grid-label">Build Time</div>
    <div class="grid-value" id="build"></div>
    <div class="grid-label">Free Memory</div>
    <div class="grid-value" id="memory"></div>
    <div class="grid-label">Uptime</div>
    <div class="grid-value" id="uptime"></div>
    <div class="grid-label">SSID</div>
    <div class="grid-value" id="ssid"></div>
  </div>
  <br>
  <h2>Devices</h2>
  <div id="devListContainer" name="devListContainer" class="table-wrapper">
    <table id="devices" style='width:100%' class="scroll-table">
      <thead>
        <th>Device</th>
        <th>RSSI</th>
        <th id="c1">Battery</th>
        <th>State</th>
      </thead>
    </table>
  </div>
  <input type=button onclick="window.open('/reset','_self')" class='fatbtn' value="Reset">
  <br><input type=button onclick="window.open('/','_self')" class=fatbtn value="Back">
  <br><br><label>by Shogunxam</label>
</form>
)====="