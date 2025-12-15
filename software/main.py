import json
import sqlite3
import ssl
import uuid
from datetime import datetime

from fastapi import FastAPI, Request, HTTPException
from fastapi.responses import HTMLResponse, JSONResponse
from fastapi.templating import Jinja2Templates
from paho.mqtt import client as mqtt

# ================= CONFIG =================
DB_NAME = "smarthome.db"

MQTT_HOST = "6753deb1228e4cc3a9e2847294ddefda.s1.eu.hivemq.cloud"
MQTT_PORT = 8883
MQTT_USERNAME = "env-monitor"
MQTT_PASSWORD = "abcABC@123"

TOPIC_SENSOR = "room_01/sensors"
TOPIC_COMMAND = "room_01/commands"
# =========================================

app = FastAPI()
templates = Jinja2Templates(directory="templates")

mqtt_client: mqtt.Client | None = None

# ================= DATABASE =================
def init_db():
    conn = sqlite3.connect(DB_NAME)
    c = conn.cursor()
    c.execute("""
        CREATE TABLE IF NOT EXISTS sensors (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            temperature REAL,
            humidity REAL,
            ts TEXT
        )
    """)
    conn.commit()
    conn.close()

def save_sensor(temp: float, hum: float):
    conn = sqlite3.connect(DB_NAME)
    c = conn.cursor()
    c.execute(
        "INSERT INTO sensors (temperature, humidity, ts) VALUES (?, ?, ?)",
        (temp, hum, datetime.utcnow().isoformat())
    )
    conn.commit()
    conn.close()

def get_latest_sensor():
    conn = sqlite3.connect(DB_NAME)
    c = conn.cursor()
    c.execute(
        "SELECT temperature, humidity FROM sensors ORDER BY id DESC LIMIT 1"
    )
    row = c.fetchone()
    conn.close()
    if not row:
        return {}
    return {"temperature": f"{row[0]:.2f}", "humidity": f"{row[1]:.2f}"}

# ================= MQTT =================
def on_connect(client, userdata, flags, rc):
    if rc == 0:
        print("MQTT connected")
        client.subscribe(TOPIC_SENSOR, qos=1)
    else:
        print("MQTT connect failed:", rc)

def on_message(client, userdata, msg):
    try:
        payload = json.loads(msg.payload.decode())
    except Exception:
        print("Invalid JSON from MQTT")
        return

    if msg.topic == TOPIC_SENSOR:
        temp = payload.get("temperature")
        hum = payload.get("humidity")
        if temp is not None and hum is not None:
            save_sensor(float(temp), float(hum))
            print(f"Saved sensor: T={temp}, H={hum}")

def start_mqtt():
    global mqtt_client
    mqtt_client = mqtt.Client(
        client_id=f"backend-{uuid.uuid4().hex[:6]}",
        protocol=mqtt.MQTTv311
    )
    mqtt_client.username_pw_set(MQTT_USERNAME, MQTT_PASSWORD)
    mqtt_client.tls_set_context(ssl.create_default_context())

    mqtt_client.on_connect = on_connect
    mqtt_client.on_message = on_message

    mqtt_client.connect(MQTT_HOST, MQTT_PORT, 60)
    mqtt_client.loop_start()

# ================= FASTAPI =================
@app.on_event("startup")
def startup():
    init_db()
    start_mqtt()

@app.get("/", response_class=HTMLResponse)
async def index(request: Request):
    return templates.TemplateResponse("index.html", {"request": request})

@app.get("/api/status")
async def api_status():
    return JSONResponse(get_latest_sensor())

@app.post("/api/command")
async def api_command(cmd: dict):
    """
    Expected from UI:
    { "type": "relay", "value": 1 }
    { "type": "fan", "value": 120 }
    """

    if "type" not in cmd:
        raise HTTPException(400, "Missing type")

    device_type = cmd["type"]
    value = cmd.get("value")

    # ===== CHUẨN HÓA ĐÚNG THEO FIRMWARE =====
    if device_type == "relay":
        payload = {
            "type": "humidifier",
            "state": "on" if int(value) == 1 else "off"
        }

    elif device_type == "fan":
        payload = {
            "type": "fan",
            "state": int(value)
        }

    else:
        raise HTTPException(400, "Unknown device type")

    mqtt_client.publish(
        TOPIC_COMMAND,
        json.dumps(payload),
        qos=1
    )

    return {"status": "ok", "sent": payload}
