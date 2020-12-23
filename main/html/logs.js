$(document).ready(function() {
  loadData();
});

function loadData() {
  now = new Date();
  var url = "/getlogsdata?time=" + now.getTime();

  $.get(url, function(data) {
    console.log(data);
    $("#logs > tbody").remove();
    var table = $('#logs');
    table.append($("<tbody/>").css("font-size","13px"));
    data.forEach(function(item, index) {
        ttimestamp = $("<td/>").text(item.timestamp).css("text-align","left").css("width","22%").css("white-space","nowrap");
        tmessage = $("<td/>").text(item.message).css("text-align","left").css("width","78%").css("white-space","nowrap");
        if(item.message.startsWith("Error"))
      	  tmessage.css("color","#d21f1b");
        else if(item.message.startsWith("--"))
      	  tmessage.css("color","#3498db");
      var row = $("<tr/>").append(ttimestamp);
      row.append(tmessage);
      table.append(row);
      console.log(table);
    });
  });
}

$(function() {
  $('#refresh').click(function(e) {
    loadData();
    e.preventDefault();
  });
  $('#clear').click(function(e) {
    $.ajax({
      type: "GET",
      url: "/eraselogs",
      success: function() {
        console.log('success!');
        loadData();
      }
    });
    e.preventDefault();
  })
});
