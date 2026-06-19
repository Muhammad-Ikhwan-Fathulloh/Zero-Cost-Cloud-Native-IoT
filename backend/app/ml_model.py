import pandas as pd
from sklearn.ensemble import RandomForestClassifier, IsolationForest
import os
import pickle

MODEL_PATH = "data/dht22_model.pkl"
ANOMALY_MODEL_PATH = "data/anomaly_model.pkl"

def train_model():
    """Train RandomForest for classification and IsolationForest for anomaly detection"""
    # 1. Random Forest - Classification
    data = [
        [22, 45, 0], [24, 50, 0], [26, 55, 0], [28, 60, 0], [30, 65, 0],
        [32, 70, 1], [34, 75, 1], [36, 80, 1], [38, 85, 1], [40, 90, 1],
        [20, 30, 0], [25, 35, 0], [27, 40, 0], [29, 45, 0], [31, 50, 0],
        [33, 55, 1], [35, 60, 1], [37, 65, 1], [39, 70, 1], [41, 75, 1]
    ]
    df = pd.DataFrame(data, columns=['temp', 'hum', 'condition'])
    X = df[['temp', 'hum']]
    y = df['condition']
    
    rf_model = RandomForestClassifier(n_estimators=100)
    rf_model.fit(X, y)
    
    # 2. Isolation Forest - Anomaly Detection
    # Using the same data as "normal" baseline
    iso_forest = IsolationForest(contamination=0.1, random_state=42)
    iso_forest.fit(X)
    
    # Ensure data directory exists
    os.makedirs(os.path.dirname(MODEL_PATH), exist_ok=True)
    
    with open(MODEL_PATH, 'wb') as f:
        pickle.dump(rf_model, f)
    
    with open(ANOMALY_MODEL_PATH, 'wb') as f:
        pickle.dump(iso_forest, f)
    
    print("Models trained and saved locally.")

def predict_condition(temp, hum):
    """Predict if cooling is needed"""
    if not os.path.exists(MODEL_PATH):
        train_model()
    with open(MODEL_PATH, 'rb') as f:
        model = pickle.load(f)
    X = pd.DataFrame([[temp, hum]], columns=['temp', 'hum'])
    return int(model.predict(X)[0])

def check_anomaly(temp, hum):
    """Check if the sensor reading is anomalous (-1 for anomaly, 1 for normal)"""
    if not os.path.exists(ANOMALY_MODEL_PATH):
        train_model()
    with open(ANOMALY_MODEL_PATH, 'rb') as f:
        model = pickle.load(f)
    X = pd.DataFrame([[temp, hum]], columns=['temp', 'hum'])
    prediction = model.predict(X)
    return int(prediction[0])
