/* script.js — enhanced for live ECG plotting + 7-parameter dashboard
   Replace the old script.js with this file (complete replacement).
*/

// Device id (default). You can change this in the form input on the left.
const DEFAULT_DEVICE = "esp32-001";
let deviceIdInput = document.getElementById ? document.getElementById('deviceId') : null;

// current patient id returned after save
let lastSavedPatientNumber = null;

// fetch intervals
const METRIC_INTERVAL = 2000;  // metrics (HR, SPO2, temp...) every 2s
const ECG_INTERVAL = 300;      // request ECG samples ~ every 300ms

// ECG plotting buffer
const ECG_BUFFER_SIZE = 600;
let ecgBuffer = new Array(ECG_BUFFER_SIZE).fill(null);
let ecgHead = 0;

// canvas setup
const canvas = document.getElementById('ecgCanvas');
const ctx = canvas ? canvas.getContext('2d') : null;
const DPR = window.devicePixelRatio || 1;
if (canvas) {
  canvas.width = canvas.clientWidth * DPR;
  canvas.height = canvas.clientHeight * DPR;
  ctx.scale(DPR, DPR);
}

// helper: get device id from the input (if present)
function getDeviceId(){
  const el = document.getElementById('deviceId');
  return (el && el.value) ? el.value.trim() : DEFAULT_DEVICE;
}

// draw ECG buffer on canvas
function drawECG(){
  if (!ctx || !canvas) return;
  const w = canvas.clientWidth;
  const h = canvas.clientHeight;
  ctx.clearRect(0,0,w,h);

  // background grid
  ctx.fillStyle = "#02060a";
  ctx.fillRect(0,0,w,h);

  // draw midline
  ctx.strokeStyle = "rgba(255,255,255,0.06)";
  ctx.lineWidth = 1;
  ctx.beginPath();
  ctx.moveTo(0,h/2); ctx.lineTo(w,h/2); ctx.stroke();

  // draw signal
  ctx.strokeStyle = "#00ff7b";
  ctx.lineWidth = 1.6;
  ctx.beginPath();

  // map buffer to visible width: take last N defined samples
  const visible = ecgBuffer.filter(v => v !== null);
  if (visible.length === 0) return;

  const step = Math.max(1, Math.floor(visible.length / w));
  let x = 0;
  for (let i = 0; i < visible.length; i += step) {
    const v = visible[visible.length - 1 - i]; // reverse to show newest at right
    const norm = (v - 0) / 4095; // assume 0..4095 ADC
    const y = h - (norm * h);
    if (i === 0) ctx.moveTo(w - x, y);
    else ctx.lineTo(w - x, y);
    x += 1;
  }
  ctx.stroke();
}

// push one new ECG sample
function pushECGSample(value){
  ecgBuffer[ecgHead] = Number(value) || 0;
  ecgHead = (ecgHead + 1) % ECG_BUFFER_SIZE;
}

// fetch latest metrics (HR, SPO2, temp, bp, sugar, gsr)
async function fetchMetrics(){
  const device = getDeviceId();
  try {
    const res = await fetch(`/latest/${device}`);
    if (!res.ok) return;
    const data = await res.json();

    // map fields to UI
    document.getElementById('hr').innerText = (data.heartRate || data.hr) ? `${data.heartRate || data.hr} bpm` : '-- bpm';
    document.getElementById('spo2').innerText = data.spo2 ? `${data.spo2} %` : (data.spo2 || '-- %');
    document.getElementById('temp').innerText = data.temperature ? `${data.temperature} °C` : '-- °C';
    if (data.bp) document.getElementById('bp').innerText = data.bp;
    else if (data.bp_sys && data.bp_dia) document.getElementById('bp').innerText = `${data.bp_sys}/${data.bp_dia}`;
    document.getElementById('sugar').innerText = (data.glucose || data.sugar) ? `${data.glucose || data.sugar} mg/dL` : '-- mg/dL';
    document.getElementById('gsr').innerText = data.gsr ? `${data.gsr}` : '--';
    // push ECG sample if provided
    if (data.ecg) pushECGSample(data.ecg);
  } catch (err) {
    // console.log('metrics fetch failed', err);
  }
}

// periodic tasks
setInterval(fetchMetrics, METRIC_INTERVAL);
setInterval(() => { drawECG(); }, 200); // redraw ECG often

// also poll faster for incoming ECG samples (server updates latest.ecg value very frequently)
setInterval(async () => {
  const device = getDeviceId();
  try {
    const res = await fetch(`/latest/${device}`);
    if (!res.ok) return;
    const data = await res.json();
    if (data.ecg) pushECGSample(data.ecg);
  } catch (e){}
}, ECG_INTERVAL);

// handle Save patient form
document.addEventListener('DOMContentLoaded', () => {
  const form = document.getElementById('patientForm');
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
        const res = await fetch('/submit',{
          method:'POST',
          headers:{'Content-Type':'application/json'},
          body: JSON.stringify(payload)
        });
        if (res.ok) {
          const json = await res.json();
          lastSavedPatientNumber = json.patientNumber;
          document.getElementById('patientId').innerText = lastSavedPatientNumber;
          alert('Saved — Patient #'+ lastSavedPatientNumber);
          form.reset();
        } else {
          const txt = await res.text();
          alert('Save failed: ' + txt);
        }
      } catch (err) {
        alert('Network error while saving');
      }
    });
  }
});

// Next patient: clear and show next patient id (if lastSaved known)
function nextPatient(){
  const form = document.getElementById('patientForm');
  if (form) form.reset();
  if (lastSavedPatientNumber) document.getElementById('patientId').innerText = Number(lastSavedPatientNumber) + 1;
  else document.getElementById('patientId').innerText = '—';
}
window.nextPatient = nextPatient;

// initial load
fetchMetrics();
