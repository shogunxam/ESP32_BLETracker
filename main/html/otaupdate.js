R"=====(
function sub(obj) {
  var fileName = obj.value.split('\\\\');
  document.getElementById('file-input').innerHTML = '   ' + fileName[fileName.length - 1];
}
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
        }
      }, false);
      return xhr;
    },
    success: function(d, s) {
      console.log('success!')
    },
    error: function(a, b, c) {}
  });
});
)====="