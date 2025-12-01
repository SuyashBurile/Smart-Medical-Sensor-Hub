// server.js — Node backend (saves to Excel + CSV + serves frontend)

const express = require('express');
const bodyParser = require('body-parser');
const ExcelJS = require('exceljs');
const fs = require('fs');
const path = require('path');

const app = express();
const PORT = process.env.PORT || 3000;

const excelFile = 'patient_data.xlsx';
const csvFile = 'patient_data.csv';
const counterFile = 'patient_counter.json';

app.use(bodyParser.json());
app.use(express.static(__dirname));

// latest sensor readings per device
let latestSensorData = {};

// patient counter persistence
let patientCounter = 0;
if (fs.existsSync(counterFile)) {
  try {
    patientCounter = JSON.parse(
      fs.readFileSync(counterFile, 'utf8')
    ).counter || 0;
  } catch {
    patientCounter = 0;
  }
}

// LOGIN CREDENTIALS
const USERNAME = 'Doctor';
const PASSWORD = 'SB';

// LOGIN ROUTE
app.post('/login', (req, res) => {
  const { username, password } = req.body;
  if (username === USERNAME && password === PASSWORD) {
    return res.status(200).send('OK');
  }
  res.status(401).send('Unauthorized');
});

// RECEIVE LIVE SENSOR DATA FROM ESP32
app.post('/sensor-data', (req, res) => {
  const data = req.body;
  if (!data.device_id) {
    return res.status(400).json({ error: 'device_id required' });
  }

  latestSensorData[data.device_id] = {
    device_id: data.device_id,
    timestamp: data.timestamp || new Date().toISOString(),
    seq: data.seq || '',
    heartRate: data.heartRate || data.hr || '',
    spo2: data.spo2 || '',
    temperature: data.temperature || '',
    ecg: data.ecg || '',
    glucose: data.glucose || data.sugar || '',
    bp_sys: data.bp_sys || '',
    bp_dia: data.bp_dia || '',
    bp: data.bp || '',
    gsr: data.gsr || '',
    spiro: data.spiro || ''
  };

  console.log('Sensor Data:', latestSensorData[data.device_id]);
  res.json({ message: 'Sensor data received' });
});

// GET LATEST SENSOR DATA
app.get('/latest/:device_id', (req, res) => {
  res.json(latestSensorData[req.params.device_id] || {});
});

// SAVE PATIENT RECORD
app.post('/submit', async (req, res) => {
  const { name, age, gender, device_id } = req.body;

  if (!device_id)
    return res.status(400).json({ error: 'device_id required' });

  const sensor = latestSensorData[device_id] || {};

  patientCounter++;
  fs.writeFileSync(counterFile, JSON.stringify({ counter: patientCounter }));

  const row = {
    PatientNumber: patientCounter,
    Name: name || '',
    Age: age || '',
    Gender: gender || '',
    DeviceID: device_id,
    Timestamp: sensor.timestamp || '',
    Seq: sensor.seq || '',
    HeartRate: sensor.heartRate || '',
    SpO2: sensor.spo2 || '',
    Temperature: sensor.temperature || '',
    ECG: sensor.ecg || '',
    Sugar_Glucose: sensor.glucose || '',
    BP_SYS: sensor.bp_sys || '',
    BP_DIA: sensor.bp_dia || '',
    BP: sensor.bp || '',
    GSR: sensor.gsr || '',
    LungCapacity: sensor.spiro || ''
  };

  // SAVE TO EXCEL
  try {
    const workbook = new ExcelJS.Workbook();
    let worksheet;

    if (fs.existsSync(excelFile)) {
      await workbook.xlsx.readFile(excelFile);
      worksheet = workbook.getWorksheet(1);
    } else {
      worksheet = workbook.addWorksheet('Patients');
      worksheet.columns = Object.keys(row).map(k => ({ header: k, key: k }));
    }

    worksheet.addRow(row);
    await workbook.xlsx.writeFile(excelFile);
  } catch (err) {
    console.error('Excel Save Error:', err);
  }

  // SAVE TO CSV
  try {
    const keys = Object.keys(row);

    if (!fs.existsSync(csvFile)) {
      fs.writeFileSync(csvFile, keys.join(',') + '\n');
    }

    const escape = v => `"${String(v).replace(/"/g, '""')}"`;

    const line = keys.map(k => escape(row[k])).join(',') + '\n';
    fs.appendFileSync(csvFile, line);
  } catch (err) {
    console.error('CSV Save Error:', err);
  }

  res.json({ message: 'Patient saved', patientNumber: patientCounter });
});

// ROOT: SERVE LOGIN PAGE
app.get('/', (req, res) => {
  res.sendFile(path.join(__dirname, 'index.html'));
});

app.listen(PORT, () =>
  console.log(`Server running on port ${PORT}`)
);
