PVision is an IoT project that monitors solar panel data in real-time via Firebase, utilizing an ESP32 microcontroller and an INA219 sensor. Through the web interface, instantaneous measurements and historical data can be viewed.

ESP32 + INA219
│
│  (Wi-Fi / Firebase Realtime Database)
▼
Firebase RTDB
│
├── panel_1/ → Real-time measurements (5 sec)
├── panel_1/system → System status (active / sleep_mode)
├── History/ → 2-minute average logs
└── WeeklySummaries/ → Automatically generated weekly summaries
│
▼
Web Interface (HTML + Chart.js)
├── login.html → Authentication via Firebase Auth
├── index.html → Real-time monitoring + charts
└── history.html → Historical data + Excel export

Project Features
Real-time Monitoring: Voltage, current, and power values are updated every 5 seconds.

Automated Charting: Instantaneous trend graphs for the last 30 measurements.

Sleep Mode Support: If the voltage drops below 4V, the ESP32 enters deep sleep for 30 minutes, and a warning is displayed on the web interface.

Historical Data Logging: Average power and energy production data are saved to Firebase every 2 minutes.

Automated Data Cleanup: When the limit of 5,040 records (≈1 week) is exceeded, old logs are deleted, and a weekly summary is automatically generated.

Excel/CSV Export: All data or weekly summaries can be downloaded with a single click.

Firebase Authentication: Unauthorized access to data is restricted; secure login is required.


