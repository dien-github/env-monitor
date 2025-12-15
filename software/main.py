import json
import ssl
import sqlite3
from fastapi import FastAPI, Request
from fastapi.responses import HTMLResponse
from fastapi.templating import Jinja2Templates
from pydantic import BaseModel

from fastapi_mqtt import FastMQTT, MQTTConfig

# ================== CONFIG ==================
DB_NAME = "smarthome.db"

MQTT_HOST = "6753deb1228e4cc3a9e2847294ddefda.s1.eu.hivemq.cloud"
MQTT_PORT = 8883
MQTT_USERNAME = "env-monitor"
MQTT_PASSWORD = "abcABC@123"

TOPIC_SENSOR = "room_1/sensors"
TOPIC_COMMAND = "room_1/commands"

# ================== APP ==================
app = FastAPI()
templates = Jinja2Templates(directory="templates")

# ================== MQTT ==================
mqtt_config = MQTTConfig(
    host=MQTT_HOST,
    port=MQTT_PORT,
    username=MQTT_USERNAME,
    password=MQTT_PASSWORD,
    ssl=True,
    ssl_params={
        "cert_reqs": ssl.CERT_REQUIRED,
        "tls_version": ssl.PROTOCOL_TLS
    }
)

mqtt = FastMQTT(config=mqtt_config)
mqtt.init_app(app)

# ================== DB ==================
def init_db():
    conn = sqlite3.connect(DB_NAME)
    c = conn.cursor()
    c.execute("""
        CREATE TABLE IF NOT EXISTS sensors (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            temperature REAL,
            humidity REAL,
            ts DATETIME DEFAULT CURRENT_TIMESTAMP
        )
    """)
    conn.commit()
    conn.close()

init_db()

def save_sensor(temp, hum):
    conn = sqlite3.connect(DB_NAME)
    c = conn.cursor()
    c.execute(
        "INSERT INTO sensors (temperature, humidity) VALUES (?, ?)",
        (temp, hum)
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
    if row:
        return {"temperature": row[0], "humidity": row[1]}
    return {"temperature": None, "humidity": None}

# ================== MQTT HANDLER ==================
@mqtt.on_connect()
def connect(client, flags, rc, properties):
    print("MQTT Connected")
    mqtt.subscribe(TOPIC_SENSOR)

@mqtt.on_message()
async def message(client, topic, payload, qos, properties):
    if topic == TOPIC_SENSOR:
        try:
            data = json.loads(payload.decode())
            temp = data.get("temperature")
            hum = data.get("humidity")
            if temp is not None and hum is not None:
