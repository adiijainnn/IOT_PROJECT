import Chart from 'chart.js/auto';

// Initial System Telemetry State
const state = {
  soilMoisture: 28,
  temperature: 27.4,
  humidity: 61.2,
  dhtValid: true,
  pumpState: true, // true = ON, false = OFF
  mode: 'AUTO', // AUTO or MANUAL
  manualTimer: 0,
  dryThreshold: 35,
  wetThreshold: 45,
  statusReason: 'SOIL DRY (<=35%)',
  wifiConnected: true,
  mqttConnected: true,
  ipAddress: '10.13.37.2'
};

// DOM Element Selectors
const elSoilGauge = document.getElementById('soil-gauge');
const elSoilVal = document.getElementById('soil-val');
const elSoilAdc = document.getElementById('soil-adc-lbl');
const elSoilBar = document.getElementById('soil-bar-fill');
const elSoilStatusTag = document.getElementById('soil-status-tag');

const elTempVal = document.getElementById('temp-val');
const elTempStatus = document.getElementById('temp-status');
const elTempBar = document.getElementById('temp-bar');

const elHumVal = document.getElementById('hum-val');
const elHumBar = document.getElementById('hum-bar');

const elModePill = document.getElementById('mode-pill');
const elPumpOrb = document.getElementById('pump-orb');
const elPumpTxt = document.getElementById('pump-state-txt');
const elRelayGpio = document.getElementById('relay-gpio-txt');
const elReasonTxt = document.getElementById('reason-txt');

const elSoilSlider = document.getElementById('soil-slider');
const elSoilSliderVal = document.getElementById('soil-slider-val');
const elBtnManual = document.getElementById('btn-manual-trigger');
const elBtnDht = document.getElementById('btn-toggle-dht');
const elTerminalLogs = document.getElementById('terminal-logs');
const elClearLogsBtn = document.getElementById('clear-logs-btn');
const elClock = document.getElementById('clock-display');

// Chart Setup
const ctx = document.getElementById('telemetryChart').getContext('2d');
const chartData = {
  labels: [],
  datasets: [
    {
      label: 'Soil Moisture (%)',
      data: [],
      borderColor: '#10b981',
      backgroundColor: 'rgba(16, 185, 129, 0.1)',
      borderWidth: 2,
      fill: true,
      tension: 0.4
    },
    {
      label: 'Temperature (°C)',
      data: [],
      borderColor: '#f59e0b',
      borderWidth: 2,
      fill: false,
      tension: 0.4
    },
    {
      label: 'Pump State (1=ON, 0=OFF)',
      data: [],
      borderColor: '#06b6d4',
      borderDash: [5, 5],
      borderWidth: 1.5,
      stepped: true,
      fill: false
    }
  ]
};

const chart = new Chart(ctx, {
  type: 'line',
  data: chartData,
  options: {
    responsive: true,
    maintainAspectRatio: false,
    plugins: {
      legend: {
        labels: { color: '#94a3b8', font: { family: 'Outfit', size: 12 } }
      }
    },
    scales: {
      x: {
        grid: { color: 'rgba(255, 255, 255, 0.05)' },
        ticks: { color: '#64748b', font: { family: 'JetBrains Mono', size: 10 } }
      },
      y: {
        grid: { color: 'rgba(255, 255, 255, 0.05)' },
        ticks: { color: '#64748b', font: { family: 'JetBrains Mono', size: 10 } },
        min: 0,
        max: 100
      }
    }
  }
});

// Helper Functions
function addLogLine(msg, type = 'info') {
  const timeStr = new Date().toLocaleTimeString();
  const div = document.createElement('div');
  div.className = `log-line ${type}`;
  div.textContent = `[${timeStr}] ${msg}`;
  elTerminalLogs.appendChild(div);
  elTerminalLogs.scrollTop = elTerminalLogs.scrollHeight;
}

function updateClock() {
  const now = new Date();
  elClock.textContent = now.toLocaleTimeString();
}
setInterval(updateClock, 1000);
updateClock();

// Core Hysteresis Logic Evaluation
function evaluateSystemLogic() {
  if (state.mode === 'MANUAL') {
    if (state.manualTimer > 0) {
      state.manualTimer--;
      state.pumpState = true;
      state.statusReason = `MANUAL WATERING (${state.manualTimer}s)`;
    } else {
      state.mode = 'AUTO';
      addLogLine('Manual watering complete. Restoring AUTO control mode.', 'info');
    }
  }

  if (state.mode === 'AUTO') {
    if (state.soilMoisture <= state.dryThreshold) {
      if (!state.pumpState) {
        state.pumpState = true;
        addLogLine(`Soil moisture dropped to ${state.soilMoisture}% (<= 35%). PUMP TURNED ON.`, 'warn');
      }
      state.statusReason = `SOIL DRY (<=35%)`;
    } else if (state.soilMoisture >= state.wetThreshold) {
      if (state.pumpState) {
        state.pumpState = false;
        addLogLine(`Soil moisture reached ${state.soilMoisture}% (>= 45%). PUMP TURNED OFF.`, 'info');
      }
      state.statusReason = `SOIL OPTIMAL (>=45%)`;
    } else {
      // Hysteresis Band (35% to 45%)
      if (state.pumpState) {
        state.statusReason = `HYSTERESIS BAND (PUMP ON)`;
      } else {
        state.statusReason = `HYSTERESIS BAND (PUMP OFF)`;
      }
    }
  }

  renderUI();
}

