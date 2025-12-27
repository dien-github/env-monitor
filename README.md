DHT11 -> GPIO4
Status:
- Source Code: Done - Not verified
- Hardware: DHT11 dead - DHT22 dead (just software verification, not tested with VOM)

MQTT -> MQTT Broker
Status:
- Source Code: Done - Verified - Not Clean - Too much Hardcode

# setup esp32
1. Run idf.py menuconfig to set WiFi SSID and Password, MQTT credential.
2. Build and flash the firmware to ESP32.

# Run webpage on Linux
1. Run `fastapi_setup.sh` to setup FastAPI environment.
2. Start server by `. server_start.sh`


# Ideas
- Get temperature and humidity outdoor through Weather API in backend to display outdoor data on webpage. Compare indoor and outdoor data. Alert user when difference is too high.
- More rooms monitoring.

# Scripts
T1 Connect WiFi 
    wsl
    show log => khoanh code lại 
T2 Connect MQTT
    wsl
    show log => khoanh code lại
T3 Read sensor
    wsl->hivemq->webpage
T4 Control actuators
    webpage->hivemq->wsl->real

# MQTT Topics

| Topic Path | Goal | Publisher | Payload | QoS | Retain | Trigger |
| --- | --- | --- | --- | --- | --- | --- |
| room_01/sensors | Send environment data from the sensor | ESP32 | {"temperature": 28.5, "humidity": 65.0, "timestamp": 12345678} | 1 | FALSE | Every 2 seconds |
| room_01/commands | Send control command from the server | Server | {"type": "fan", "state": "on"} | 1 | FALSE | When the user turns a device on of off |
| room_01/status/connection | Monitor the connection state of ESP32 with the broker using the LWT mechanism | ESP32 & Broker | "online"/"offline" | 1 | TRUE | During mqtt_service_start: configure LWT message as "offline"; on connection callback: publish "online" |
| room_01/status/devices | Report the actual state of actuators after receiving a command | ESP32 (humid_task, fan_task) | {"device": "fan", "state": "on", "timestamp": 1234} | 1 | FALSE | Immediately after relay_set_level successfully executes the command in each task |
| room_01/status/system | Perform periodic health checks | ESP32 | {"uptime_ms": 1234, "free_heap": 2048, "rssi": -65} | 0 | FALSE | Every 1 minute |
