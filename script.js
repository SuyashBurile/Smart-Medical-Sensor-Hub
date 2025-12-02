// ================== LOGIN ==================
function doLogin() {
  let u = document.getElementById("username").value;
  let p = document.getElementById("password").value;

  if (u === "admin" && p === "admin") {
    document.getElementById("login-box").classList.add("hidden");
    document.getElementById("main-box").classList.remove("hidden");
  } else {
    document.getElementById("login-error").innerText = "Invalid credentials!";
  }
}

// ================== TEXT UPDATE HELPER ==================
function setText(id, value) {
  document.getElementById(id).innerText = value ?? "--";
}

// ================== CHART INITIALIZATION ==================
let hrData = [];
let gsrData = [];
let spiroData = [];
let timeLabels = [];

const hrChart = new Chart(document.getElementById("hrChart"), {
  type: "line",
  data: {
    labels: timeLabels,
    datasets: [{ label: "HR BPM", data: hrData, borderWidth: 2 }]
  },
  options: { animation: false, scales: { y: { suggestedMin: 40, suggestedMax: 150 } } }
});

const gsrChart = new Chart(document.getElementById("gsrChart"), {
  type: "line",
  data: {
    labels: timeLabels,
    datasets: [{ label: "GSR %", data: gsrData, borderWidth: 2 }]
  },
  options: { animation: false, scales: { y: { suggestedMin: 0, suggestedMax: 100 } } }
});

const spiroChart = new Chart(document.getElementById("spiroChart"), {
  type: "line",
  data: {
    labels: timeLabels,
    datasets: [{ label: "Spiro Value", data: spiroData, borderWidth: 2 }]
  },
  options: { animation: false, scales: { y: { suggestedMin: 4000, suggestedMax: 7000 } } }
});

// ================== ECG CANVAS ==================
const ecgCanvas = document.getElementById("ecgCanvas");
const ecgCtx = ecgCanvas.getContext("2d");
let ecgX = 0;

function drawECG(value) {
  let y = 150 - value * 0.03; // scale ECG amplitude
  if (y < 0) y = 0;
  if (y > 150) y = 150;

  // Move X
  ecgX++;
  if (ecgX >= ecgCanvas.width) {
    ecgX = 0;
    ecgCtx.clearRect(0, 0, ecgCanvas.width, ecgCanvas.height);
  }

  // Draw pixel
  ecgCtx.fillStyle = "lime";
  ecgCtx.fillRect(ecgX, y, 2, 2);
}

// ================== AUTO SENSOR UPDATES ==================
setInterval(async () => {
  let res = await fetch("/latest");
  let data = await res.json();

  // Update text fields
  setText("hr", data.heartRate);
  setText("spo2", data.spo2);
  setText("temp", data.temperature);
  setText("gsr", data.gsr);
  setText("spiro", data.spiro);
  setText("bp", `${data.bp_sys || "--"} / ${data.bp_dia || "--"}`);
  setText("pulse", data.bp || "--");
  setText("glucose", data.glucose);

  // Add data to graph arrays
  const t = new Date().toLocaleTimeString();
  timeLabels.push(t);

  hrData.push(data.heartRate || 0);
  gsrData.push(data.gsr || 0);
  spiroData.push(data.spiro || 0);

  if (timeLabels.length > 50) {
    timeLabels.shift();
    hrData.shift();
    gsrData.shift();
    spiroData.shift();
  }

  hrChart.update();
  gsrChart.update();
  spiroChart.update();

  // ECG (only when value exists)
  if (data.ecg !== undefined) {
    drawECG(data.ecg);
  }

}, 300);

// ================== SAVE PATIENT RECORD ==================
async function saveRecord() {
  let payload = {
    name: document.getElementById("pname").value,
    age: document.getElementById("page").value,
    gender: document.getElementById("pgender").value
  };

  let res = await fetch("/save-record", {
    method: "POST",
    headers: {"Content-Type": "application/json"},
    body: JSON.stringify(payload)
  });

  let txt = await res.text();
  document.getElementById("save-status").innerText = txt;
}
