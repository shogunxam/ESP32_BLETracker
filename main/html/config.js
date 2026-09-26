const locallyPairedDevices = new Map();
let discoveryRefreshInFlight = false;

const deviceKey = address => address.replace(/[^0-9a-f]/gi, '').toUpperCase();

$(document).ready(() => {

  // Carica il Tab MQTT se disponibile
  getData(
    '/mqtt_config_fragment',
    null,
    (html, status, xhr) => {
      if(xhr.status === 204) return;
      $('#mqtt-btn').show();
      document.getElementById('mqtt').innerHTML = html;
    },
    (xhr, status, error) => {
      console.error('Errore nel caricamento del fragment UDP:', error);
      // Puoi anche mostrare un messaggio all'utente se vuoi
    }
  );

  // Carica il Tab UDP se disponibile
  getData(
    '/udp_config_fragment',
    null,
    (html, status, xhr) => {
      // Success callback: inserisci l'HTML ricevuto nel DOM
      if(xhr.status === 204) return;
      $('#udp-btn').show();
      document.getElementById('udp').innerHTML = html;
    },
    (xhr, status, error) => {
      console.error('Errore nel caricamento del fragment UDP:', error);
      // Puoi anche mostrare un messaggio all'utente se vuoi
    }
  );

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

  $('#discoveryMode').change(function () {
    const enabled = $(this).is(':checked');
    sendData(`/api/discovery?state=${enabled ? 'on' : 'off'}`, null,
      function () {
        if (enabled) {
          refreshDiscoveryDevices();
        } else {
          getData('/getconfigdata', null, data => PopulatePage(data));
        }
      },
      function () {
        $('#discoveryMode').prop('checked', !enabled);
        showPopup('Unable to change discovery mode');
      }
    );
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
        if (tr.dataset.discovery === 'true' && !cells[2].querySelector('input').checked) return;
        const mac = cells[0].textContent.replace(/[:-]/g, '');
        const desc = cells[1].querySelector('input').value || '';
        formData.append(mac + '[desc]', desc);

        if (tr.dataset.discovery !== 'true') {
          const battery = cells[2].querySelector('input[type="checkbox"]').checked;
          if (battery) formData.append(mac + '[batt]', 'true');
        }
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
    const formattedMac = formatDeviceId(cleanMac);
    const device = { address: formattedMac, description, readBattery: false };
    $('#devices-table tbody').append(createDeviceRow(device));
    if (window.innerWidth < 768) createDeviceCard(device);
    $('#newDeviceAddr').val('');
    $('#newDeviceDesc').val('');
    $('#addDeviceBtn').prop('disabled', true);
  });

  $('#newDeviceAddr').on('input', function (event){
    const rawInput = $(this).val().toUpperCase().replace(/[^0-9A-F]/g, '');
    $(this).val(formatDeviceId(rawInput));
    $('#addDeviceBtn').prop('disabled', rawInput.length !== 12 && rawInput.length !== 40);
  });

  updateDevicesView();

  // Aggiungi un listener per il ridimensionamento della finestra
  $(window).resize(function () {
    updateDevicesView();
  });

  setInterval(refreshDiscoveryDevices, 5000);

});

function PopulatePage(data) {
  data.trk_list = data.trk_list || {};
  locallyPairedDevices.forEach((device, address) => {
    const trackedAddress = Object.keys(data.trk_list).find(mac => deviceKey(mac) === address);
    if (device.paired && !trackedAddress) {
      // Preserva il valore impostato nell'oggetto locale (false di default)
      data.trk_list[device.address] = { desc: device.description, battery: device.readBattery };
    }
  });

  window.trackedDeviceList = data.trk_list;

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

  $('#whiteList').prop('checked', data.whiteList);
  $('#manualscan').prop('checked', data.manualscan);
  $('#devices-table thead th:nth-child(2)').text('Description');
  $('#devices-table thead th:nth-child(3)').text('Read Battery');
  $('#devices-table th:last-child, #devices-table td:last-child').show();

  $('#devices-table tbody').empty();

  // Populate tracked devices from trk_list
  if (data.trk_list) {
    for (const mac in data.trk_list) {
      const device = {
        address: formatDeviceId(mac),
        description: data.trk_list[mac].desc || '',
        readBattery: data.trk_list[mac].battery || false
      };

      $('#devices-table tbody').append(createDeviceRow(device));
    }
  }

  updateDevicesView(true);
  refreshDiscoveryDevices(true);
}

