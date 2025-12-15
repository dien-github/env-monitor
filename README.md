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



# Run webpage
1. Run `fastapi_setup.sh` to setup FastAPI environment.
2. Start server by `uvicorn main:app --host 0.0.0.0 --port 8000 --reload`
3. Access `http://<SERVER_IP>:8000/docs` to see the API documentation.