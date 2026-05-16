import boto3
import os
from dotenv import load_dotenv

load_dotenv()

LOCALSTACK_ENDPOINT = os.getenv("LOCALSTACK_ENDPOINT", "http://localhost:4566")
AWS_REGION = os.getenv("AWS_REGION", "us-east-1")
AWS_ACCESS_KEY = os.getenv("AWS_ACCESS_KEY_ID", "test")
AWS_SECRET_KEY = os.getenv("AWS_SECRET_ACCESS_KEY", "test")

# Resources
dynamodb = boto3.resource(
    'dynamodb',
    endpoint_url=LOCALSTACK_ENDPOINT,
    region_name=AWS_REGION,
    aws_access_key_id=AWS_ACCESS_KEY,
    aws_secret_access_key=AWS_SECRET_KEY
)

s3 = boto3.client(
    's3',
    endpoint_url=LOCALSTACK_ENDPOINT,
    region_name=AWS_REGION,
    aws_access_key_id=AWS_ACCESS_KEY,
    aws_secret_access_key=AWS_SECRET_KEY
)

sns = boto3.client(
    'sns',
    endpoint_url=LOCALSTACK_ENDPOINT,
    region_name=AWS_REGION,
    aws_access_key_id=AWS_ACCESS_KEY,
    aws_secret_access_key=AWS_SECRET_KEY
)

sqs = boto3.client(
    'sqs',
    endpoint_url=LOCALSTACK_ENDPOINT,
    region_name=AWS_REGION,
    aws_access_key_id=AWS_ACCESS_KEY,
    aws_secret_access_key=AWS_SECRET_KEY
)

cloudwatch = boto3.client(
    'cloudwatch',
    endpoint_url=LOCALSTACK_ENDPOINT,
    region_name=AWS_REGION,
    aws_access_key_id=AWS_ACCESS_KEY,
    aws_secret_access_key=AWS_SECRET_KEY
)

BUCKET_NAME = "iot-ai-models"
TOPIC_NAME = "IoT_Alerts"
QUEUE_NAME = "IoT_Sensor_Queue"

def init_resources():
    try:
        # DynamoDB
        existing_tables = [t.name for t in dynamodb.tables.all()]
        if 'IoT_Sensor_Data' not in existing_tables:
            dynamodb.create_table(
                TableName='IoT_Sensor_Data',
                KeySchema=[{'AttributeName': 'id', 'KeyType': 'HASH'}],
                AttributeDefinitions=[{'AttributeName': 'id', 'AttributeType': 'S'}],
                ProvisionedThroughput={'ReadCapacityUnits': 5, 'WriteCapacityUnits': 5}
            )
            print("Table IoT_Sensor_Data created.")

        # S3
        buckets = s3.list_buckets().get('Buckets', [])
        if not any(b['Name'] == BUCKET_NAME for b in buckets):
            s3.create_bucket(Bucket=BUCKET_NAME)
            print(f"S3 Bucket {BUCKET_NAME} created.")

        # SNS
        sns.create_topic(Name=TOPIC_NAME)
        print(f"SNS Topic {TOPIC_NAME} ensured.")

        # SQS
        sqs.create_queue(QueueName=QUEUE_NAME)
        print(f"SQS Queue {QUEUE_NAME} ensured.")

    except Exception as e:
        print(f"AWS Initialization Error: {e}")

def upload_model_to_s3(file_path, object_name):
    try:
        s3.upload_file(file_path, BUCKET_NAME, object_name)
    except Exception as e:
        print(f"S3 Upload Error: {e}")

def send_alert(message):
    try:
        topic_arn = sns.create_topic(Name=TOPIC_NAME)['TopicArn']
        sns.publish(TopicArn=topic_arn, Message=message, Subject="IoT Critical Alert")
    except Exception as e:
        print(f"SNS Error: {e}")

def push_to_queue(message_body):
    """Push sensor data to SQS for async processing"""
    try:
        queue_url = sqs.get_queue_url(QueueName=QUEUE_NAME)['QueueUrl']
        sqs.send_message(QueueUrl=queue_url, MessageBody=message_body)
    except Exception as e:
        print(f"SQS Error: {e}")

def log_metric(metric_name, value, unit='None'):
    """Log sensor metrics to CloudWatch"""
    try:
        cloudwatch.put_metric_data(
            Namespace='IoT/DHT22',
            MetricData=[
                {
                    'MetricName': metric_name,
                    'Value': float(value),
                    'Unit': unit
                }
            ]
        )
    except Exception as e:
        print(f"CloudWatch Error: {e}")

def create_cloudwatch_alarm(threshold=30.0):
    """Create a CloudWatch Alarm for High Temperature"""
    try:
        cloudwatch.put_metric_alarm(
            AlarmName='High_Temperature_Alarm',
            ComparisonOperator='GreaterThanThreshold',
            EvaluationPeriods=1,
            MetricName='Temperature',
            Namespace='IoT/DHT22',
            Period=60,
            Statistic='Average',
            Threshold=float(threshold),
            ActionsEnabled=False,
            AlarmDescription=f'Alarm when temperature exceeds {threshold}C',
            Unit='None'
        )
        print(f"CloudWatch Alarm created/updated with threshold: {threshold}")
    except Exception as e:
        print(f"CloudWatch Alarm Creation Error: {e}")

def get_alarm_status():
    """Fetch the current state of CloudWatch Alarms"""
    try:
        response = cloudwatch.describe_alarms(AlarmNames=['High_Temperature_Alarm'])
        alarms = response.get('MetricAlarms', [])
        if alarms:
            return {
                "name": alarms[0]['AlarmName'],
                "state": alarms[0]['StateValue'], # OK, ALARM, INSUFFICIENT_DATA
                "reason": alarms[0].get('StateReason', 'No reason')
            }
        return None
    except Exception as e:
        print(f"Error fetching alarm status: {e}")
        return None

import requests
TELEGRAM_TOKEN = os.getenv("TELEGRAM_BOT_TOKEN")
TELEGRAM_CHAT_ID = os.getenv("TELEGRAM_CHAT_ID")

def send_telegram_alert(message):
    """Send alert notification to Telegram Bot"""
    if not TELEGRAM_TOKEN or not TELEGRAM_CHAT_ID:
        return
    
    url = f"https://api.telegram.org/bot{TELEGRAM_TOKEN}/sendMessage"
    payload = {
        "chat_id": TELEGRAM_CHAT_ID,
        "text": f"🚨 *IoT ALERT* 🚨\n\n{message}",
        "parse_mode": "Markdown"
    }
    try:
        requests.post(url, json=payload, timeout=5)
    except Exception as e:
        print(f"Telegram Error: {e}")