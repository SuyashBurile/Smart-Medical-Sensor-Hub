// script.js — front-end logic for Smart Medical Sensor Hub

// Helper to set value or default
function setText(id, value, fallback = "--") {
  const el = document.getElementById(id);
  if (!el) return;
  if (value === undefined || value === null || value === "") {
    el.textContent = fallback;
  } else {
    el.textContent = value;
  }
}

// Simple ECG drawing buffer
const ecgBuffer = [];
let ecgZoom = 1;

function pushEcgSample(val) {
  if (typeof val !== "number") return;
  ecgBuffer.push(val);
  if (ecgBuffer.length > 500) ecgBuffer.shift();
  drawEcg();
}

function drawEcg() {
  const canvas = document.getElementById("ecgCanvas");
  if (!canvas) return;
  const ctx = canvas.getContext("2d");

  const w = canvas.width;
  const h = canvas.height;
  ctx.clearRect(0, 0, w, h);

  // baseline
  ctx.strokeStyle = "rgba(255,255,255,0.12)";
  ctx.lineWidth = 1;
  ctx.beginPath();
  ctx.moveTo(0, h / 2);
  ctx.lineTo(w, h / 2);
  ctx.stroke();

  if (ecgBuffer.length < 2) return;

  const maxVal = 4095;
  const scale = (h * 0.4) * ecgZoom;

  ctx.strokeStyle = "#40E0D0";
  ctx.lineWidth = 2;
  ctx.beginPath();

  for (let i = 0; i < ecgBuffer.length; i++) {
    const x = (i / (ecgBuffer.length - 1)) * w;
    const centered = (ecgBuffer[i] / maxVal - 0.5) * 2;
    const y = h / 2 - centered * scale;

    if (i === 0) ctx.moveTo(x, y);
    else ctx.lineTo(x, y);
  }
  ctx.stroke();
}

document.addEventListener("DOMContentLoaded", () => {
  const fullName   = document.getElementById("fullName");
  const age        = document.getElementById("age");
  const gender     = document.getElementById("gender");
  const deviceId   = document.getElementById("deviceId");

  const saveBtn        = document.getElementById("saveBtn");
  const clearBtn       = document.getElementById("clearBtn");
  const fetchBtn       = document.getElementById("fetchLatestBtn");
  const fetchStatus    = document.getElementById("fetchStatus");
  const patientNumber  = document.getElementById("patientNumber");
  const lastSaved      = document.getElementById("lastSaved");
  const deviceStatus   = document.getElementById("deviceStatus");
  const nextPatientBtn = document.getElementById("nextPatientBtn");

  const zoomInBtn  = document.getElementById("zoomInBtn");
  const zoomOutBtn = document.getElementById("zoomOutBtn");

  // default device id (MUST match device_id in ESP32 code)
  if (!deviceId.value) deviceId.value = "esp32-sensor-1";


  // ---- Fetch latest sensor snapshot from backend (which ESP32 posted) ----
  async function loadFromEsp32() {
    const id = deviceId.value.trim();
    if (!id) {
      fetchStatus.textContent = "Enter Device ID first.";
      return;
    }
    fetchStatus.textContent = "Contacting server…";

    try {
      const res = await fetch(`/latest/${encodeURIComponent(id)}`);
      if (!res.ok) {
        fetchStatus.textContent = "No data (server error).";
        deviceStatus.textContent = "Offline";
        deviceStatus.classList.add("off");
        return;
      }
      const data = await res.json();
      if (!data || Object.keys(data).length === 0) {
        fetchStatus.textContent = "No recent data from this device.";
        deviceStatus.textContent = "Offline";
        deviceStatus.classList.add("off");
        return;
      }

      // Update vitals
      setText("hrValue", data.heartRate);
      setText("spo2Value", data.spo2);
      setText("tempValue", data.temperature);
      const bpText =
        data.bp_sys && data.bp_dia
          ? `${data.bp_sys} / ${data.bp_dia}`
          : data.bp || "-- / --";
      setText("bpValue", bpText, "-- / --");
      setText("glucoseValue", data.glucose);
      setText("gsrValue", data.gsr);
      setText("spiroValue", data.spiro);

      // ECG single sample (for now just one)
      if (data.ecg !== undefined && data.ecg !== null && data.ecg !== "") {
        const v = Number(data.ecg);
        if (!Number.isNaN(v)) pushEcgSample(v);
      }

      deviceStatus.textContent = "Online";
      deviceStatus.classList.remove("off");
      fetchStatus.textContent = "Live data loaded.";
    } catch (err) {
      console.error(err);
      fetchStatus.textContent = "Network error while fetching.";
      deviceStatus.textContent = "Offline";
      deviceStatus.classList.add("off");
    }
  }

  // ---- Save patient + current readings ----
  async function savePatient() {
    const nameVal = fullName.value.trim();
    const ageVal  = age.value.trim();
    const genderVal = gender.value;
    const devVal  = deviceId.value.trim();

    if (!nameVal || !ageVal || !devVal) {
      alert("Please fill Name, Age and Device ID.");
      return;
    }

    try {
      const res = await fetch("/submit", {
        method: "POST",
        headers: { "Content-Type": "application/json" },
        body: JSON.stringify({
          name: nameVal,
          age: ageVal,
          gender: genderVal,
          device_id: devVal
        })
      });

      if (!res.ok) {
        alert("Server error while saving.");
        return;
      }

      const data = await res.json();
      patientNumber.textContent = data.patientNumber ?? "—";
      const ts = new Date().toLocaleString();
      lastSaved.textContent = ts;

      alert("Patient saved successfully.");
    } catch (err) {
      console.error(err);
      alert("Network error while saving.");
    }
  }

  function clearForm() {
    fullName.value = "";
    age.value = "";
    gender.value = "Male";
    // keep device ID same
  }

  // ---- Event listeners ----
  if (fetchBtn) fetchBtn.addEventListener("click", loadFromEsp32);
  if (saveBtn) saveBtn.addEventListener("click", savePatient);
  if (clearBtn) clearBtn.addEventListener("click", clearForm);
  if (nextPatientBtn) nextPatientBtn.addEventListener("click", () => {
    clearForm();
    patientNumber.textContent = "—";
    lastSaved.textContent = "—";
  });

  if (zoomInBtn) {
    zoomInBtn.addEventListener("click", () => {
      ecgZoom = Math.min(ecgZoom + 0.2, 3);
      drawEcg();
    });
  }

  if (zoomOutBtn) {
    zoomOutBtn.addEventListener("click", () => {
      ecgZoom = Math.max(ecgZoom - 0.2, 0.4);
      drawEcg();
    });
  }

  // Optional: auto-load once when page opens
  loadFromEsp32();
});