// Render Telemetry onto UI
function renderUI() {
  // Soil Gauge & Cards
  elSoilGauge.style.setProperty('--pct', state.soilMoisture);
  elSoilVal.innerHTML = `${state.soilMoisture.toFixed(1)}<small>%</small>`;
  const adcRaw = Math.round((state.soilMoisture / 100) * 4095);
  elSoilAdc.textContent = `ADC: ${adcRaw}`;
  elSoilBar.style.width = `${state.soilMoisture}%`;

  if (state.soilMoisture <= 35) {
    elSoilStatusTag.textContent = 'SOIL DRY';
    elSoilStatusTag.style.background = 'rgba(244, 63, 94, 0.2)';
    elSoilStatusTag.style.color = '#f43f5e';
  } else if (state.soilMoisture >= 45) {
    elSoilStatusTag.textContent = 'SOIL WET';
    elSoilStatusTag.style.background = 'rgba(16, 185, 129, 0.2)';
    elSoilStatusTag.style.color = '#10b981';
  } else {
    elSoilStatusTag.textContent = 'HYSTERESIS BAND';
    elSoilStatusTag.style.background = 'rgba(245, 158, 11, 0.2)';
    elSoilStatusTag.style.color = '#f59e0b';
  }

  // Temperature & Humidity
  if (state.dhtValid) {
    elTempVal.innerHTML = `${state.temperature.toFixed(1)}<span class="unit">°C</span>`;
    elTempStatus.textContent = 'Sensor Active (DHT22)';
    elTempBar.style.width = `${(state.temperature / 50) * 100}%`;

    elHumVal.innerHTML = `${state.humidity.toFixed(1)}<span class="unit">%</span>`;
    elHumBar.style.width = `${state.humidity}%`;
  } else {
    elTempVal.innerHTML = `ERR<span class="unit">°C</span>`;
    elTempStatus.textContent = 'DHT Read Error!';
    elHumVal.innerHTML = `ERR<span class="unit">%</span>`;
  }

  // Mode Pill
  elModePill.textContent = state.mode === 'AUTO' ? 'AUTO MODE' : 'MANUAL MODE';
  elModePill.style.background = state.mode === 'AUTO' ? 'rgba(139, 92, 246, 0.2)' : 'rgba(6, 182, 212, 0.2)';
  elModePill.style.color = state.mode === 'AUTO' ? '#8b5cf6' : '#06b6d4';

  // Pump Visual Orb
  if (state.pumpState) {
    elPumpOrb.style.background = 'radial-gradient(circle, #10b981 0%, rgba(16, 185, 129, 0.2) 70%)';
    elPumpOrb.style.boxShadow = '0 0 25px rgba(16, 185, 129, 0.4)';
    elPumpTxt.textContent = 'PUMP ON';
    elRelayGpio.textContent = 'HIGH (1)';
    elRelayGpio.style.color = '#10b981';
  } else {
    elPumpOrb.style.background = 'radial-gradient(circle, #334155 0%, rgba(51, 65, 85, 0.2) 70%)';
    elPumpOrb.style.boxShadow = 'none';
    elPumpTxt.textContent = 'PUMP OFF';
    elRelayGpio.textContent = 'LOW (0)';
    elRelayGpio.style.color = '#94a3b8';
  }

  elReasonTxt.textContent = state.statusReason;
}

// Interactive Controls Listeners
elSoilSlider.addEventListener('input', (e) => {
  state.soilMoisture = parseFloat(e.target.value);
  elSoilSliderVal.textContent = `${state.soilMoisture}%`;
  evaluateSystemLogic();
});

elBtnManual.addEventListener('click', () => {
  state.mode = 'MANUAL';
  state.manualTimer = 5;
  state.pumpState = true;
  addLogLine('Manual irrigation triggered via Dashboard control (5s duration)', 'warn');
  evaluateSystemLogic();
});

elBtnDht.addEventListener('click', () => {
  state.dhtValid = !state.dhtValid;
  if (!state.dhtValid) {
    addLogLine('Simulated DHT22 read failure! System continues soil monitoring safely.', 'error');
    elBtnDht.textContent = 'Restore DHT Sensor';
  } else {
    addLogLine('DHT22 sensor communication restored.', 'info');
    elBtnDht.textContent = 'Simulate DHT Failure';
  }
  evaluateSystemLogic();
});

elClearLogsBtn.addEventListener('click', () => {
  elTerminalLogs.innerHTML = '';
});

// Periodic Telemetry Telemetry Stream & Chart Updates
setInterval(() => {
  evaluateSystemLogic();

  // Update Live Chart
  const timeLabel = new Date().toLocaleTimeString();
  if (chartData.labels.length > 15) {
    chartData.labels.shift();
    chartData.datasets[0].data.shift();
    chartData.datasets[1].data.shift();
    chartData.datasets[2].data.shift();
  }

  chartData.labels.push(timeLabel);
  chartData.datasets[0].data.push(state.soilMoisture);
  chartData.datasets[1].data.push(state.dhtValid ? state.temperature : null);
  chartData.datasets[2].data.push(state.pumpState ? 100 : 0);

  chart.update('none');
}, 1000);

// Initial Load
evaluateSystemLogic();
addLogLine('Local Dashboard connected to Smart Irrigation control engine', 'info');
