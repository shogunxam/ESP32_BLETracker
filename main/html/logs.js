$(document).ready(function() {
  loadData();
  loadLogLevel();
});

function loadData() {
  now = new Date();
  var url = "/getlogsdata?time=" + now.getTime();
  $("#logmsg").text("Reading logs...");
  $("#logmsg").css("color","#46bd02");
  $("#logmsg").css("visibility","visible");
  setTimeout(loadData, 15000);
  $.ajax({
    url: url,
    type: 'GET',
    success: function(data ,s) {
      clearTimeout();
      $("#logs > tbody").remove();
      var table = $('#logs');
      table.append($("<tbody/>").css("font-size","13px"));
      data.forEach(function(item, index) {
          ttimestamp = $("<td/>").text(item.t).css("text-align","left").css("width","22%").css("white-space","nowrap");
          tmessage = $("<td/>").text(item.m).css("text-align","left").css("width","78%").css("white-space","nowrap");
          if(item.m.startsWith("Error")){
            tmessage.css("color","#d21f1b");
          }
          else if(item.m.startsWith("BLETracker")){
            tmessage.css("color","#3498db");
          }
        var row = $("<tr/>").append(ttimestamp);
        row.append(tmessage);
        table.append(row);
      });
      $("#logmsg").css("visibility","hidden");
    },
    error: function(a, b, c)
    {
      $("#logmsg").text("Error Reading logs!!! Retring in 15 seconds.");
      $("#logmsg").css("color","#d21f1b");
      $("#logmsg").css("visibility","visible");
    }
  });
}

function loadLogLevel() {
  now = new Date();
  var url = "/getconfigdata?time=" + now.getTime();
   $.get(url,function(data) {
    console.log(data);
    $('#loglevel').val(data.loglevel);
  });
}

function logLevelChanged(s)
{
	var data ={};
  data[s.id]=s.value;
$.ajax({
    url: "/logs",
    type: 'POST',
    data:data,
    success: function(data ,s) {
    	$("#logmsg").css("visibility","hidden");
      console.log("ok");
    },
    error: function(a, b, c)
    {
      $("#logmsg").text("Error saving new log level!!!");
      $("#logmsg").css("color","#d21f1b");
      $("#logmsg").css("visibility","visible");
    }
  });
}

$(function() {
  $('#refresh').click(function(e) {
    $("#logmsg").css("visibility","hidden");
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
