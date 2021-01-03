function sub(obj) {
  var fileName = obj.value.split('\\');
  document.getElementById('file-input').innerHTML = '   ' + fileName[fileName.length - 1];
};
$(function() {
$('form').submit(function(e) {
  e.preventDefault();
  var form = $('#upload_form')[0];
  var data = new FormData(form);
  $.ajax({
    url: '/update',
    type: 'POST',
    data: data,
    contentType: false,
    processData: false,
    xhr: function() {
      var xhr = new window.XMLHttpRequest();
      xhr.upload.addEventListener('progress', function(evt) {
        if (evt.lengthComputable) {
          var per = evt.loaded / evt.total;
          $('#prg').html('progress: ' + Math.round(per * 100) + '%');
          $('#bar').css('width', Math.round(per * 100) + '%');
          if(per==1)
          {
            var ftimeout=setTimeout(function(){window.location='/';},10000);
          }
        }
      }, false);
      return xhr;
    },
    success: function(d, s) {
      console.log('success!')
      $("#errormsg").text("Restarting...");
      $("#errormsg").css("visibility","visible");
      var ftimeout=setTimeout(function(){window.location='/';},10000);
    },
    error: function(a, b, c) {
    	$("#errormsg").css("visibility","visible");
    }
  });
});
});
