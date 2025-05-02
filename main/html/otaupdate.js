function sub(obj) {
  var fileName = obj.value.split('\\');
  document.getElementById('file-input').innerHTML = '   ' + fileName[fileName.length - 1];
};

// Carica le informazioni di sistema
let currentVersion = null;

function checkForUpdates() {
  $('#updateResult').show();
  $('#updateStatusContainer').show();
  $('#updateStatus').text('Checking for updates...');

  $.get('https://api.github.com/repos/shogunxam/ESP32_BLETracker/releases/latest')
    .done(function (gitData) {
      if (gitData && gitData.tag_name) {
        let matches = gitData.tag_name.match(/(\d+(\.\d+)?)/);
        if (matches) {
          let latestVersion = parseFloat(matches[0]);
          $('#latestVersion').text(gitData.tag_name);

          if (currentVersion && latestVersion) {
            if (currentVersion < latestVersion) {
              $('#updateStatus').html('<span class="status-update-available">Update available!</span>');
              $('#downloadLink').attr('href', gitData.html_url).show();
            } else {
              $('#updateStatus').html('<span class="status-up-to-date">Your firmware is up to date</span>');
              $('#downloadLink').hide();
            }
          } else {
            $('#updateStatus').text('Cannot compare versions');
            $('#downloadLink').hide();
          }
        } else {
          $('#updateStatus').text('Could not parse version number');
          $('#downloadLink').hide();
        }
      } else {
        $('#updateStatus').text('Could not retrieve release information');
        $('#downloadLink').hide();
      }
    })
    .fail(function (error) {
      $('#updateStatus').html('<span class="status-error">Error checking for updates: ' + error.statusText + '</span>');
      $('#downloadLink').hide();
    });
}

$(function () {

  document.getElementById('firmware').addEventListener('change', function (e) {
    var fileName = e.target.files[0] ? e.target.files[0].name : 'Choose file...';
    document.getElementById('file-name').textContent = fileName;
  });

  // Carica le informazioni di sistema una sola volta all'avvio
  getData('/getsysinfodata', null,
    function (data) {
      // Memorizza e mostra la versione del firmware
      if (data && data.firmware) {
        $('#version').text(data.firmware);

        // Estrai e memorizza il numero di versione per uso futuro
        let matches = data.firmware.match(/(\d+(\.\d+)?)/);
        if (matches) {
          currentVersion = parseFloat(matches[0]);
        }
        checkForUpdates();
      } else {
        $('#version').text('Unknown');
      }
    },
    function () {
    $('#version').text('Error loading firmware info');
    }
  )
 
  $('#uploadBtn').click(function (e) {
    e.preventDefault();

    // Controlla se è stato selezionato un file
    var fileInput = $('#firmware')[0];
    if (!fileInput.files.length) {
      $('#message').removeClass('alert-success').addClass('alert-error').text('Please select a firmware file').show();
      return;
    }

    var form = $('#uploadForm')[0];
    var formData = new FormData(form);

    // Mostra la barra di progresso
    $('.progress-container').show();
    $('#message').removeClass('alert-error').addClass('alert-info').text('Uploading firmware...').show();

    // Invia la richiesta
    $.ajax({
      url: '/update',
      type: 'POST',
      data: formData,
      contentType: false,
      processData: false,
      xhr: function () {
        var xhr = new window.XMLHttpRequest();
        xhr.upload.addEventListener('progress', function (evt) {
          if (evt.lengthComputable) {
            var percent = Math.round((evt.loaded / evt.total) * 100);
            $('#progressBar').css('width', percent + '%');
            $('#progressPercent').text(percent + '%');
          }
        }, false);
        return xhr;
      },
      success: function (response) {
        $('#message').removeClass('alert-info alert-error').addClass('alert-success').text('Firmware uploaded successfully. Device is restarting...').show();
        setTimeout(function () {
          window.location.href = '/';
        }, 10000);
      },
      error: function (xhr, status, error) {
        $('#message').removeClass('alert-info alert-success').addClass('alert-error').text('Upload failed: ' + error).show();
        $('.progress-container').hide();
      }
    });
  });
});
