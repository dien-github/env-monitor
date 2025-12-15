import json
import sqlite3
import ssl
import asyncio
from fastapi import FastAPI, Request
from fastapi.responses import HTMLResponse
from fastapi.staticfiles import StaticFiles
from fastapi.templating import Jinja2Templates
from fastapi_mqtt import FastMQTT, MQTTConfig
from pydantic import BaseModel

# --- CẤU HÌNH ---
DB_NAME = "smarthome.db"
MQTT_HOST = "6753deb1228e4cc3a9e2847294ddefda.s1.eu.hivemq.cloud"
MQTT_PORT = 8883
MQTT_USERNAME = "env-monitor"
MQTT_PASSWORD = "abcABC@123"
TOPIC_SENSOR = "room_1/sensors"
TOPIC_COMMAND = "room_1/commands"

app = FastAPI()
templates = Jinja2Templates(directory="templates")

class CommandModel(BaseModel):
    type: str
    value: int

# --- DATABASE ---
def init_db():
    conn = sqlite3.connect(DB_NAME)
    c = conn.cursor()
    # Tạo bảng sensor_data đúng như Blueprint
    c.execute('''
        CREATE TABLE IF NOT EXISTS sensor_data (
        id INTEGER PRIMARY KEY AUTOINCREMENT, 
        timestamp DATETIME DEFAULT CURRENT_TIMESTAMP,
        temperature REAL, 
        humidity REAL);
    ''')
    conn.commit()
    conn.close()

init_db()

# --- MQTT CONFIG ---
ssl_context = ssl.create_default_context()

mqtt_config = MQTTConfig(
    host=MQTT_HOST,
    port=MQTT_PORT,
    username=MQTT_USERNAME,
    password=MQTT_PASSWORD,
    keepalive=30,
    ssl_context=ssl_context,
    reconnect_retries=10,
    reconnect_delay=5,
)
mqtt = FastMQTT(config=mqtt_config)
mqtt.init_app(app)

@mqtt.on_connect()
def connect(client, flags, rc, properties):
    print(f"Connected to MQTT. Subscribing to {TOPIC_SENSOR}")
    client.subscribe(TOPIC_SENSOR, qos=1)

@mqtt.on_disconnect()
def disconnect(client, packet, exc=None):
    print("Disconnected form MQTT Broker!")
    if exc:
        print(f"Error: {exc}")

@mqtt.on_message()
async def message(client, topic, payload, qos, properties):
    if topic == TOPIC_SENSOR:
        try:
            data = json.loads(payload.decode())
            # Parse đúng key: temperature, humidity
            temp = data.get("temperature")
            hum = data.get("humidity")
            
            if temp is not None and hum is not None:
                conn = sqlite3.connect(DB_NAME)
                c = conn.cursor()
                c.execute("INSERT INTO sensor_data (temperature, humidity) VALUES (?, ?)", (temp, hum))
                conn.commit()
                conn.close()
                print(f"Saved: Temp={temp}, Hum={hum}")
        except Exception as e:
            print(f"Error: {e}")


# --- API ENDPOINTS ---

@app.get("/", response_class=HTMLResponse)
async def read_root(request: Request):
    return templates.TemplateResponse("index.html", {"request": request})

@app.get("/api/status")
async def get_status():
    conn = sqlite3.connect(DB_NAME)
    c = conn.cursor()
    c.execute("SELECT temperature, humidity FROM sensor_data ORDER BY id DESC LIMIT 1")
    row = c.fetchone()
    conn.close()
    if row:
        return {"temperature": row[0], "humidity": row[1]}
    return {"temperature": 0, "humidity": 0}

@app.post("/api/command")
async def send_command(cmd: CommandModel):
    if not mqtt.client or not mqtt.client.is_connected():
        return {
            "status": "error",
            "message": "MQTT client not connected"
        }
    
    # Tạo payload đúng chuẩn {"type": "...", "value": ...}
    payload = {"type": cmd.type, "state": cmd.value}
    payload_str = json.dumps(payload)

    try:
        await mqtt.publish(TOPIC_COMMAND, json.dumps(payload), qos=1)
        print(f"Sent command: {payload_str}")
        return {"status": "sent", "payload": payload}
    except Exception as e:
        print(f"MQTT Publish Error: {e}")
        return {"status": "error", "message": str(e)}