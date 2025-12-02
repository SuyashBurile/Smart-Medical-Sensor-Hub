const express = require("express");
const fs = require("fs");
const path = require("path");
const app = express();

app.use(express.json());
app.use(express.static(__dirname));

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

// ========== ESP32 POSTS SENSOR DATA HERE ==========
app.post("/sensor-data", (req, res) => {
  let body = req.body;

  // Merge incoming ESP32 values
  Object.assign(latest, body);

  res.sendStatus(200);
});


// ========== WEBPAGE FETCHES LATEST SENSOR DATA ==========
app.get("/latest", (req, res) => {
  res.json(latest);
});

// ========== SAVE CSV ON FORM SUBMIT ==========
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
    latest.glucose
  ].join(",") + "\n";

  // Create file if not exists
  if (!fs.existsSync(file)) {
    fs.writeFileSync(
      file,
      "timestamp,name,age,gender,heartRate,spo2,temperature,gsr,spiro,bp_sys,bp_dia,bp,glucose\n"
    );
  }

  fs.appendFileSync(file, row);

  res.send("Patient record saved successfully!");
});

// SERVER
app.listen(3000, () => console.log("Server running on http://localhost:3000"));
