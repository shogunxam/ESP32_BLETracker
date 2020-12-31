$(document).ready(function() {
    loadData();
  });

function loadData() {
  now = new Date();
  var url = "/getindexdata?time=" + now.getTime();
  $.get(url, function(data) {
    console.log(data);
    $('#gateway').text(data.gateway);
    if(!data.logs){
        $('#logs').hide();
    }
  });
}