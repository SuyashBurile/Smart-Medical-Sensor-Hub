// ====================== LOGIN ======================
function doLogin() {
  let u = document.getElementById("username").value;
  let p = document.getElementById("password").value;

  // Default admin login
  if (u === "admin" && p === "admin") {
    document.getElementById("login-box").classList.add("hidden");
    document.getElementById("main-box").classList.remove("hidden");
  } else {
    document.getElementById("login-error").innerText = "Invalid username or password!";
  }
}

// ====================== SENSOR TEXT UPDATE ======================
function setText(id, value) {
  document.getElementById(id).innerText = value ?? "--";
}

// ====================== REAL-TIME GRAPHS ======================
let timeLabels = [];
let hrData = [];
let gsrData = [];
let spiroData = [];

// --- HR Chart ---
const hrChart = new Chart(document.getElementById("hrChart"), {
  type: "line",
  data: {
    labels: timeLabels,
    datasets: [{ label: "HR (BPM)", data: hrData, borderWidth: 2, borderColor: "red" }]
  },
  options: { animation: false }
});

// --- GSR Chart ---
const gsrChart = new Chart(document.getElementById("gsrChart"), {
  type: "line",
  data: {
    labels: timeLabels,
    datasets: [{ label: "GSR (%)", data: gsrData, borderWidth: 2, borderColor: "blue" }]
  },
  options: { animation: false }
});

// --- SPIRO Chart ---
const spiroChart = new Chart(document.getElementById("spiroChart"), {
  type: "line",
  data: {
    labels: timeLabels,
    datasets: [{ label: "Spiro Value", data: spiroData, borderWidth: 2, borderColor: "green" }]
  },
  options: { animation: false }
});

// ====================== ECG CANVAS DRAW ======================
const ecgCanvas = document.getElementById("ecgCanvas");
const ecgCtx = ecgCanvas.getContext("2d");
let ecgX = 0;

function drawECG(val) {
  let y = 150 - (val * 0.03);
  if (y < 0) y = 0;
  if (y > 150) y = 150;

  ecgX++;
  if (ecgX >= ecgCanvas.width) {
    ecgX = 0;
    ecgCtx.clearRect(0, 0, ecgCanvas.width, ecgCanvas.height);
  }

  ecgCtx.fillStyle = "lime";
  ecgCtx.fillRect(ecgX, y, 2, 2);
}

// ====================== AUTO UPDATE SENSORS ======================
setInterval(async () => {
  let res = await fetch("/latest");
  let data = await res.json();

  // Update text fields
  setText("hr", data.heartRate);
  setText("spo2", data.spo2);
  setText("temp", data.temperature);
  setText("gsr", data.gsr);
  setText("spiro", data.spiro);
  setText("bp", `${data.bp_sys} / ${data.bp_dia}`);
  setText("pulse", data.bp);
  setText("glucose", data.glucose);

  // Graph time label
  let t = new Date().toLocaleTimeString();
  timeLabels.push(t);

  // Add data points
  hrData.push(data.heartRate || 0);
  gsrData.push(data.gsr || 0);
  spiroData.push(data.spiro || 0);

  // Remove old data after 50 points
  if (timeLabels.length > 50) {
    timeLabels.shift();
    hrData.shift();
    gsrData.shift();
    spiroData.shift();
  }

  hrChart.update();
  gsrChart.update();
  spiroChart.update();

  // ECG value draw
  if (data.ecg !== undefined && data.ecg !== "--") {
    drawECG(data.ecg);
  }

}, 300);

// ====================== SAVE PATIENT RECORD ======================
async function saveRecord() {
  let payload = {
    name: document.getElementById("pname").value,
    age: document.getElementById("page").value,
    gender: document.getElementById("pgender").value
  };

  let res = await fetch("/save-record", {
    method: "POST",
    headers: { "Content-Type": "application/json" },
    body: JSON.stringify(payload)
  });

  let txt = await res.text();
  document.getElementById("save-status").innerText = txt;
}
