DHT11 -> GPIO4
Status:
- Source Code: Done - Not verified
- Hardware: DHT11 dead - DHT22 dead (just software verification, not tested with VOM)

MQTT -> MQTT Broker
Status:
- Source Code: Done - Verified - Not Clean - Too much Hardcode

Relay -> GPIO 16

{"type":"humidifier","state":"on"}

MOSFET -> GPIO 21

# setup esp32
1. Run idf.py menuconfig to set WiFi SSID and Password, MQTT credential.
2. Build and flash the firmware to ESP32.

# Run webpage on Linux
1. Run `fastapi_setup.sh` to setup FastAPI environment.
2. Start server by `. server_start.sh`