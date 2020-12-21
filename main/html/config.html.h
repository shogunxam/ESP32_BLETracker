R"=====(
<form id="form" name="config" action="">
  <fieldset>
    <legend>MQTT Broker:</legend>
    <table>
      <tr>
        <td><label for="mqttsrvr">Address:</label></td>
        <td><input type="text" id="mqttsrvr" name="mqttsrvr"></td>
      </tr>
      <tr>
        <td><label for="mqttport">Port:</label></td>
        <td><input type="number" id="mqttport" name="mqttport"></td>
      </tr>
      <tr>
        <td><label for="mqttusr">User:</label></td>
        <td><input type="text" id="mqttusr" name="mqttusr"></td>
      </tr>
      <tr>
        <td><label for="mqttpwd">Password:</label></td>
        <td><input type="password" id="mqttpwd" name="mqttpwd"></td>
      </tr>
    </table>
  </fieldset>
  <fieldset>
    <legend>Devices to track</legend>
    <label for="scanperiod" style="width:auto height:auto">Scan period:</label>
    <input type="number" id="scanperiod" name="scanperiod" style="width:20%;height:auto"><br>
    <input type="checkbox" id="whiteList" name="whiteList" style="width:auto;height:auto">
    <label for="whiteList">Enable Track Whitelist</label><br>
    <div id="devListContainer" name="devListContainer" class="table-wrapper">
      <table id="devList" style="margin-left: auto; margin-right: auto;" class="scroll-table">
        <thead>
          <tr>
            <th>Battery</th>
            <th>Device</th>
            <th>Action</th>
          </tr>
        </thead>
        <tbody>
        </tbody>
      </table>
    </div>
  </fieldset>
  <input type="text" id="newmac" name="newmac" style="width:70%" placeholder="Insert device MAC here" maxlength="12" onkeydown="this.value=validateMacDigit(this)" onpaste="var e=this; setTimeout(function(){e.value=validateMac(e.value);}, 4);">
  <input type="button" id="addMac" value="Add" class=btn style="width:19%">
  <input style="width:49%" type="submit" id="undo" value="Undo Changes" class=btn>
  <input style="width:49%" type="submit" id="factory" value="Factory Values" class=btn>
  <input type="submit" id="configure" value="Submit" class=fatbtn>
  <br><input type=button onclick="window.open('/','_self')" class=fatbtn value="Back">
  <br><br><label>by Shogunxam</label>
</form>)
)====="