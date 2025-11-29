// server.js — Node backend (saves to Excel + CSV)
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
app.use(express.static(__dirname)); // serve static files from project root

// Keep latest sensor readings per device in memory
let latestSensorData = {};

// patient counter persistence
let patientCounter = 0;
if (fs.existsSync(counterFile)) {
  try {
    const saved = JSON.parse(fs.readFileSync(counterFile, 'utf8'));
    patientCounter = saved.counter || 0;
  } catch (e) {
    patientCounter = 0;
  }
}

// Simple login credentials (you can change)
const USERNAME = 'doctor';
const PASSWORD = '1234';

app.post('/login', (req, res) => {
  const { username, password } = req.body;
  if (username === USERNAME && password === PASSWORD) {
    res.status(200).send('OK');
  } else {
    res.status(401).send('Unauthorized');
  }
});

// Receive sensor data from ESP32 (or from curl / Postman)
app.post('/sensor-data', (req, res) => {
  const data = req.body;
  if (!data.device_id) return res.status(400).json({ error: 'device_id required' });

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

  console.log('Received sensor data:', latestSensorData[data.device_id]);
  res.json({ message: 'Sensor data received' });
});

// Provide latest reading for a device
app.get('/latest/:device_id', (req, res) => {
  const id = req.params.device_id;
  res.json(latestSensorData[id] || {});
});

// Save patient info + sensor snapshot (Excel + CSV)
app.post('/submit', async (req, res) => {
  const { name, age, gender, device_id } = req.body;

  if (!device_id) return res.status(400).json({ error: 'device_id required' });

  const sensor = latestSensorData[device_id] || {};

  // increment and persist patient counter
  patientCounter += 1;
  try {
    fs.writeFileSync(counterFile, JSON.stringify({ counter: patientCounter }));
  } catch (e) {
    console.error('Failed to write counter file', e);
  }

  // build row object
  const row = {
    PatientNumber: patientCounter,
    Name: name || '',
    Age: age || '',
    Gender: gender || '',
    DeviceID: device_id,
    Timestamp: sensor.timestamp || new Date().toISOString(),
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
    Spiro: sensor.spiro || ''
  };

  // -------- Write to Excel --------
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
    console.error('Error writing Excel:', err);
    // continue to attempt CSV write
  }

  // -------- Write to CSV (append) --------
  try {
    // build CSV header if file not exists
    const keys = Object.keys(row);
    if (!fs.existsSync(csvFile)) {
      const header = keys.join(',') + '\n';
      fs.writeFileSync(csvFile, header, { encoding: 'utf8' });
    }
    // escape double quotes and commas inside fields
    const escapeCsv = (v) => {
      if (v === null || v === undefined) return '';
      const s = String(v);
      if (s.includes('"') || s.includes(',') || s.includes('\n')) {
        return `"${s.replace(/"/g, '""')}"`;
      }
      return s;
    };
    const csvLine = keys.map(k => escapeCsv(row[k])).join(',') + '\n';
    fs.appendFileSync(csvFile, csvLine, { encoding: 'utf8' });
  } catch (err) {
    console.error('Error writing CSV:', err);
  }

  res.json({ message: 'Patient saved', patientNumber: patientCounter });
});

// Serve root
app.get('/', (req, res) => {
  res.sendFile(path.join(__dirname, 'index.html'));
});

// start server
app.listen(PORT, () => {
  console.log(`Server running on port ${PORT}`);
});
