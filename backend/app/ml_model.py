import joblib
import numpy as np
from sklearn.ensemble import RandomForestClassifier
import os

MODEL_PATH = "data/dht22_model.pkl"

def train_model():
    """Retrain model using data from DynamoDB (LocalStack)"""
    from .aws_service import dynamodb
    
    try:
        table = dynamodb.Table('IoT_Sensor_Data')
        response = table.scan()
        items = response.get('Items', [])
        
        # Default training data if DynamoDB is empty or has too little data
        if len(items) < 10:
            print("Using fallback data for training (Insufficient data in DynamoDB)")
            X = np.array([[24,50], [25,55], [22,60], [35,30], [40,20], [32,35], [20,80], [18,90], [38,15], [42,10]])
            y = np.array([0, 0, 0, 1, 1, 1, 0, 0, 1, 1])
        else:
            X = []
            y = []
            for item in items:
                X.append([float(item['temp']), float(item['hum'])])
                # Training on previous decisions (Reinforcement-like pattern)
                y.append(1 if item['led_status'] == "ON" else 0)
            X = np.array(X)
            y = np.array(y)
            print(f"Retraining with {len(items)} samples from DynamoDB.")

        model = RandomForestClassifier(n_estimators=10)
        model.fit(X, y)
        
        os.makedirs('data', exist_ok=True)
        joblib.dump(model, MODEL_PATH)
        return model
        
    except Exception as e:
        print(f"Training Error: {e}")
        # Fallback to hardcoded if DB fails
        X = np.array([[24,50], [35,30]])
        y = np.array([0, 1])
        model = RandomForestClassifier(n_estimators=10)
        model.fit(X, y)
        joblib.dump(model, MODEL_PATH)
        return model

def predict_condition(temp, hum):
    if not os.path.exists(MODEL_PATH):
        train_model()
    model = joblib.load(MODEL_PATH)
    prediction = model.predict([[temp, hum]])
    return int(prediction[0])