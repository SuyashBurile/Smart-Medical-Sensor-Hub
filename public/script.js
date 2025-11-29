// Fetch latest sensor data and fill the form
fetch('/sensor-data')
  .then(res => res.json())
  .then(data => {
    document.getElementById('heartRate').value = data.heartRate || '';
    document.getElementById('temperature').value = data.temperature || '';
    document.getElementById('ecg').value = data.ecg || '';
    document.getElementById('sugar').value = data.sugar || '';
    document.getElementById('bp').value = data.bp || '';
  });

// Handle form submit
document.getElementById('patientForm').addEventListener('submit', function (e) {
  e.preventDefault();

  const formData = {
    name: document.getElementById('name').value,
    age: document.getElementById('age').value,
    gender: document.getElementById('gender').value
  };

  fetch('/submit', {
    method: 'POST',
    headers: { 'Content-Type': 'application/json' },
    body: JSON.stringify(formData)
  }).then(res => {
    if (res.ok) {
      alert('Patient data saved to Excel!');
      document.getElementById('patientForm').reset();
    } else {
      alert('Error saving data.');
    }
  });
});
