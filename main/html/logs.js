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
    table.append($("<tbody/>"));
    data.forEach(function(item, index) {
        ttimestamp = $("<td/>").text(item.timestamp).css("text-align","left").css("width","30%").css("white-space","nowrap");
        tmessage = $("<td/>").text(item.message).css("text-align","left").css("width","70%").css("white-space","nowrap");
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
