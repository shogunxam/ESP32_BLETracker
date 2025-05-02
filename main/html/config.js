
$(document).ready(function () {
  let factory = getUrlParameter('factory');
  var param = null;
  if (typeof factory !== 'undefined' && factory === 'true') {
    param = {factory: true};
  }

  $('#factoryBtn').click(function (e) {
    if (confirm("Are you sure you want to load the default configuration values?")) {
      window.location.href = '/config?factory=true';
    }
    e.preventDefault();
  });

  getData("/getconfigdata", param, PopulatePage, function (xhr, status, error) {
    console.error('Errore nel caricamento dei dati:', status, error);
  });

  $('#saveBtn').click(function (e) {
    e.preventDefault();

    // Crea un oggetto FormData per raccogliere tutti i dati del form
    const formData = new FormData();

    // Dati di rete e MQTT
    ['ssid', 'wifipwd', 'gateway', 'wbsusr', 'wbspwd', 'mqttsrvr', 'mqttusr', 'mqttpwd']
      .forEach(id => formData.append(id, $(`#${id}`).val()));
    
    // Impostazioni di scansione con valori predefiniti
    formData.append('mqttport', $('#mqttport').val() || '1883');
    formData.append('scanperiod', $('#scanperiod').val() || '10');
    formData.append('maxNotAdvPeriod', $('#maxNotAdvPeriod').val() || '120');
    
    // Checkbox (aggiungi 'true' se selezionate)
    formData.append('manualscan', $('#manualscan').is(':checked') ? 'true' : '');
    formData.append('whiteList', $('#whiteList').is(':checked') ? 'true' : '');

    // Aggiungi i dispositivi tracciati dalla tabella
    [...$('#devices-table tbody tr')].forEach(tr => {
      const cells = tr.cells;
      if (cells.length >= 3) {
        const mac = cells[0].textContent.replace(/:/g, '');
        const desc = cells[1].querySelector('input').value || '';
        const battery = cells[2].querySelector('input[type="checkbox"]').checked;
        formData.append(mac + '[desc]', desc);
        if (battery) formData.append(mac + '[batt]', 'true');
      }
    });

    // Mostra l'alert di salvataggio in corso
    $('#message').removeClass('alert-success alert-error').addClass('alert-info').text('Saving configuration...').show();

    // Converte FormData in URLSearchParams per l'invio
    var searchParams = new URLSearchParams();
    for (var pair of formData.entries()) {
      searchParams.append(pair[0], pair[1]);
    }

    sendData("/updateconfig", searchParams.toString(),
      function () {
        // Mostra un messaggio di successo
        $('#message').removeClass('alert-info alert-error').addClass('alert-success').text('Configuration saved successfully! Restarting device...').show();

        // Reindirizza alla pagina di riavvio dopo un breve ritardo
        setTimeout(function () {
          window.location.href = '/restart';
        }, 2000);
      },
      function (xhr, status, error) {
        // Mostra un messaggio di errore
        $('#message').removeClass('alert-info alert-success').addClass('alert-error')
          .text('Error saving configuration: ' + (xhr.responseText || error)).show();
      }
    );

  });


  $('#resetBtn').click(function (e) {
    window.location.href = '/config';
    e.preventDefault();
  });

  $('#addDeviceBtn').click(function () {
    const macInput = $('#newDeviceAddr').val();
    const cleanMac = macInput.replace(/[^0-9A-F]/g, '').toUpperCase();
    const description = $('#newDeviceDesc').val();  
    const formattedMac = formatMacAddress(cleanMac);
    const device = { address: formattedMac, description, readBattery: false };
    $('#devices-table tbody').append(createDeviceRow(device));
    if (window.innerWidth < 768) createDeviceCard(device);
    $('#newDeviceAddr').val('');
    $('#newDeviceDesc').val('');
    $('#addDeviceBtn').prop('disabled', true);
  });

  $('#newDeviceAddr').on('input', function (event){
    const rawInput = $(this).val().toUpperCase().replace(/[^0-9A-F]/g, '');
    $(this).val(formatMacAddress(rawInput));
    $('#addDeviceBtn').prop('disabled', rawInput.length !== 12);
  });

  updateDevicesView();

  // Aggiungi un listener per il ridimensionamento della finestra
  $(window).resize(function () {
    updateDevicesView();
  });

});

function PopulatePage(data) {
  const fields = {
    wbsusr: 'wbs_user',
    wbspwd: 'wbs_pwd',
    ssid: 'wifi_ssid',
    wifipwd: 'wifi_pwd',
    gateway: 'gateway',
    mqttsrvr: 'mqtt_address',
    mqttport: 'mqtt_port',
    mqttusr: 'mqtt_usr',
    mqttpwd: 'mqtt_pwd',
    scanperiod: 'scanPeriod',
    maxNotAdvPeriod: 'maxNotAdvPeriod',
  };

  for (const field in fields) {
    $(`#${field}`).val(data[fields[field]]);
  }

  $('#mqttEnabled').prop('checked', data.mqtt_enabled);
  $('#whiteList').prop('checked', data.whiteList);
  $('#manualscan').prop('checked', data.manualscan);

  if (!data.mqtt_enabled) {
    $('button.tab-btn[onclick="openTab(event, \'mqtt\')"]').hide();
    $('#mqtt').hide();
  }

  $('#devices-table tbody').empty();

  // Populate tracked devices from trk_list
  if (data.trk_list) {
    for (const mac in data.trk_list) {
      const device = {
        address: formatMacAddress(mac),
        description: data.trk_list[mac].desc || '',
        readBattery: data.trk_list[mac].battery || false
      };

      $('#devices-table tbody').append(createDeviceRow(device));
    }
  }

  updateDevicesView(true);
}

