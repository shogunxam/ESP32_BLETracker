R"=====(
$(document).ready(function() {
  now = new Date();
  var url = "/getserverinfodata?time="+now.getTime();
  $.get(url,function(data) {
    $('#gateway').text(data.gateway);
    $('#firmware').text(data.firmware);
    $('#uptime').text(data.uptime);
    $('#ssid').text(data.ssid);
    if (data.build) $('#build').text(data.build);
    else $('#r2').hide();
    if (data.memory) $('#memory').text(data.memory);
    else $('#r3').hide();
    if (!data.battery)
      $('#c1').hide();
    var table = $('#devices');
    data.devices.forEach(function(item, index) {
      tmac = $("<td/>").text(item.mac);
      trssi = $("<td/>").text(item.rssi);
      if (data.battery)
        tbtr = $("<td/>").text(item.battery);
      tstate = $("<td/>").text(item.state);
      var row = $("<tr/>").append(tmac);
      row.append(trssi);
      if (data.battery)
        row.append(tbtr);
      row.append(tstate);
      table.append(row);
      console.log(table);
    });
  });
});
)====="