from fastapi import FastAPI, WebSocket, WebSocketDisconnect, HTTPException, BackgroundTasks
from fastapi.middleware.cors import CORSMiddleware
from .ml_model import predict_condition, train_model
from .aws_service import (
    init_resources, dynamodb, send_alert, upload_model_to_s3, 
    push_to_queue, log_metric, sqs, QUEUE_NAME, send_telegram_alert
)
import uuid
import datetime
import os
import json
import asyncio
from decimal import Decimal
from pydantic import BaseModel

app = FastAPI(
    title="Noc Lab IoT Bridge",
    description="IoT Monitoring with S3, SNS, SQS, DynamoDB, & CloudWatch via LocalStack",
    version="3.0.0"
)

app.add_middleware(
    CORSMiddleware,
    allow_origins=["*"],
    allow_credentials=True,
    allow_methods=["*"],
    allow_headers=["*"],
)

# --- WebSocket & State Management ---
class ConnectionManager:
    def __init__(self):
        self.active_connections: list[WebSocket] = []
        self.esp32_connection: WebSocket = None

    async def connect(self, websocket: WebSocket, is_esp32: bool = False):
        await websocket.accept()
        self.active_connections.append(websocket)
        if is_esp32:
            self.esp32_connection = websocket

    def disconnect(self, websocket: WebSocket):
        if websocket in self.active_connections:
            self.active_connections.remove(websocket)
        if self.esp32_connection == websocket:
            self.esp32_connection = None

    async def broadcast_to_ui(self, message: dict):
        for connection in self.active_connections:
            if connection != self.esp32_connection:
                try:
                    await connection.send_json(message)
                except:
                    pass

    async def send_to_esp32(self, message: dict):
        if self.esp32_connection:
            await self.esp32_connection.send_json(message)

manager = ConnectionManager()

# Real-time System State
system_state = {
    "mode": "AUTO", # AUTO or MANUAL
    "kipas": 0,
    "mist": 0,
    "heater": 0
}

class ControlRequest(BaseModel):
    component: str  # kipas, mist, heater, mode
    status: int     # 0 or 1

# --- Background Worker ---
@app.on_event("startup")
async def startup_event():
    init_resources()
    asyncio.create_task(sqs_worker())

async def sqs_worker():
    print("SQS Worker Started...")
    while True:
        try:
            queue_url = sqs.get_queue_url(QueueName=QUEUE_NAME)['QueueUrl']
            response = sqs.receive_message(
                QueueUrl=queue_url,
                MaxNumberOfMessages=5,
                WaitTimeSeconds=10
            )

            messages = response.get('Messages', [])
            for msg in messages:
                data = json.loads(msg['Body'])
                table = dynamodb.Table('IoT_Sensor_Data')
                
                table.put_item(Item={
                    'id': data.get('id', str(uuid.uuid4())),
                    'timestamp': data.get('timestamp'),
                    'temp': Decimal(str(data.get('temp', 0))),
                    'hum': Decimal(str(data.get('hum', 0))),
                    'led_status': data.get('led_status'),
                    'mist_status': data.get('mist_status', 'OFF'),
                    'heater_status': data.get('heater_status', 'OFF'),
                    'mode_status': data.get('mode_status', 'ON')
                })
                sqs.delete_message(QueueUrl=queue_url, ReceiptHandle=msg['ReceiptHandle'])
        except Exception as e:
            print(f"Worker Error: {e}")
        await asyncio.sleep(1)

# --- Endpoints ---
@app.get("/", tags=["General"])
async def root():
    return {"message": "IoT AI Bridge is Online", "state": system_state}

@app.get("/logs", tags=["Monitoring"])
async def get_logs():
    try:
        table = dynamodb.Table('IoT_Sensor_Data')
        response = table.scan()
        items = response.get('Items', [])
        items.sort(key=lambda x: x['timestamp'], reverse=True)
        return items[:50]
    except Exception as e:
        raise HTTPException(status_code=500, detail=str(e))

