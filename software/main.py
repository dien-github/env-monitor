import json
import sqlite3
import ssl
import uuid
from datetime import datetime
from typing import Set
from queue import Queue

from fastapi import FastAPI, Request, HTTPException, WebSocket, WebSocketDisconnect
from fastapi.responses import HTMLResponse, JSONResponse
from fastapi.templating import Jinja2Templates
from fastapi.staticfiles import StaticFiles
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
websocket_clients: Set[WebSocket] = set()
message_queue: Queue = Queue()

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
    c.execute("""
        CREATE TABLE IF NOT EXISTS device_status (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            type TEXT,
            state TEXT,
            ts TEXT
        )
    """)
    c.execute("""
        CREATE TABLE IF NOT EXISTS connection_status (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            status TEXT,
            ts TEXT
        )
    """)
    c.execute("""
        CREATE TABLE IF NOT EXISTS system_status (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            uptime_ms INTEGER,
            free_heap INTEGER,
            rssi REAL,
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

def save_device_status(device_type: str, state: str):
    conn = sqlite3.connect(DB_NAME)
    c = conn.cursor()
    c.execute(
        "INSERT INTO device_status (type, state, ts) VALUES (?, ?, ?)",
        (device_type, state, datetime.utcnow().isoformat())
    )
    conn.commit()
    conn.close()

def get_device_status(device_type: str):
    conn = sqlite3.connect(DB_NAME)
    c = conn.cursor()
    c.execute(
        "SELECT state FROM device_status WHERE type = ? ORDER BY id DESC LIMIT 1",
        (device_type,)
    )
    row = c.fetchone()
    conn.close()
    if not row:
        return {"state": "unknown"}
    return {"state": row[0]}

def save_connection_status(status: str):
    conn = sqlite3.connect(DB_NAME)
    c = conn.cursor()
    c.execute(
        "INSERT INTO connection_status (status, ts) VALUES (?, ?)",
        (status, datetime.utcnow().isoformat())
    )
    conn.commit()
    conn.close()

def get_connection_status():
    conn = sqlite3.connect(DB_NAME)
    c = conn.cursor()
    c.execute(
        "SELECT status FROM connection_status ORDER BY id DESC LIMIT 1"
    )
    row = c.fetchone()
    conn.close()
    if not row:
        return {"status": "offline"}
    return {"status": row[0]}

def save_system_status(uptime_ms: int, free_heap: int, rssi: float):
    conn = sqlite3.connect(DB_NAME)
    c = conn.cursor()
    c.execute(
        "INSERT INTO system_status (uptime_ms, free_heap, rssi, ts) VALUES (?, ?, ?, ?)",
        (uptime_ms, free_heap, rssi, datetime.utcnow().isoformat())
    )
    conn.commit()
    conn.close()

def get_system_status():
    conn = sqlite3.connect(DB_NAME)
    c = conn.cursor()
    c.execute(
        "SELECT uptime_ms, free_heap, rssi FROM system_status ORDER BY id DESC LIMIT 1"
    )
    row = c.fetchone()
    conn.close()
    if not row:
        return {"uptime_ms": 0, "free_heap": 0, "rssi": -100}
    return {"uptime_ms": int(row[0] or 0), "free_heap": int(row[1] or 0), "rssi": float(row[2] or -100)}

# ================= MQTT =================
def on_connect(client, userdata, flags, rc):
    if rc == 0:
        print("MQTT connected")
        client.subscribe(TOPIC_SENSOR, qos=1)
        client.subscribe("room_01/status/connection", qos=1)
        client.subscribe("room_01/status/network", qos=1)
        client.subscribe("room_01/status/devices", qos=1)
        client.subscribe("room_01/status/system", qos=0)
    else:
        print("MQTT connect failed:", rc)

def on_message(client, userdata, msg):
    # Decode payload to text, then try JSON; accept raw for connection topics
    raw_text = ""
    payload_obj = None
    try:
        raw_text = msg.payload.decode()
    except Exception:
        raw_text = ""

    try:
        payload_obj = json.loads(raw_text)
    except Exception:
        payload_obj = None

    if msg.topic == TOPIC_SENSOR:
        if not isinstance(payload_obj, dict):
            print("Invalid JSON from MQTT (sensor)")
            return
        temp = payload_obj.get("temperature")
        hum = payload_obj.get("humidity")
        if temp is not None and hum is not None:
            save_sensor(float(temp), float(hum))
            broadcast_message({
                "type": "sensor",
                "temperature": float(temp),
                "humidity": float(hum),
                "timestamp": payload_obj.get("timestamp")
            })
            print(f"Saved sensor: T={temp}, H={hum}")
        else:
            print("Invalid sensor payload (missing fields)")

    elif msg.topic in ("room_01/status/connection", "room_01/status/network"):
        if isinstance(payload_obj, dict):
            status = payload_obj.get("status", "offline")
        else:
            status = (raw_text or "").strip().strip('"').lower() or "offline"
        save_connection_status(status)
        broadcast_message({
            "type": "connection",
            "status": status
        })
        print(f"Connection status: {status}")

    elif msg.topic == "room_01/status/system":
        if not isinstance(payload_obj, dict):
            print("Invalid JSON from MQTT (system)")
            return
        uptime_ms = payload_obj.get("uptime_ms")
        free_heap = payload_obj.get("free_heap")
        rssi = payload_obj.get("rssi")
        if rssi is None:
            print("System payload missing rssi")
            return
        save_system_status(int(uptime_ms or 0), int(free_heap or 0), float(rssi))
        broadcast_message({
            "type": "system",
            "uptime_ms": int(uptime_ms or 0),
            "free_heap": int(free_heap or 0),
            "rssi": float(rssi)
        })
        print(f"System status: rssi={rssi}, heap={free_heap}, uptime={uptime_ms}")

    elif msg.topic == "room_01/status/devices":
        if not isinstance(payload_obj, dict):
            print("Invalid JSON from MQTT (devices)")
            return
        device_type = payload_obj.get("device")
        state = payload_obj.get("state")
        if device_type and state:
            save_device_status(device_type, state)
            broadcast_message({
                "type": "device",
                "device": device_type,
                "state": state,
                "timestamp": payload_obj.get("timestamp")
            })
            print(f"Device {device_type} status: {state}")
        else:
            print("Invalid device payload (missing fields)")

# ================= BROADCAST TASK =================
def broadcast_message(data: dict):
    """Queue message for broadcast to all connected WebSocket clients"""
    message_queue.put(data)

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

async def broadcast_pending_messages():
    """Background task to broadcast queued messages to WebSocket clients"""
    import asyncio
    while True:
        try:
            if not message_queue.empty():
                data = message_queue.get_nowait()
                clients_copy = list(websocket_clients)
                for ws in clients_copy:
                    try:
                        await ws.send_json(data)
                    except Exception as e:
                        print(f"Error sending to client: {e}")
                        websocket_clients.discard(ws)
            await asyncio.sleep(0.01)
        except Exception as e:
            print(f"Error in broadcast task: {e}")
            await asyncio.sleep(0.1)

# ================= FASTAPI =================
@app.on_event("startup")
async def startup():
    init_db()
    start_mqtt()
    # Start background broadcast task
    import asyncio
    asyncio.create_task(broadcast_pending_messages())

@app.get("/", response_class=HTMLResponse)
async def index(request: Request):
    return templates.TemplateResponse("index.html", {"request": request})

@app.websocket("/ws")
async def websocket_endpoint(websocket: WebSocket):
    await websocket.accept()
    websocket_clients.add(websocket)
    print(f"Client connected. Total clients: {len(websocket_clients)}")
    
    try:
        # Send initial state to new client
        latest_sensor = get_latest_sensor()
        if latest_sensor:
            await websocket.send_json({
                "type": "sensor",
                "temperature": float(latest_sensor.get("temperature", 0)),
                "humidity": float(latest_sensor.get("humidity", 0))
            })
        
        connection_status = get_connection_status()
        await websocket.send_json({
            "type": "connection",
            "status": connection_status.get("status", "offline")
        })
        # Send latest system status (RSSI, heap, uptime)
        system_status = get_system_status()
        await websocket.send_json({
            "type": "system",
            "uptime_ms": system_status.get("uptime_ms", 0),
            "free_heap": system_status.get("free_heap", 0),
            "rssi": system_status.get("rssi", -100)
        })
        
        # Keep connection alive
        while True:
            data = await websocket.receive_text()
            print(f"Received: {data}")
    except WebSocketDisconnect:
        websocket_clients.discard(websocket)
        print(f"Client disconnected. Total clients: {len(websocket_clients)}")
    except Exception as e:
        print(f"WebSocket error: {e}")
        websocket_clients.discard(websocket)

@app.get("/api/status")
async def api_status():
    return JSONResponse(get_latest_sensor())

@app.get("/api/device-status")
async def api_device_status(device: str):
    return JSONResponse(get_device_status(device))

@app.get("/api/connection-status")
async def api_connection_status():
    return JSONResponse(get_connection_status())

@app.get("/api/system-status")
async def api_system_status():
    return JSONResponse(get_system_status())

@app.post("/api/command")
async def api_command(cmd: dict):
    """
    Expected from UI:
    { "type": "humidifier", "value": "on"/"off" }
    { "type": "fan", "value": "on"/"off" }
    """

    if "type" not in cmd:
        raise HTTPException(400, "Missing type")

    device_type = cmd["type"]
    value = cmd.get("value")

    # ===== CHUẨN HÓA ĐÚNG THEO FIRMWARE =====
    if device_type == "humidifier":
        payload = {
            "type": "humidifier",
            "state": "on" if value == "on" else "off"
        }

    elif device_type == "fan":
        payload = {
            "type": "fan",
            "state": "on" if value == "on" else "off"
        }

    else:
        raise HTTPException(400, "Unknown device type")

    mqtt_client.publish(
        TOPIC_COMMAND,
        json.dumps(payload),
        qos=1
    )

    return {"status": "ok", "sent": payload}
