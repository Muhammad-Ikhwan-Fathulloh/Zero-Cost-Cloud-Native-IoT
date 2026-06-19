import boto3
import os
import json
import requests
from dotenv import load_dotenv

load_dotenv()

# Configuration
AWS_REGION = os.getenv("AWS_REGION", "us-east-1")
LOCALSTACK_ENDPOINT = os.getenv("LOCALSTACK_ENDPOINT", "http://localhost:4566")
QUEUE_NAME = "IoT_Sensor_Queue"
TOPIC_NAME = "IoT_Alerts"
TABLE_NAME = "IoT_Sensor_Data"
BUCKET_NAME = "iot-ai-models"
TELEGRAM_BOT_TOKEN = os.getenv("TELEGRAM_BOT_TOKEN")
TELEGRAM_CHAT_ID = os.getenv("TELEGRAM_CHAT_ID")

# Clients
s3 = boto3.client("s3", region_name=AWS_REGION, endpoint_url=LOCALSTACK_ENDPOINT)
sqs = boto3.client("sqs", region_name=AWS_REGION, endpoint_url=LOCALSTACK_ENDPOINT)
sns = boto3.client("sns", region_name=AWS_REGION, endpoint_url=LOCALSTACK_ENDPOINT)
dynamodb = boto3.resource("dynamodb", region_name=AWS_REGION, endpoint_url=LOCALSTACK_ENDPOINT)
cloudwatch = boto3.client("cloudwatch", region_name=AWS_REGION, endpoint_url=LOCALSTACK_ENDPOINT)

def init_resources():
    """Build Infrastructure in LocalStack (S3, SQS, SNS, DynamoDB)"""
    try:
        # 1. DynamoDB
        existing_tables = [table.name for table in dynamodb.tables.all()]
        if TABLE_NAME not in existing_tables:
            dynamodb.create_table(
                TableName=TABLE_NAME,
                KeySchema=[
                    {'AttributeName': 'id', 'KeyType': 'HASH'},
                    {'AttributeName': 'timestamp', 'KeyType': 'RANGE'}
                ],
                AttributeDefinitions=[
                    {'AttributeName': 'id', 'AttributeType': 'S'},
                    {'AttributeName': 'timestamp', 'AttributeType': 'S'}
                ],
                ProvisionedThroughput={'ReadCapacityUnits': 5, 'WriteCapacityUnits': 5}
            )
            print(f"Table {TABLE_NAME} created.")

        # 2. S3
        buckets = s3.list_buckets().get('Buckets', [])
        if not any(b['Name'] == BUCKET_NAME for b in buckets):
            s3.create_bucket(Bucket=BUCKET_NAME)
            print(f"Bucket {BUCKET_NAME} created.")

        # 3. SQS
        queues = sqs.list_queues().get('QueueUrls', [])
        if not any(QUEUE_NAME in q for q in queues):
            sqs.create_queue(QueueName=QUEUE_NAME)
            print(f"Queue {QUEUE_NAME} created.")

        # 4. SNS
        topics = sns.list_topics().get('Topics', [])
        if not any(TOPIC_NAME in t['TopicArn'] for t in topics):
            sns.create_topic(Name=TOPIC_NAME)
            print(f"Topic {TOPIC_NAME} created.")
            
    except Exception as e:
        print(f"Init Error: {e}")

def push_to_queue(message_body):
    try:
        queue_url = sqs.get_queue_url(QueueName=QUEUE_NAME)['QueueUrl']
        sqs.send_message(QueueUrl=queue_url, MessageBody=message_body)
    except Exception as e:
        print(f"SQS Error: {e}")

def send_alert(message):
    try:
        topics = sns.list_topics()['Topics']
        topic_arn = next(t['TopicArn'] for t in topics if TOPIC_NAME in t['TopicArn'])
        sns.publish(TopicArn=topic_arn, Message=message, Subject="IoT Critical Alert")
    except Exception as e:
        print(f"SNS Error: {e}")

def log_metric(name, value, unit='None'):
    try:
        cloudwatch.put_metric_data(
            Namespace='IoT/DHT22',
            MetricData=[{
                'MetricName': name,
                'Value': value,
                'Unit': unit
            }]
        )
    except Exception as e:
        print(f"CloudWatch Error: {e}")

def upload_model_to_s3(file_path, object_name):
    try:
        s3.upload_file(file_path, BUCKET_NAME, object_name)
        print(f"Model {object_name} uploaded to S3.")
    except Exception as e:
        print(f"S3 Upload Error: {e}")

def send_telegram_alert(message):
    if not TELEGRAM_BOT_TOKEN or not TELEGRAM_CHAT_ID:
        return
    url = f"https://api.telegram.org/bot{TELEGRAM_BOT_TOKEN}/sendMessage"
    payload = {"chat_id": TELEGRAM_CHAT_ID, "text": f"🚨 IoT ALERT: {message}"}
    try:
        requests.post(url, json=payload)
    except Exception as e:
        print(f"Telegram Error: {e}")

def create_cloudwatch_alarm(threshold=35.0):
    """Create a CloudWatch Alarm for high temperature"""
    try:
        cloudwatch.put_metric_alarm(
            AlarmName='High_Temperature_Alarm',
            ComparisonOperator='GreaterThanThreshold',
            EvaluationPeriods=1,
            MetricName='Temperature',
            Namespace='IoT/DHT22',
            Period=60,
            Statistic='Average',
            Threshold=threshold,
            ActionsEnabled=False,
            AlarmDescription=f'Triggered when sensor temp exceeds {threshold}C',
            Unit='None'
        )
    except Exception as e:
        print(f"CW Alarm Error: {e}")

def get_alarm_status():
    try:
        alarms = cloudwatch.describe_alarms(AlarmNames=['High_Temperature_Alarm'])
        if alarms['MetricAlarms']:
            alarm = alarms['MetricAlarms'][0]
            return {
                "name": alarm['AlarmName'],
                "state": alarm['StateValue'],
                "reason": alarm['StateReason']
            }
    except:
        pass
    return None
