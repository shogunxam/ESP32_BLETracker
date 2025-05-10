// Set the ESP32 IP address to debug http://<user>:<pwd>@<ip>
// Let empty URL for production
const esp32URL = "";

const dateTimeOptions = {
  year: 'numeric',
  month: '2-digit',
  day: '2-digit',
  hour: '2-digit',
  minute: '2-digit',
  second: '2-digit',
  };

const timeConverter = (timestampUtc) => {
  const data = new Date(timestampUtc * 1000);
  return data.toLocaleString(undefined, dateTimeOptions).replace(/\//g, '-');
};

const createAuthHeaders = (baseURL) => {
  const headers = {};
  let urlBase = baseURL || "";

  if (baseURL) {
    const match = baseURL.match(/^(https?:\/\/)?([^:]+):([^@]+)@(.*)$/);
    if (match && match[2] && match[3]) {
      headers.Authorization = 'Basic ' + btoa(match[2] + ':' + match[3]);
      urlBase = (match[1] || "") + match[4];
    }
  }

  return { baseURL: urlBase, headers };
};

const showPopup = (message,bckg = "#e74c3c", tm = 3000) => {
  const msgPopupId = 'msg-popup';
  const existingPopup = document.getElementById(msgPopupId);
  if (existingPopup) {
      existingPopup.remove();
  }
  const msgHTML = `<div id="${msgPopupId}"style="position: fixed; top: 50%; left: 50%; transform: translate(-50%, -50%); background-color: ${bckg}; color: white; padding: 15px; border-radius: 5px; z-index: 1001; display: flex; flexDirection: column; alignItems: center;">${message}</div>`;
  document.body.insertAdjacentHTML('beforeend', msgHTML);
  setTimeout( () => {
      const popupToRemove = document.getElementById(msgPopupId);
      if (popupToRemove) {
          popupToRemove.remove();
      }
  }
  , tm);
};

const getData = (endpoint, data, successCallback, errorCallback) => {
  let loadingTimeout;
  const spinnerId = 'loading-spinner-delayed';
  const spinnerHTML = `
    <div id="${spinnerId}" style="position: fixed; top: 50%; left: 50%; transform: translate(-50%, -50%); z-index: 1000; background-color: rgba(0, 0, 0, 0.5); color: white; padding: 20px; border-radius: 8px; display: flex; flexDirection: column; alignItems: center;">
      <div class="spinner"></div>
      <p>Loading data...</p>
    </div>
  `;

  const showSpinner = () => {
    document.body.insertAdjacentHTML('beforeend', spinnerHTML);
  };

  loadingTimeout = setTimeout(showSpinner, 1000);

  const { baseURL, headers } = createAuthHeaders(esp32URL);
  const jqXHR = $.ajax({
    url: baseURL + endpoint,
    method: 'GET',
    data,
    headers: {
      ...headers,
      'Cache-Control': 'no-cache, no-store, must-revalidate',
      'Pragma': 'no-cache',
      'Expires': '0'
    },
    success: successCallback,
    error: (xhr, status, error) => {
      clearTimeout(loadingTimeout);
      const existingSpinner = document.getElementById(spinnerId);
      if (existingSpinner) {
        existingSpinner.remove();
      }
      showPopup(`Error loading data`);
      errorCallback(xhr, status, error);
    },
    complete: () => {
      clearTimeout(loadingTimeout);
      const existingSpinner = document.getElementById(spinnerId);
      if (existingSpinner) {
        existingSpinner.remove();
      }
    }
  });

  return jqXHR;
};

const sendData = (endpoint, body, successCallback, errorCallback) => {
  const { baseURL, headers } = createAuthHeaders(esp32URL);
  $.ajax({
    type: 'POST',
    url: baseURL + endpoint,
    headers,
    data: body,
    contentType: 'application/x-www-form-urlencoded',
    success: successCallback,
    error: errorCallback,
  });
};