function openTab(evt, tabName) {
  const tabcontent = document.querySelectorAll(".tab-content");
  tabcontent.forEach(el => el.classList.remove("active"));

  const tablinks = document.querySelectorAll(".tab-btn");
  tablinks.forEach(el => el.classList.remove("active"));

  document.getElementById(tabName)?.classList.add("active");
  evt.currentTarget?.classList.add("active");
}

// Helper function to format MAC address with separators
const formatMacAddress = (mac) => {
  const cleanMac = mac.replace(/[^0-9A-F]/gi, '').toUpperCase();
  return cleanMac.match(/.{1,2}/g)?.join(':') || '';
};

const getUrlParameter = (sParam) => {
  const urlParams = new URLSearchParams(window.location.search);
  const param = urlParams.get(sParam);
  return param === null ? true : decodeURIComponent(param);
};

const togglePasswordVisibility = (fieldId) => {
  const field = document.getElementById(fieldId);
  const toggleIcon = $(field).siblings('.toggle-password').find('i');
  const isPassword = field.type === 'password';

  field.type = isPassword ? 'text' : 'password';
  toggleIcon.toggleClass('fa-eye', !isPassword).toggleClass('fa-eye-slash', isPassword);
};

const createDeviceRow = (device) => {
  const row = document.createElement('tr');
  row.className = 'device-row';
  row.dataset.mac = device.address; // Use dataset for data attributes

  row.innerHTML = `
    <td>${device.address}</td>
    <td><input type="text" name="${device.address}_desc" value="${device.description || ''}" placeholder="Description" maxLength="20"></td>
    <td>
      <label class="toggle-switch">
        <input type="checkbox" name="${device.address}_batt" ${device.readBattery ? 'checked' : ''}>
        <span class="toggle-slider"></span>
      </label>
    </td>
    <td><button type="button" class="btn btn-danger btn-icon" title="Delete device"><i class="fas fa-trash-alt"></i></button></td>
  `;

  row.querySelector('button').onclick = () => {
    if (confirm('Are you sure you want to delete this device?')) {
      row.remove();
      document.querySelector(`.device-card[data-mac="${device.address}"]`)?.remove(); // Optional chaining
    }
  };

  return row;
};

const createDeviceCard = (device) => {
  const card = document.createElement('div');
  card.className = 'device-card';
  card.dataset.mac = device.address;

  card.innerHTML = `
    <div class="device-card-header"><h3>${device.address}</h3></div>
    <div class="device-card-content">
      <div class="card-item"><strong>Description:</strong><input type="text" name="${device.address}_desc_mobile" value="${device.description || ''}" placeholder="Description" maxLength="20" class="mobile-input"></div>
      <div class="card-item"><strong>Read Battery:</strong>
        <label class="toggle-switch">
          <input type="checkbox" name="${device.address}_batt_mobile" ${device.readBattery ? 'checked' : ''}>
          <span class="toggle-slider"></span>
        </label>
      </div>
      <div class="card-item card-actions"><button type="button" class="btn btn-danger"><i class="fas fa-trash-alt"></i> Delete Device</button></div>
    </div>
  `;

  const descInput = card.querySelector(`input[name="${device.address}_desc_mobile"]`);
  descInput.oninput = () => document.querySelector(`input[name="${device.address}_desc"]`).value = descInput.value;

  const batteryCheckbox = card.querySelector(`input[name="${device.address}_batt_mobile"]`);
  batteryCheckbox.onchange = () => document.querySelector(`input[name="${device.address}_batt"]`).checked = batteryCheckbox.checked;

  card.querySelector('button').onclick = () => {
    if (confirm('Are you sure you want to delete this device?')) {
      card.remove();
      document.querySelector(`tr[data-mac="${device.address}"]`)?.remove();
    }
  };

  document.getElementById('devices-cards').appendChild(card);
};

const updateDevicesView = (isInitial = false) => {
  const isMobile = window.innerWidth < 768;
  $('#devices-table').toggle(!isMobile);
  $('#devices-cards').toggle(isMobile);

  if (isMobile && (isInitial ||!$('#devices-cards').children().length)) {
    $('#devices-cards').empty();
    $('#devices-table tbody tr').each(function () {
      const mac = $(this).data('mac'); // Use .data()
      if (mac) {
        createDeviceCard({
          address: mac,
          description: $(this).find(`input[name="${mac}_desc"]`).val() || '', // Use template literal
          readBattery: $(this).find(`input[name="${mac}_batt"]`).prop('checked') // Use .prop()
        });
      }
    });
  }
};