function refreshDiscoveryDevices(force = false) {
  if ((!force && !$('#discoveryMode').is(':checked')) || discoveryRefreshInFlight) return;
  discoveryRefreshInFlight = true;
  getData('/api/devices', null, data => {
    discoveryRefreshInFlight = false;
    updateDiscoveryView(data);
  }, function (xhr, status, error) {
    discoveryRefreshInFlight = false;
    console.error('Error loading discovery devices:', status, error);
  });
}

function updateDiscoveryView(data) {
  $('#discoveryMode').prop('checked', data.discovery === true);
  if (!data.discovery) return;

  $('#devices-table thead th:nth-child(2)').text('Advertised Name');
  $('#devices-table thead th:nth-child(3)').text('Pair');
  $('#devices-table th:last-child, #devices-table td:last-child').hide();
  $('#devices-table tbody').empty();

  const devices = new Map((data.devices || []).map(device => {
    const address = formatDeviceId(device.mac);
    return [deviceKey(address), { ...device, address }];
  }));
  
  // Sincronizza i dispositivi locali in fase di accoppiamento temporaneo
  locallyPairedDevices.forEach((pending, key) => {
    if (!devices.has(key)) {
      devices.set(key, { ...pending, address: pending.address });
    }
  });

  devices.forEach(device => {
    const key = deviceKey(device.address);
    const pending = locallyPairedDevices.get(key);
    
    // Il dispositivo è accoppiato se è già whitelisted sul server OPPURE se l'utente lo ha cliccato localmente
    const pairedOnServer = device.whitelisted === true;
    const isPaired = pairedOnServer || (pending?.paired === true);

    const row = createDeviceRow({
      address: device.address,
      description: pending ? pending.description : (device.name || ''),
      discovery: true,
      paired: isPaired,
      whitelisted: pairedOnServer,
      // Manteniamo il valore reale della batteria che arriva dal server (se presente)
      readBattery: pending ? pending.readBattery : (device.battery !== undefined ? device.battery : false)
    });
    $('#devices-table tbody').append(row);
  });
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
  row.dataset.mac = device.address;
  row.dataset.discovery = device.discovery ? 'true' : 'false';

  // LOGICA TOGGLE SEPARATA: 
  // In Discovery controlla lo stato dell'accoppiamento (paired)
  // In modalità Normale controlla lo stato della batteria (readBattery)
  const isChecked = device.discovery ? (device.paired ? 'checked' : '') : (device.readBattery ? 'checked' : '');
  const isDisabled = device.discovery && device.whitelisted ? 'disabled' : '';

  row.innerHTML = `
    <td>${device.address}</td>
    <td><input type="text" name="${device.address}_desc" value="${device.description || ''}" placeholder="Description" maxLength="20" ${device.discovery ? 'readonly' : ''}></td>
    <td>
      <label class="toggle-switch">
        <input type="checkbox" name="${device.address}_${device.discovery ? 'pair' : 'batt'}" ${isChecked} ${isDisabled}>
        <span class="toggle-slider"></span>
      </label>
    </td>
    <td><button type="button" class="btn btn-danger btn-icon" title="Delete device"><i class="fas fa-trash-alt"></i></button></td>
  `;
  if (device.discovery) row.cells.hidden = true;

  row.querySelector('button').onclick = () => {
    if (confirm('Are you sure you want to delete this device?')) {
      row.remove();
      document.querySelector(`.device-card[data-mac="${device.address}"]`)?.remove();
    }
  };

  const pairCheckbox = row.querySelector('input[type="checkbox"]');
  if (device.discovery) {
    pairCheckbox.onchange = () => {
      // Quando accoppiamo un nuovo dispositivo localmente, NON forziamo la lettura della batteria a true di default
      locallyPairedDevices.set(deviceKey(device.address), {
        address: device.address,
        description: row.querySelector('input[type="text"]').value,
        discovery: true,
        paired: pairCheckbox.checked,
        readBattery: false // Di default la lettura della batteria a livello UI rimane disattivata (off) al ritorno in modalità normale
      });
      updateDevicesView(true);
    };

    row.querySelector('input[type="text"]').oninput = event => {
      let pending = locallyPairedDevices.get(deviceKey(device.address));
      if (!pending) {
        pending = { address: device.address, discovery: true, paired: pairCheckbox.checked, readBattery: false };
        locallyPairedDevices.set(deviceKey(device.address), pending);
      }
      pending.description = event.target.value;
    };
  }

  return row;
};

