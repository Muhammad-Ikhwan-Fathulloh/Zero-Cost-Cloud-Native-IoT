import joblib
import numpy as np
from sklearn.ensemble import RandomForestClassifier
import os

MODEL_PATH = "data/dht22_model.pkl"

def train_model():
    # X: [Suhu, Kelembapan], y: [0: Normal, 1: Anomali/LED ON]
    X = np.array([[24,50], [25,55], [22,60], [35,30], [40,20], [32,35]])
    y = np.array([0, 0, 0, 1, 1, 1])
    model = RandomForestClassifier(n_estimators=10)
    model.fit(X, y)
    os.makedirs('data', exist_ok=True)
    joblib.dump(model, MODEL_PATH)
    return model

def predict_condition(temp, hum):
    if not os.path.exists(MODEL_PATH):
        train_model()
    model = joblib.load(MODEL_PATH)
    prediction = model.predict([[temp, hum]])
    return int(prediction[0])