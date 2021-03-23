$(document).ready(function () {
  loadData();
  setInterval(loadData, 5000);
});

function readBattery(mac) {
  now = new Date();
  var url = "/updatebattery";
  $.get(url, { mac: mac })
    .done(function (data) {
    });
}

function timeConverter(UNIX_timestamp) {
  var a = new Date(UNIX_timestamp * 1000);
  var months = ['Jan', 'Feb', 'Mar', 'Apr', 'May', 'Jun', 'Jul', 'Aug', 'Sep', 'Oct', 'Nov', 'Dec'];
  var year = a.getFullYear();
  var month = months[a.getMonth()];
  var date = a.getDate();
  var hour = a.getHours();
  var min = a.getMinutes();
  var sec = a.getSeconds();
  var time = ("00" + date).slice(-2) + ' ' + month + ' ' + year + ' ' + ("00" + hour).slice(-2) + ':' + ("00" + min).slice(-2) + ':' + ("00" + sec).slice(-2);
  return time;
}

function loadData() {
  now = new Date();
  var url = "/getsysinfodata?time=" + now.getTime(); 
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
    tmac = $("<td/>").text(item.name ? item.name : item.mac);
    trssi = $("<td/>").text(item.rssi);
    if (data.battery) {
      tbtr = $("<td/>");
      div = '<div style="align-content: center;">'
        + '<p style="display : inline-block; width: fit-content; height: fit-content;">' + item.battery + '</p>'
        + '<input type="button" value="Refresh" id="rbtn_' + item.mac + '" class=btn onclick="readBattery(\'' + item.mac + '\')"style="font-size : 0.8em; padding: 5px; display : inline-block; width: fit-content; width: -moz-fit-content;height: fit-content;float: right;">'
        + '<p style="margin-top: 0px; font-size : 0.8em;">' + (item.bttime ? timeConverter(item.bttime) : '') + '</p>'
        + '</div>';
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