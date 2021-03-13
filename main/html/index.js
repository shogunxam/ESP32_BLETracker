$(document).ready(function () {
  loadData();
});

function loadData() {
  var ltstVer;
  var curVer;
  now = new Date();
  var url = "/getindexdata?time=" + now.getTime();
  $.when(
    $.get(url, function (data) {
      console.log(data);
      $('#gateway').text(data.gateway);
      if (!data.logs) {
        $('#logs').hide();
      }
      matches = data.ver.match(/(\d+(\.\d+)?)/);
      if (matches)
        curVer = Number(matches[0]);
      console.log("curVer " + curVer);
    }),
    $.get('https://api.github.com/repos/shogunxam/ESP32_BLETracker/releases/latest', function (data) {
      matches = data.tag_name.match(/(\d+(\.\d+)?)/);
      if(matches)
        ltstVer = Number(matches[0]);
      console.log("ltstVer " + ltstVer);
    })
  ).then(function () {
    if (ltstVer  && curVer && (curVer < ltstVer))
      $('#newver').show();
  });
}