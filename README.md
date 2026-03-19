PVision is an IoT project that monitors solar panel data in real-time via Firebase, utilizing an ESP32 microcontroller and an INA219 sensor. Through the web interface, instantaneous measurements and historical data can be viewed.

------------------------------------------------------------------------------

PVision System Architecture and Data Flow
The PVision system operates through an integrated data pipeline starting from the hardware layer to the cloud and finally to the user interface:

1. Hardware & Connectivity Layer
The core of the system consists of an ESP32 microcontroller integrated with an INA219 sensor. This unit measures solar panel parameters and transmits the data via Wi-Fi to the Firebase Realtime Database (RTDB).

2. Database Structure (Firebase RTDB)
The cloud database is organized into four main nodes to ensure efficient data management:

panel_1/: Stores real-time measurement data, updated every 5 seconds.

panel_1/system: Tracks the current operational status (e.g., active or sleep_mode).

History/: Contains logged data based on 2-minute averages for long-term tracking.

WeeklySummaries/: Stores automatically generated summaries compiled at the end of each week.

3. Web Interface (Frontend)
The user-facing side is built with standard web technologies and Chart.js for visualization, consisting of three primary modules:

Login (login.html): Provides secure access control using Firebase Authentication.

Dashboard (index.html): Features real-time monitoring and dynamic live charts.

Data Logs (history.html): Allows users to review historical records and export data to Excel.

--------------------------------------------------------------------------------------

Project Features
Real-time Monitoring: Voltage, current, and power values are updated every 5 seconds.

Automated Charting: Instantaneous trend graphs for the last 30 measurements.

Sleep Mode Support: If the voltage drops below 4V, the ESP32 enters deep sleep for 30 minutes, and a warning is displayed on the web interface.

Historical Data Logging: Average power and energy production data are saved to Firebase every 2 minutes.

Automated Data Cleanup: When the limit of 5,040 records (≈1 week) is exceeded, old logs are deleted, and a weekly summary is automatically generated.

Excel/CSV Export: All data or weekly summaries can be downloaded with a single click.

Firebase Authentication: Unauthorized access to data is restricted; secure login is required.


