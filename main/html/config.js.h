R"=====(
  $(document).ready(function() {
    now = new Date();
    factory = getUrlParameter('factory');
    var url = "/getconfigdata?time="+now.getTime();
    if(factory == "true")
      url+="&factory=true";

    $.get(url,function(data) {
      console.log(data);
      $('#mqttsrvr').val(data.mqtt_address);
      $('#mqttport').val(data.mqtt_port);
      $('#mqttusr').val(data.mqtt_usr);
      $('#mqttpwd').val(data.mqtt_pwd);
      $('#scanperiod').val(data.scanPeriod);
      $('#whiteList').prop("checked", data.whiteList);
      for (const property in data.trk_list) {
        addMac(`${property}`, data.trk_list[property]);
        $('#addMac').prop("disabled", true);
      }
    });
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
  
  $(function() {
    $('#configure').click(function(e) {
      var d = $('#form :input[name!="newmac"]').serialize({
        checkboxesAsBools: true
      });
      console.log(d);
      $.ajax({
        type: "POST",
        url: "/updateconfig",
        data: d,
        success: function() {
          console.log('success!');
          var ftimeout = setTimeout(null, 5000);
          window.location = '/reset';
        },
        error: function() {
          console.log('error!');
          alert("WARNING: An error occurred saving the settings!");
        },
      });
      e.preventDefault();
    });
    $('#factory').click(function(e) {
      window.location.href = '/config?factory=true';
      e.preventDefault();
    });
    $('#undo').click(function(e) {
      window.location.href = '/config';
      e.preventDefault();
    });
  });
  
  function addMac(mac, whitelist) {
    container = $('#devList > tbody:last-child');
    raw = '<tr id="rw_'+ mac + '">';
    raw += '<td><input type="checkbox" id="' + mac + '"name="' + mac + '"style="width:auto;height:auto"></td>';
    raw += '<td>' + mac + '</td>';
    raw += '<td><input type="button" id="rm_' + mac + '" value="Remove"style="width:auto;height:auto" class=removebtn onclick="$(\'#rw_'+mac+'\').remove()"></td>';
    raw += '</tr>';
    container.append(raw);
    $('#' + mac).prop("checked", whitelist);
  }
  
  $('#addMac').click(function() {
    mac = $('#newmac').val();
    addMac(mac, false);
    $('#addMac').prop("disabled", true);
  });
  
  function removeElement(elem)
  {
      elem
  }
  function validateMacDigit(elem) {
    if (event.key.length > 1)
      return elem.value;
    mac = elem.value + event.key.toUpperCase();
    event.preventDefault();
    if (/^[0-9A-F]+$/.test(mac)) {
      if (mac.length == 12)
        $('#addMac').prop("disabled", false);
      return mac;
    }
    alert("Invalid mac address format, only uppercase exadecimal digits are allowed!");
    $('#addmac').disabled = true;
    return elem.value;
  }
  
  function validateMac(mac) {
    console.log(mac);
    if (/^[0-9A-F]+$/.test(mac) && mac.length == 12) {
      $('#addMac').prop("disabled", false);
      return mac;
    }
    alert("Invalid mac address format, only uppercase exadecimal digits are allowed!");
    $('#addMac').prop("disabled", true);
    return "";
  }
  
  function isMacValid(mac) {
    console.log((/^[0-9A-F]+$/.test(mac)));
    return (/^[0-9A-F]+$/.test(mac));
  }
  
  (function($) {
    $.fn.serialize = function(options) {
      return $.param(this.serializeArray(options));
    };
  
    $.fn.serializeArray = function(options) {
      var o = $.extend({
        checkboxesAsBools: false
      }, options || {});
  
      var rselectTextarea = /select|textarea/i;
      var rinput = /text|hidden|password|search|number/i;
  
      return this.map(function() {
          return this.elements ? $.makeArray(this.elements) : this;
        })
        .filter(function() {
          return this.name && !this.disabled &&
            (this.checked ||
              (o.checkboxesAsBools && this.type === 'checkbox') ||
              rselectTextarea.test(this.nodeName) ||
              rinput.test(this.type));
        })
        .map(function(i, elem) {
          var val = $(this).val();
          return val == null ?
            null :
            $.isArray(val) ?
            $.map(val, function(val, i) {
              return {
                name: elem.name,
                value: val
              };
            }) : {
              name: elem.name,
              value: (o.checkboxesAsBools && this.type === 'checkbox') ?
                (this.checked ? true : false) : val
            };
        }).get();
    };
  })(jQuery);
  )====="
