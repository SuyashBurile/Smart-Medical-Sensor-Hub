// ========== LOGIN ==========
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

// ========== UPDATE TEXT ==========
function setText(id, val) {
  document.getElementById(id).innerText = val ?? "--";
}

// ========== GRAPHS ==========
let labels = [];
let hrArr = [];
let gsrArr = [];
let spiroArr = [];

// HR Chart
const hrChart = new Chart(document.getElementById("hrChart"), {
  type: "line",
  data: { labels, datasets: [{ label: "HR", data: hrArr, borderColor: "red" }] },
  options: { animation: false }
});

// GSR Chart
const gsrChart = new Chart(document.getElementById("gsrChart"), {
  type: "line",
  data: { labels, datasets: [{ label: "GSR", data: gsrArr, borderColor: "blue" }] },
  options: { animation: false }
});

// Spiro Chart
const spiroChart = new Chart(document.getElementById("spiroChart"), {
  type: "line",
  data: { labels, datasets: [{ label: "Spiro", data: spiroArr, borderColor: "green" }] },
  options: { animation: false }
});

// ========== ECG DRAW ==========
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

// ========== AUTO FETCH SENSOR DATA ==========
setInterval(async () => {
  let res = await fetch("/latest");
  let d = await res.json();

  setText("hr", d.heartRate);
  setText("spo2", d.spo2);
  setText("temp", d.temperature);
  setText("gsr", d.gsr);
  setText("spiro", d.spiro);
  setText("bp", `${d.bp_sys} / ${d.bp_dia}`);
  setText("pulse", d.bp);
  setText("glucose", d.glucose);

  labels.push(new Date().toLocaleTimeString());
  hrArr.push(d.heartRate || 0);
  gsrArr.push(d.gsr || 0);
  spiroArr.push(d.spiro || 0);

  if (labels.length > 50) {
    labels.shift();
    hrArr.shift();
    gsrArr.shift();
    spiroArr.shift();
  }

  hrChart.update();
  gsrChart.update();
  spiroChart.update();

  if (d.ecg && d.ecg !== "--") drawECG(d.ecg);

}, 300);

// ========== SAVE PATIENT RECORD ==========
async function saveRecord() {
  let body = {
    name: document.getElementById("pname").value,
    age: document.getElementById("page").value,
    gender: document.getElementById("pgender").value
  };

  let res = await fetch("/save-record", {
    method: "POST",
    headers: { "Content-Type": "application/json" },
    body: JSON.stringify(body)
  });

  document.getElementById("save-status").innerText = await res.text();
}
