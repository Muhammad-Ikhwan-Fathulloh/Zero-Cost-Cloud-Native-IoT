from fastapi import FastAPI, WebSocket, WebSocketDisconnect, HTTPException, BackgroundTasks
from fastapi.middleware.cors import CORSMiddleware
from .ml_model import predict_condition, train_model
from .aws_service import (
    init_resources, dynamodb, send_alert, upload_model_to_s3, 
    push_to_queue, log_metric, sqs, QUEUE_NAME
)
import uuid
import datetime
import os
import json
import asyncio

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

@app.on_event("startup")
async def startup_event():
    init_resources()
    # Start background SQS consumer
    asyncio.create_task(sqs_worker())

async def sqs_worker():
    """Background task to process sensor data from SQS and save to DynamoDB"""
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
                
                # Persistence logic
                table.put_item(Item={
                    'id': data.get('id', str(uuid.uuid4())),
                    'timestamp': data.get('timestamp'),
                    'temp': data.get('temp'),
                    'hum': data.get('hum'),
                    'led_status': data.get('led_status'),
                    'mist_status': data.get('mist_status', 'OFF'),
                    'heater_status': data.get('heater_status', 'OFF'),
                    'mode_status': data.get('mode_status', 'ON')
                })
                
                # Delete message after processing
                sqs.delete_message(QueueUrl=queue_url, ReceiptHandle=msg['ReceiptHandle'])
                print(f"Processed SQS Message: {data.get('id')}")

        except Exception as e:
            print(f"Worker Error: {e}")
        await asyncio.sleep(1)

@app.get("/", tags=["General"])
async def root():
    return {"message": "IoT AI Bridge is Online"}

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

@app.websocket("/ws/sensor")
async def websocket_endpoint(websocket: WebSocket):
    await websocket.accept()
    print("ESP32 Connected")
    try:
        while True:
            data = await websocket.receive_json()
            temp = float(data.get("temp", 0))
            hum = float(data.get("hum", 0))

            # 1. CloudWatch Metric (Trend)
            log_metric('Temperature', temp, 'None')
            log_metric('Humidity', hum, 'None')

            # 2. AI Inference (Real-time) for Fan
            led_decision = predict_condition(temp, hum)
            led_status_str = "ON" if led_decision else "OFF"

            # Threshold logic for other components
            mist_decision = 1 if hum < 40.0 else 0
            heater_decision = 1 if temp < 24.0 else 0
            mode_decision = 1  # Default Auto

            mist_status_str = "ON" if mist_decision else "OFF"
            heater_status_str = "ON" if heater_decision else "OFF"
            mode_status_str = "ON" if mode_decision else "OFF"

            # 3. SNS Alert
            if temp > 35.0:
                send_alert(f"CRITICAL: High Temperature Detected! Current: {temp}°C")

            # 4. SQS Push (Async Persistence)
            sensor_payload = {
                'id': str(uuid.uuid4()),
                'timestamp': datetime.datetime.now().isoformat(),
                'temp': temp,
                'hum': hum,
                'led_status': led_status_str,
                'mist_status': mist_status_str,
                'heater_status': heater_status_str,
                'mode_status': mode_status_str
            }
            push_to_queue(json.dumps(sensor_payload))

            # 5. Send Real-time Command back to ESP32
            await websocket.send_json({
                "led_control": 1 if led_decision else 0,
                "control_2": mist_decision,
                "control_3": heater_decision,
                "control_4": mode_decision,
                "msg": "AI & Rules Processed (SQS & CW Logged)",
                "alert": "High Temp" if temp > 35.0 else "Normal"
            })
    except WebSocketDisconnect:
        print("ESP32 Disconnected")
    except Exception as e:
        print(f"WS Error: {e}")

@app.post("/train", tags=["Machine Learning"])
async def run_model_training():
    try:
        model_path = "data/dht22_model.pkl"
        os.makedirs("data", exist_ok=True)
        train_model()
        upload_model_to_s3(model_path, "dht22_latest_model.pkl")
        return {
            "status": "success",
            "message": "Model trained and uploaded to S3",
            "s3_path": "s3://iot-ai-models/dht22_latest_model.pkl"
        }
    except Exception as e:
        raise HTTPException(status_code=500, detail=f"Training failed: {str(e)}")