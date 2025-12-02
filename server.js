const express = require("express");
const fs = require("fs");
const path = require("path");
const app = express();

app.use(express.json());
app.use(express.static(__dirname));  // <--- Serves CSS, JS, HTML automatically

let latest = {
  heartRate: "--",
  spo2: "--",
  temperature: "--",
  gsr: "--",
  spiro: "--",
  bp_sys: "--",
  bp_dia: "--",
  bp: "--",
  glucose: "--",
  ecg: "--"
};

// ---------- Serve Homepage ----------
app.get("/", (req, res) => {
  res.sendFile(__dirname + "/form.html");
});

// ---------- Receive ESP32 Sensor Data ----------
app.post("/sensor-data", (req, res) => {
  Object.assign(latest, req.body);
  res.sendStatus(200);
});

// ---------- Send Latest Values to Dashboard ----------
app.get("/latest", (req, res) => {
  res.json(latest);
});

// ---------- Save Patient Record ----------
app.post("/save-record", (req, res) => {
  const file = path.join(__dirname, "records.csv");
  const { name, age, gender } = req.body;

  const row = [
    new Date().toISOString(),
    name,
    age,
    gender,
    latest.heartRate,
    latest.spo2,
    latest.temperature,
    latest.gsr,
    latest.spiro,
    latest.bp_sys,
    latest.bp_dia,
    latest.bp,
    latest.glucose,
    latest.ecg
  ].join(",") + "\n";

  if (!fs.existsSync(file)) {
    fs.writeFileSync(
      file,
      "timestamp,name,age,gender,heartRate,spo2,temperature,gsr,spiro,bp_sys,bp_dia,bp,glucose,ecg\n"
    );
  }

  fs.appendFileSync(file, row);
  res.send("Patient record saved successfully!");
});

// ---------- Start Server ----------
app.listen(3000, () =>
  console.log("Server running on http://localhost:3000")
);
