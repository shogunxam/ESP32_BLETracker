$(document).ready(function() {
  loadData();
});

function loadData() {
  now = new Date();
  var url = "/getlogsdata?time=" + now.getTime();
  $("#errormsg").text("Reading logs...");
  $("#errormsg").css("color","#46bd02");
  $("#errormsg").css("visibility","visible");
  $.ajax({
    url: url,
    type: 'GET',
    success: function(data ,s) {
      console.log(data);
      $("#logs > tbody").remove();
      var table = $('#logs');
      table.append($("<tbody/>").css("font-size","13px"));
      data.forEach(function(item, index) {
          ttimestamp = $("<td/>").text(item.t).css("text-align","left").css("width","22%").css("white-space","nowrap");
          tmessage = $("<td/>").text(item.m).css("text-align","left").css("width","78%").css("white-space","nowrap");
          if(item.m.startsWith("Error"))
            tmessage.css("color","#d21f1b");
          else if(item.m.startsWith("BLETracker"))
            tmessage.css("color","#3498db");
        var row = $("<tr/>").append(ttimestamp);
        row.append(tmessage);
        table.append(row);
        console.log(table);
      });
      $("#errormsg").css("visibility","hidden");
    },
    error: function(a, b, c)
    {
      $("#errormsg").text("Error Reading logs!!!");
      $("#errormsg").css("color","#d21f1b");
      $("#errormsg").css("visibility","visible");
    }
  });
}

$(function() {
  $('#refresh').click(function(e) {
    $("#errormsg").css("visibility","hidden");
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
