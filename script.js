// script.js — FINAL WORKING VERSION

function setText(id, value, fallback="--") {
  const el = document.getElementById(id);
  if (!el) return;
  el.textContent = (value === "" || value === undefined || value === null)
    ? fallback
    : value;
}

// ECG
const ecgBuffer = [];
let ecgZoom = 1;

function pushEcg(val) {
  if (typeof val !== "number") return;
  ecgBuffer.push(val);
  if (ecgBuffer.length > 600) ecgBuffer.shift();
  drawEcg();
}

function drawEcg() {
  const canvas = document.getElementById("ecgCanvas");
  if (!canvas) return;
  const ctx = canvas.getContext("2d");

  const w = canvas.width, h = canvas.height;
  ctx.clearRect(0, 0, w, h);

  ctx.strokeStyle = "rgba(255,255,255,0.2)";
  ctx.beginPath(); ctx.moveTo(0, h/2); ctx.lineTo(w, h/2); ctx.stroke();

  if (ecgBuffer.length < 2) return;

  ctx.strokeStyle = "#00f0ff";
  ctx.lineWidth = 2;
  ctx.beginPath();

  const scale = h * 0.4 * ecgZoom;

  ecgBuffer.forEach((v, i) => {
    const x = (i / (ecgBuffer.length - 1)) * w;
    const y = h/2 - ((v/4095 - 0.5)*2)*scale;
    if (i === 0) ctx.moveTo(x, y);
    else ctx.lineTo(x, y);
  });

  ctx.stroke();
}

// MAIN
document.addEventListener("DOMContentLoaded", () => {

  const fetchDeviceBtn = document.getElementById("fetchLatestBtn");
  const fetchAllBtn = document.getElementById("fetchAllBtn");

  async function loadAll() {
    try {
      const res = await fetch("/latest-all");
      const { sender={}, receiver={} } = await res.json();

      // Sender
      setText("hrValue", sender.heartRate);
      setText("spo2Value", sender.spo2);
      setText("tempValue", sender.temperature);

      const bp = sender.bp_sys && sender.bp_dia
        ? `${sender.bp_sys} / ${sender.bp_dia}`
        : sender.bp || "-- / --";
      setText("bpValue", bp);

      setText("gsrValue", sender.gsr);
      setText("spiroValue", sender.spiro);

      if (sender.ecg !== "" && sender.ecg !== undefined) {
        const v = Number(sender.ecg);
        if (!isNaN(v)) pushEcg(v);
      }

      // Receiver
      setText("glucoseValue", receiver.glucose);

      fetchStatus.textContent = "All devices loaded.";
      deviceStatus.textContent = "Online";
      deviceStatus.classList.remove("off");

    } catch (e) {
      fetchStatus.textContent = "Network error.";
    }
  }

  fetchAllBtn.addEventListener("click", loadAll);
  fetchDeviceBtn.addEventListener("click", async () => {
    const id = deviceId.value.trim();
    const res = await fetch(`/latest/${id}`);
    const data = await res.json();

    setText("hrValue", data.heartRate);
    setText("spo2Value", data.spo2);
    setText("tempValue", data.temperature);
    setText("gsrValue", data.gsr);
    setText("spiroValue", data.spiro);
    setText("glucoseValue", data.glucose);

    const bp = data.bp_sys && data.bp_dia
     ? `${data.bp_sys} / ${data.bp_dia}`
     : data.bp || "-- / --";
    setText("bpValue", bp);

    if (data.ecg) pushEcg(Number(data.ecg));

    fetchStatus.textContent = "Loaded.";
  });

  // Save Patient
  saveBtn.addEventListener("click", async () => {
    const res = await fetch("/submit", {
      method: "POST",
      headers: {"Content-Type": "application/json"},
      body: JSON.stringify({
        name: fullName.value,
        age: age.value,
        gender: gender.value,
        device_id: deviceId.value
      })
    });

    const data = await res.json();
    alert("Saved!");
    patientNumber.textContent = data.patientNumber;
    lastSaved.textContent = new Date().toLocaleString();
  });

  clearBtn.addEventListener("click", () => {
    fullName.value = "";
    age.value = "";
    gender.value = "Male";
  });

  loadAll();
});
