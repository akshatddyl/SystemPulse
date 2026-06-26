import numpy as np
from fastapi import FastAPI, Request
from collections import deque
import uvicorn

app = FastAPI()

# Window size for the rolling Z-Score baseline
WINDOW_SIZE = 30
# Z-Score above 3.0 (3 standard deviations) is considered an anomaly
Z_SCORE_THRESHOLD = 3.0

# Store recent metrics for calculating the rolling mean and stddev
cpu_history = deque(maxlen=WINDOW_SIZE)
mem_history = deque(maxlen=WINDOW_SIZE)

# ANSI escape codes for colorful console printing
COLOR_RESET = "\033[0m"
COLOR_RED_BG = "\033[41;1m"
COLOR_YELLOW = "\033[33;1m"
COLOR_GREEN = "\033[32m"
COLOR_CYAN = "\033[36m"

def check_anomaly(metric_name: str, value: float, history: deque):
    """
    Calculates the rolling Z-Score and updates the history buffer.
    Returns (is_anomaly: bool, z_score: float)
    """
    if len(history) < 10:
        # Not enough data for a stable baseline
        history.append(value)
        return False, 0.0

    mean = np.mean(history)
    std = np.std(history)
    
    # Avoid division by zero if variance is exactly 0
    if std == 0:
        std = 1e-6
        
    z_score = (value - mean) / std
    
    history.append(value)
    
    is_anomaly = bool(abs(z_score) > Z_SCORE_THRESHOLD)
    return is_anomaly, float(z_score)

@app.post("/analyze")
async def analyze_metrics(request: Request):
    """
    Receives metric payloads from the C++ Broker.
    Supports either single JSON objects or arrays of JSON objects.
    """
    try:
        data = await request.json()
        
        if isinstance(data, list):
            metrics_list = data
        else:
            metrics_list = [data]
            
        anomalies_detected = []
            
        for metrics in metrics_list:
            cpu_usage = metrics.get("cpu_usage_percent", 0.0)
            mem_usage = metrics.get("mem_usage_percent", 0.0)
            timestamp = metrics.get("timestamp_ms", 0)

            cpu_anomaly, cpu_z = check_anomaly("CPU", cpu_usage, cpu_history)
            mem_anomaly, mem_z = check_anomaly("Memory", mem_usage, mem_history)

            if cpu_anomaly or mem_anomaly:
                print(f"\n{COLOR_RED_BG}!!! ANOMALY DETECTED !!!{COLOR_RESET}")
                if cpu_anomaly:
                    print(f"{COLOR_YELLOW}[CPU SPIKE] Value: {cpu_usage:.2f}% | Z-Score: {cpu_z:.2f}{COLOR_RESET}")
                if mem_anomaly:
                    print(f"{COLOR_YELLOW}[MEM SPIKE] Value: {mem_usage:.2f}% | Z-Score: {mem_z:.2f}{COLOR_RESET}")
                print(f"{COLOR_CYAN}Timestamp: {timestamp}{COLOR_RESET}\n")
                
                anomalies_detected.append({
                    "timestamp_ms": timestamp,
                    "cpu_anomaly": cpu_anomaly,
                    "mem_anomaly": mem_anomaly,
                    "cpu_z_score": cpu_z,
                    "mem_z_score": mem_z
                })
            else:
                # Normal operational log
                print(f"{COLOR_GREEN}[OK]{COLOR_RESET} CPU: {cpu_usage:.2f}%, MEM: {mem_usage:.2f}%")

        return {"status": "analyzed", "anomalies_found": len(anomalies_detected), "details": anomalies_detected}
        
    except Exception as e:
        print(f"{COLOR_RED_BG}Error processing data: {e}{COLOR_RESET}")
        return {"status": "error", "message": str(e)}

if __name__ == "__main__":
    print(f"Starting ML Processor on port 5000...")
    uvicorn.run(app, host="0.0.0.0", port=5000)
