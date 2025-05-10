let currentLogs = []; 
let filteredLogs = []; 
let currentPage = 1;
let logsPerPage = 50;
let autoRefreshInterval = null;

const populatePage = (data) => {
  if (data?.length) { // Optional chaining and length check
    currentLogs = data;
    updateLogStats();
    filterLogs();
  } else {
    currentLogs = [];
    filteredLogs = [];
    $('#logEntries').empty();
    $('#logsLoading').hide();
    $('#noLogs').show();
    updateLogStats();
  }
};

$(document).ready(function() {

  loadLogs();

    // Modal functionality
    $('#clearBtn').click(() => {
      $('#clearLogsModal').css('display', 'block');
    });
  
    $('.close-modal').click(() => {
      $('#clearLogsModal').css('display', 'none');
    });
  
    $('#cancelClearBtn').click(() => {
      $('#clearLogsModal').css('display', 'none');
    });
  
    $('#setLogLevel').click(function() {
      const level = $('#logLevel').val();
      sendData(`/logs?loglevel=${level}`, null,
          () => showPopup('Log level updated successfully', '#2ecc71'),
          () => showPopup('Failed to update log level')
      );
      filterLogs();
    });

    const $autoRefresh = $('#autoRefresh');
    const $refreshStatus = $('#refreshStatus');
    let autoRefreshInterval;
  
    const updateRefreshStatus = () => {
      $refreshStatus.text($autoRefresh.prop('checked') ? 'On' : 'Off');
    };
  
    const toggleAutoRefresh = () => {
      clearInterval(autoRefreshInterval);
      autoRefreshInterval = $autoRefresh.prop('checked') ? setInterval(loadLogs, 30000) : null;
      updateRefreshStatus();
    };
  
    $autoRefresh.change(toggleAutoRefresh);
    toggleAutoRefresh(); // Initialize on page load
  
  $('#refreshBtn').click(() => {
    loadLogs();
  });
  
  $('#logLevel').change(() =>{
    filterLogs();
  });
  
  $('#prevPage').click(() =>{
    if (currentPage > 1) {
      currentPage--;
      displayLogs();
    }
  });
  
  $('#nextPage').click(() =>{
    if (currentPage < Math.ceil(filteredLogs.length / logsPerPage)) {
      currentPage++;
      displayLogs();
    }
  });
  
  $('#confirmClearBtn').click(() =>{
    clearLogs();
    $('#clearLogsModal').hide();
  });
});

const loadLogs = () => {
  $('#logsLoading').show();
  $('#logEntries').hide();
  $('#noLogs').hide();

  getData('/getlogsdata', null,
    populatePage,
    (xhr, status, error) => {
      console.error('Errore nel caricamento dei log:', status, error);
      $('#logsLoading').hide();
      $('#logEntries').html(`<div class="error-message"><i class="fas fa-exclamation-triangle"></i> Error loading logs: ${error}</div>`).show();
    }
  );
};

const filterLogs = () => {
  const logLevel = parseInt($('#logLevel').val());

  filteredLogs = currentLogs.filter(log => log?.l !== undefined && log.l <= logLevel);

  currentPage = 1;
  displayLogs();
  updateLogStats();
};

const getLogLevelIndicator = (level) =>
  ['E', 'W', 'I', 'D', 'V'][level] ?? '?';

const displayLogs = () => {
  const startIndex = (currentPage - 1) * logsPerPage;
  const endIndex = Math.min(startIndex + logsPerPage, filteredLogs.length);
  const pageEntries = filteredLogs.slice(startIndex, endIndex);

  const $logEntries = $('#logEntries');
  $logEntries.empty();

  if (!pageEntries.length) {
    $('#logsLoading').hide();
    $('#noLogs').show();
    return;
  }

  const logElements = pageEntries.map(log => {
    const $logEntry = $('<div class="log-entry"></div>');
    const $timestamp = $('<span class="log-timestamp"></span>').text(timeConverter(log.t));
    const $level = $('<span class="log-level"></span>').text(getLogLevelIndicator(log.l));
    const $message = $('<span class="log-message"></span>').text(log.m);

    if (log.m.startsWith('Error')) {
      $message.addClass('log-level-error');
    } else if (log.m.startsWith('BLETracker')) {
      $message.addClass('log-level-tracker');
    }

    $logEntry.append($timestamp, ' ', $level, ' ', $message);
    return $logEntry;
  });

  $logEntries.append(logElements);

  $('#logsLoading').hide();
  $('#noLogs').hide();
  $logEntries.show();

  updatePaginationControls();
};


const updatePaginationControls = () => {
  const totalPages = Math.max(1, Math.ceil(filteredLogs.length / logsPerPage));
  $('#pageInfo').text(`Page ${currentPage} of ${totalPages}`);
  $('#prevPage').prop('disabled', currentPage <= 1);
  $('#nextPage').prop('disabled', currentPage >= totalPages);
};

const updateLogStats = () => {
  $('#totalLogs').text(currentLogs.length);
  $('#filteredLogs').text(filteredLogs.length);

  const logSize = currentLogs.reduce((total, log) => total + (log.t?.length || 0) + (log.m?.length || 0), 0);
  $('#logSize').text(`${Math.round(logSize / 102.4) / 10} KB`); // Corrected KB calculation and rounding
};

const clearLogs = () => {
  $('#logsLoading').show();
  $('#logEntries').hide();

  getData('/eraselogs', null,
    () => {
      loadLogs();
      $('#message').removeClass('alert-error').addClass('alert-success').text('Logs cleared successfully').show().delay(3000).fadeOut();
    },
    (xhr, status, error) => {
      $('#message').removeClass('alert-success').addClass('alert-error').text(`Error clearing logs: ${error}`).show();
    }
  );
};