const createDeviceCard = (device) => {
  const card = document.createElement('div');
  card.className = 'device-card';
  card.dataset.mac = device.address;

  card.innerHTML = `
    <div class="device-card-header"><h3>${device.address}</h3></div>
    <div class="device-card-content">
      <div class="card-item"><strong>Description:</strong><input type="text" name="${device.address}_desc_mobile" value="${device.description || ''}" placeholder="Description" maxLength="20" class="mobile-input" ${device.discovery ? 'readonly' : ''}></div>
      <div class="card-item"><strong>${device.discovery ? 'Pair' : 'Read Battery'}:</strong>
        <label class="toggle-switch">
          <input type="checkbox" name="${device.address}_${device.discovery ? 'pair' : 'batt'}_mobile" ${device.discovery ? (device.paired ? 'checked' : '') : (device.readBattery ? 'checked' : '')} ${device.discovery && device.paired ? 'disabled' : ''}>
          <span class="toggle-slider"></span>
        </label>
      </div>
      <div class="card-item card-actions"><button type="button" class="btn btn-danger"><i class="fas fa-trash-alt"></i> Delete Device</button></div>
    </div>
  `;
  if (device.discovery) card.querySelector('.card-actions').hidden = true;

  const descInput = card.querySelector(`input[name="${device.address}_desc_mobile"]`);
  descInput.oninput = () => document.querySelector(`input[name="${device.address}_desc"]`).value = descInput.value;

  const batteryCheckbox = card.querySelector(`input[name="${device.address}_${device.discovery ? 'pair' : 'batt'}_mobile"]`);
  batteryCheckbox.onchange = () => {
    const tableCheckbox = document.querySelector(`input[name="${device.address}_${device.discovery ? 'pair' : 'batt'}"]`);
    tableCheckbox.checked = batteryCheckbox.checked;
    tableCheckbox.onchange?.();
  };

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
        const isDiscovery = $(this).attr('data-discovery') === 'true';
        const toggle=(this).find(`input[name="${mac}_${isDiscovery ? 'pair' : 'batt'}"]`);
        const isChecked = $toggle.prop('checked');
        const isDisabled = $toggle.prop('disabled');

        createDeviceCard({
          address: mac,
          description: $(this).find(`input[name="${mac}_desc"]`).val() || '', 
          readBattery: isDiscovery ? false : isChecked, 
          discovery: isDiscovery,
          paired: isDiscovery ? isChecked : isDisabled
        });

        createDeviceCard({
          address: mac,
          description: $(this).find(`input[name="${mac}_desc"]`).val() || '', // Use template literal
          readBattery: $(this).find(`input[name="${mac}_${isDiscovery ? 'pair' : 'batt'}"]`).prop('checked'), // Use .prop()
          discovery: isDiscovery,
          paired: $(this).find(`input[name="${mac}_${isDiscovery ? 'pair' : 'batt'}"]`).prop('disabled')
        });
      }
    });
  }
};
