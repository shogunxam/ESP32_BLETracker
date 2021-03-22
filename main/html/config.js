$(document).ready(function () {
  now = new Date();
  factory = getUrlParameter('factory');
  var url = "/getconfigdata?time=" + now.getTime();
  if (factory == "true") {
    url += "&factory=true";
  }
  data = { "version": 3, "mqtt_enabled": true, "mqtt_address": "192.168.1.7", "mqtt_port": 1883, "mqtt_usr": "xxxxxx", "mqtt_pwd": "xxxxxx", "scanPeriod": 10, "maxNotAdvPeriod": 120, "loglevel": 2, "whiteList": true, "trk_list": { "CA67347FD139": { "battery": true, "desc": "12345678901234567890" }, "D1667690EAA2": { "battery": false, "desc": "123" }, "EB3B442D6CCE": { "battery": false, "desc": "123" } } }
  //$.get(url, function (data) {
    console.log(data);
    if (!data.mqtt_enabled)
      $('#mqttbroker').hide()
    $('#mqttsrvr').val(data.mqtt_address);
    $('#mqttport').val(data.mqtt_port);
    $('#mqttusr').val(data.mqtt_usr);
    $('#mqttpwd').val(data.mqtt_pwd);
    $('#scanperiod').val(data.scanPeriod);
    $('#maxNotAdvPeriod').val(data.maxNotAdvPeriod);
    $('#whiteList').prop("checked", data.whiteList);
    for (const property in data.trk_list) {
      addMac(`${property}`, data.trk_list[property]);
      $('#addMac').prop("disabled", true);
    }
  //});
});

function getUrlParameter(sParam) {
  var sPageURL = window.location.search.substring(1),
    sURLVariables = sPageURL.split('&'),
    sParameterName,
    i;

  for (i = 0; i < sURLVariables.length; i++) {
    sParameterName = sURLVariables[i].split('=');

    if (sParameterName[0] === sParam) {
      return sParameterName[1] === undefined ? true : decodeURIComponent(sParameterName[1]);
    }
  }
};

$(function () {
  $('#configure').click(function (e) {

    const form = document.querySelector('#configform');
    const data = new FormData(form);
    var checkbox = $("#configform").find("input[type=checkbox]");
    $.each(checkbox, function (key, val) {
      data.append($(val).attr('name'), $(val).is(':checked'))
    });

    var d = Array.from(data).reduce((acc, e) => {
      if(e[0] == "newmac")
        return acc;
      if( e[0].includes(":"))
      {
        [g, k, v] = [...e[0].split(':'), e[1]];
        if (!acc[g])
          acc[g] = {};
        acc[g][k]=v;
      }
      else 
      {
        k = e[0]; v= e[1];
        acc[k] = v;
      }
      return acc;
    }, {}
    );

    $.ajax({
      type: "POST",
      url: "/updateconfig",
      data: d,
      success: function () {
        console.log('success!');
        window.location = '/restart';
      },
      error: function () {
        console.log('error!');
        alert("WARNING: An error occurred saving the settings!");
      },
    });
    e.preventDefault();
  });
  $('#factory').click(function (e) {
    window.location.href = '/config?factory=true';
    e.preventDefault();
  });
  $('#undo').click(function (e) {
    window.location.href = '/config';
    e.preventDefault();
  });
  $('#addMac').click(function () {
    mac = $('#newmac').val();
    addMac(mac, false);
    $('#addMac').prop("disabled", true);
    $('#newmac').val("");
    $('#devListContainer').scrollTop($('#devListContainer')[0].scrollHeight);
  });
});

function addMac(mac, devProp) {
  container = $('#devList > tbody:last-child');
  raw = '<tr id="rw_' + mac + '">';
  raw += '<td><input type="checkbox" id="'+ mac + '_batt" name="' + mac + ':batt" style="width:auto;height:auto"></td>';
  raw += '<td>' + mac + '<input type="text" id="' + mac + '_desc" name="' + mac + ':desc" style="font-size:0.95em;text-align:center;width:15em;height:auto" value="' + devProp.desc +'"><br><hr></td>';
  raw += '<td><input type="button" id="rm_' + mac + '" value="Remove"style="width:auto;height:auto" class=dangerbtn onclick="$(\'#rw_' + mac + '\').remove()"></td>';
  raw += '</tr>';
  container.append(raw);
  $('#' + mac +'_batt').prop("checked", devProp.battery);
}

function validateMacDigit(elem) {
  if (event.key.length > 1) {
    return elem.value;
  }
  mac = elem.value + event.key.toUpperCase();
  event.preventDefault();
  if (/^[0-9A-F]+$/.test(mac)) {
    if (mac.length == 12) {
      $('#addMac').prop("disabled", false);
    }
    return mac;
  }
  alert("Invalid mac address format, only uppercase hexadecimal digits are allowed!");
  $('#addmac').disabled = true;
  return elem.value;
}

function validateMac(mac) {
  console.log(mac);
  if (/^[0-9A-F]+$/.test(mac) && mac.length == 12) {
    $('#addMac').prop("disabled", false);
    return mac;
  }
  alert("Invalid mac address format, only uppercase hexadecimal digits are allowed!");
  $('#addMac').prop("disabled", true);
  return "";
}

function isMacValid(mac) {
  console.log((/^[0-9A-F]+$/.test(mac)));
  return (/^[0-9A-F]+$/.test(mac));
}

