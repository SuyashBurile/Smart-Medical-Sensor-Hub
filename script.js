// script.js — live metrics, ECG plotting, patient save logic

const DEFAULT_DEVICE = 'esp32-001';

let lastSavedPatientNumber = null;
let ecgBufferSize = 600;
let ecgBuffer = new Array(ecgBufferSize).fill(null);
let ecgHead = 0;
let ecgZoom = 1.0;

const METRIC_INTERVAL = 2000;  // 2s for vitals
const ECG_INTERVAL = 300;      // extra sampling for ECG

// canvas init
const canvas = document.getElementById('ecgCanvas');
let ctx = null;
if (canvas) {
  const DPR = window.devicePixelRatio || 1;
  canvas.width = canvas.clientWidth * DPR;
  canvas.height = canvas.clientHeight * DPR;
  ctx = canvas.getContext('2d');
  ctx.scale(DPR, DPR);
}

// helpers
function getDeviceId() {
  const el = document.getElementById('deviceId');
  return (el && el.value.trim()) || DEFAULT_DEVICE;
}

function pushECGSample(value) {
  const v = Number(value);
  if (Number.isNaN(v)) return;
  ecgBuffer[ecgHead] = v;
  ecgHead = (ecgHead + 1) % ecgBufferSize;
}

function drawECG() {
  if (!ctx || !canvas) return;

  const w = canvas.clientWidth;
  const h = canvas.clientHeight;

  ctx.clearRect(0, 0, w, h);

  // background
  ctx.fillStyle = '#010814';
  ctx.fillRect(0, 0, w, h);

  // midline
  ctx.strokeStyle = 'rgba(255,255,255,0.06)';
  ctx.lineWidth = 1;
  ctx.beginPath();
  ctx.moveTo(0, h / 2);
  ctx.lineTo(w, h / 2);
  ctx.stroke();

  // ECG line
  const data = ecgBuffer.filter(v => v !== null);
  if (!data.length) return;

  const visibleCount = Math.floor(data.length / ecgZoom);
  const slice = data.slice(-visibleCount);
  const step = Math.max(1, Math.floor(slice.length / w));

  ctx.strokeStyle = '#22c55e';
  ctx.lineWidth = 1.6;
  ctx.beginPath();

  let x = 0;
  for (let i = 0; i < slice.length; i += step) {
    const v = slice[i];
    const norm = Math.max(0, Math.min(1, v / 4095)); // 0..1
    const y = h - norm * h;
    if (x === 0) ctx.moveTo(x, y);
    else ctx.lineTo(x, y);
    x += 1;
  }
  ctx.stroke();
}

// update connection status
function setConnectionStatus(online) {
  const dot = document.getElementById('connDot');
  const text = document.getElementById('connectedText');
  if (!dot || !text) return;

  if (online) {
    dot.style.color = '#22c55e';
    text.textContent = 'Online';
  } else {
    dot.style.color = '#f97373';
    text.textContent = 'Offline';
  }
}

// fetch metrics (HR, SpO2, temp, BP, sugar, GSR, lung, ECG)
async function fetchMetrics() {
  const device = getDeviceId();
  try {
    const res = await fetch(`/latest/${device}`);
    if (!res.ok) {
      setConnectionStatus(false);
      return;
    }
    const data = await res.json();
    const hasData = Object.keys(data).length > 0;
    setConnectionStatus(hasData);

    // map to UI
    const hrEl = document.getElementById('hr');
    const spo2El = document.getElementById('spo2');
    const tempEl = document.getElementById('temp');
    const bpEl = document.getElementById('bp');
    const sugarEl = document.getElementById('sugar');
    const gsrEl = document.getElementById('gsr');
    const lungEl = document.getElementById('lung');

    const hr = data.heartRate || data.hr;
    if (hrEl) hrEl.innerHTML = (hr ? hr : '--') + ' <span class="muted">bpm</span>';

    const spo2 = data.spo2;
    if (spo2El) spo2El.innerHTML = (spo2 ? spo2 : '--') + ' <span class="muted">%</span>';

    const temp = data.temperature;
    if (tempEl) tempEl.innerHTML = (temp ? temp : '--') + ' <span class="muted">°C</span>';

    if (bpEl) {
      if (data.bp) bpEl.textContent = data.bp;
      else if (data.bp_sys && data.bp_dia) bpEl.textContent = `${data.bp_sys}/${data.bp_dia}`;
      else bpEl.textContent = '-- / --';
    }

    const sugar = data.glucose || data.sugar;
    if (sugarEl) sugarEl.innerHTML = (sugar ? sugar : '--') + ' <span class="muted">mg/dL</span>';

    const gsr = data.gsr;
    if (gsrEl) gsrEl.textContent = gsr ? `${gsr}` : '--';

    const spiro = data.spiro;
    if (lungEl) lungEl.innerHTML = (spiro ? spiro : '--') + ' <span class="muted">units</span>';

    if (data.ecg !== undefined) {
      pushECGSample(data.ecg);
    }
  } catch {
    setConnectionStatus(false);
  }
}

// extra ECG-specific polling (more frequent)
async function fetchECGOnly() {
  const device = getDeviceId();
  try {
    const res = await fetch(`/latest/${device}`);
    if (!res.ok) return;
    const data = await res.json();
    if (data.ecg !== undefined) {
      pushECGSample(data.ecg);
    }
  } catch {
    // ignore
  }
}

// patient save + UI hooks
document.addEventListener('DOMContentLoaded', () => {
  const form = document.getElementById('patientForm');
  const patientIdEl = document.getElementById('patientId');
  const recentSavedEl = document.getElementById('recentSaved');

  if (form) {
    form.addEventListener('submit', async (ev) => {
      ev.preventDefault();
      const payload = {
        name: document.getElementById('name').value,
        age: document.getElementById('age').value,
        gender: document.getElementById('gender').value,
        device_id: getDeviceId()
      };

      try {
        const res = await fetch('/submit', {
          method: 'POST',
          headers: { 'Content-Type': 'application/json' },
          body: JSON.stringify(payload)
        });
        if (res.ok) {
          const json = await res.json();
          lastSavedPatientNumber = json.patientNumber;
          if (patientIdEl) patientIdEl.textContent = lastSavedPatientNumber;
          if (recentSavedEl) recentSavedEl.textContent = lastSavedPatientNumber;
          alert('Saved — Patient #' + lastSavedPatientNumber);
          form.reset();
        } else {
          const txt = await res.text();
          alert('Save failed: ' + txt);
        }
      } catch {
        alert('Network error while saving');
      }
    });
  }

  const zoomIn = document.getElementById('zoomIn');
  const zoomOut = document.getElementById('zoomOut');
  if (zoomIn) zoomIn.onclick = () => { ecgZoom = Math.min(3, ecgZoom + 0.2); };
  if (zoomOut) zoomOut.onclick = () => { ecgZoom = Math.max(0.5, ecgZoom - 0.2); };

  fetchMetrics();
});

// global for inline onclick
function nextPatient() {
  const form = document.getElementById('patientForm');
  if (form) form.reset();
  const patientIdEl = document.getElementById('patientId');
  if (patientIdEl) {
    if (lastSavedPatientNumber) patientIdEl.textContent = Number(lastSavedPatientNumber) + 1;
    else patientIdEl.textContent = '—';
  }
}
window.nextPatient = nextPatient;

// periodic tasks
setInterval(fetchMetrics, METRIC_INTERVAL);
setInterval(fetchECGOnly, ECG_INTERVAL);
setInterval(() => { if (canvas) drawECG(); }, 200);
