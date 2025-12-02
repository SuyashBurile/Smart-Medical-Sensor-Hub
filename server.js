const express = require("express");
const fs = require("fs");
const path = require("path");
const cors = require("cors");

const app = express();
app.use(cors());
app.use(express.json());

// Serve static files
app.use(express.static(path.join(__dirname)));

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

// Home route
app.get("/", (req, res) => {
  res.sendFile(path.join(__dirname, "form.html"));
});

// ESP32 → Server
app.post("/sensor-data", (req, res) => {
  Object.assign(latest, req.body);
  res.sendStatus(200);
});

// Dashboard → fetch latest
app.get("/latest", (req, res) => {
  res.json(latest);
});

// Save CSV record
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

// Render Cloud Port
const PORT = process.env.PORT || 3000;
app.listen(PORT, () => console.log("Server running on port " + PORT));
