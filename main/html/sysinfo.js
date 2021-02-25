$(document).ready(function () {
  loadData();
  setInterval(loadData, 5000);
});

function readbBattery(mac)
{
  now = new Date();
  var url = "/updatebattery";
  $.get(url, {mac: mac})
  .done(function (data) {
  });
}

function loadData() {
  now = new Date();
  var url = "/getsysinfodata?time=" + now.getTime();
  //data = JSON.parse('{"gateway":"BLETracker2","firmware":"2.1R","build":"2021-01-19T21:50:24","memory":"23560 bytes","uptime":"0.02:48:15","ssid":"HURRICANE1","battery":true,"devices":[{"mac":"CA67347FD139","rssi":-100,"battery":100,"state":"On"},{"mac":"D1667690EAA2","rssi":-100,"battery":-1,"state":"Off"},{"mac":"EB3B442D6CCE","rssi":-100,"battery":-1,"state":"Off"}]}');
  $.get(url, function (data) {
  $('#gateway').text(data.gateway);
  $('#firmware').text(data.firmware);
  $('#uptime').text(data.uptime);
  $('#ssid').text(data.ssid);
  if (data.build) { $('#build').text(data.build); }
  else { $('#buildlbl').hide(); $('#build').hide(); }
  if (data.memory) { $('#memory').text(data.memory); }
  else { $('#memorylbl').hide(); $('#memory').hide(); }
  if (!data.battery) {
    $('#c1').hide();
  }
  $("#devices > tbody").remove();
  var table = $('#devices');
  table.append($("<tbody/>"));

  data.devices.forEach(function (item, index) {
    tmac = $("<td/>").text(item.mac);
    trssi = $("<td/>").text(item.rssi);
    if (data.battery) {
      tbtr = $("<td/>");
      div = '<div>' +
       '<p style="display : inline-block; width: fit-content; height: fit-content;float: left;">'+ item.battery +'</p>'+
        '<input type="button" value="Refresh" id="rbtn_' + item.mac + '" class=btn onclick="readbBattery(\'' + item.mac + '\')"style="font-size : 0.8em; padding : 5px; display : inline-block; width: fit-content; height: fit-content;float: right;">';  
      tbtr.append(div);
    }
    tstate = $("<td/>").text(item.state);
    var row = $("<tr/>").append(tmac);
    row.append(trssi);
    if (data.battery) {
      row.append(tbtr);
    }
    row.append(tstate);
    table.append(row);
    console.log(table);
  });
  });
}