@app.post("/control", tags=["Control"])
async def manual_control(req: ControlRequest):
    """Update system state and send to ESP32"""
    if req.component == "mode":
        system_state["mode"] = "MANUAL" if req.status == 1 else "AUTO"
    elif req.component in ["kipas", "mist", "heater"]:
        system_state[req.component] = req.status
        system_state["mode"] = "MANUAL" # Auto-switch to manual on override
    
    # Push update to ESP32
    await manager.send_to_esp32({
        "led_control": system_state["kipas"],
        "control_2": system_state["mist"],
        "control_3": system_state["heater"],
        "control_4": 1 if system_state["mode"] == "AUTO" else 0,
        "msg": f"Manual Override: {req.component}"
    })
    
    # Sync UI
    await manager.broadcast_to_ui({"type": "state_update", "state": system_state})
    return {"status": "success", "new_state": system_state}

@app.websocket("/ws/sensor")
async def websocket_sensor(websocket: WebSocket):
    await manager.connect(websocket, is_esp32=True)
    print("ESP32 Connected via WebSocket")
    try:
        while True:
            data = await websocket.receive_json()
            temp = float(data.get("temp", 0))
            hum = float(data.get("hum", 0))

            log_metric('Temperature', temp, 'None')
            log_metric('Humidity', hum, 'None')

            # Logic based on Mode
            if system_state["mode"] == "AUTO":
                # AI & Rules
                system_state["kipas"] = predict_condition(temp, hum)
                system_state["mist"] = 1 if hum < 40.0 else 0
                system_state["heater"] = 1 if temp < 24.0 else 0
            
            alert_msg = "High Temp" if temp > 35.0 else "Normal"
            if temp > 35.0:
                msg = f"High Temperature Detected: {temp}°C!"
                send_alert(f"CRITICAL: {msg}")
                send_telegram_alert(msg)
                await manager.broadcast_to_ui({
                    "type": "sys_alert",
                    "message": f"CRITICAL SNS & TELEGRAM ALERT: {msg}"
                })

            # Record Payload
            payload = {
                'id': str(uuid.uuid4()),
                'timestamp': datetime.datetime.now().isoformat(),
                'temp': temp, 'hum': hum,
                'led_status': "ON" if system_state["kipas"] else "OFF",
                'mist_status': "ON" if system_state["mist"] else "OFF",
                'heater_status': "ON" if system_state["heater"] else "OFF",
                'mode_status': system_state["mode"]
            }
            push_to_queue(json.dumps(payload))

            # Reply to ESP32
            await websocket.send_json({
                "led_control": system_state["kipas"],
                "control_2": system_state["mist"],
                "control_3": system_state["heater"],
                "control_4": 1 if system_state["mode"] == "AUTO" else 0,
                "alert": alert_msg
            })

            # Real-time Broadcast to Dashboard
            await manager.broadcast_to_ui({
                "type": "sensor_update",
                "data": payload
            })

    except WebSocketDisconnect:
        manager.disconnect(websocket)
        print("ESP32 Disconnected")
    except Exception as e:
        print(f"WS Sensor Error: {e}")
        manager.disconnect(websocket)

@app.websocket("/ws/ui")
async def websocket_ui(websocket: WebSocket):
    await manager.connect(websocket, is_esp32=False)
    try:
        # Send current state on connect
        await websocket.send_json({"type": "state_update", "state": system_state})
        while True:
            await websocket.receive_text() # Keep connection alive
    except WebSocketDisconnect:
        manager.disconnect(websocket)
    except Exception as e:
        manager.disconnect(websocket)

@app.post("/train", tags=["Machine Learning"])
async def run_model_training():
    try:
        train_model()
        upload_model_to_s3("data/dht22_model.pkl", "dht22_latest_model.pkl")
        return {"status": "success", "message": "Model trained and uploaded to S3"}
    except Exception as e:
        raise HTTPException(status_code=500, detail=str(e))