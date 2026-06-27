import numpy as np
from fastapi import FastAPI, Request
from collections import deque
import uvicorn
from prometheus_client import Counter, Gauge, make_asgi_app

app = FastAPI()

# Prometheus metrics setup
anomalies_detected_total = Counter('ml_anomalies_detected_total', 'Total anomalies detected', ['metric_type', 'agent_id'])
current_z_score = Gauge('ml_current_z_score', 'Current Z-Score', ['metric_type', 'agent_id'])
messages_processed_total = Counter('ml_messages_processed_total', 'Total messages processed')
agent_cpu_usage = Gauge('agent_cpu_usage_percent', 'Agent CPU Usage', ['agent_id'])
agent_mem_usage = Gauge('agent_mem_usage_percent', 'Agent Mem Usage', ['agent_id'])

# Mount prometheus metrics endpoint
metrics_app = make_asgi_app()
app.mount("/metrics", metrics_app)

WINDOW_SIZE = 30
MIN_SAMPLES = 30
Z_SCORE_THRESHOLD = 3.0

# Store recent metrics per agent to ensure accurate rolling baselines
cpu_history = {}
mem_history = {}

COLOR_RESET = "\033[0m"
COLOR_RED_BG = "\033[41;1m"
COLOR_YELLOW = "\033[33;1m"
COLOR_GREEN = "\033[32m"
COLOR_CYAN = "\033[36m"
COLOR_MAGENTA = "\033[35m"

def check_anomaly(metric_name: str, value: float, history_dict: dict, agent_id: str):
    if agent_id not in history_dict:
        history_dict[agent_id] = deque(maxlen=WINDOW_SIZE)
    history = history_dict[agent_id]
    
    if len(history) < MIN_SAMPLES:
        history.append(value)
        # Returns (is_anomaly, z_score, is_warmup)
        return False, 0.0, True

    mean = np.mean(history)
    std = np.std(history)
    
    # Prevent Zero-Division & Micro-Spike dampen
    std += 1e-5
        
    z_score = (value - mean) / std
    history.append(value)
    
    # Absolute Threshold logic
    is_anomaly = abs(z_score) > Z_SCORE_THRESHOLD and (value > 20.0 or abs(value - mean) > 5.0)
    
    return is_anomaly, z_score, False

@app.post("/analyze")
async def analyze_metrics(request: Request):
    try:
        data = await request.json()
        
        if isinstance(data, list):
            metrics_list = data
        else:
            metrics_list = [data]
            
        anomalies_detected = []
            
        for metrics in metrics_list:
            messages_processed_total.inc()
            
            agent_id = metrics.get("agent_id", "unknown")
            cpu_usage = float(metrics.get("cpu_usage_percent", 0.0))
            mem_usage = float(metrics.get("mem_usage_percent", 0.0))
            timestamp = int(metrics.get("timestamp_ms", 0))

            agent_cpu_usage.labels(agent_id=agent_id).set(cpu_usage)
            agent_mem_usage.labels(agent_id=agent_id).set(mem_usage)

            cpu_anomaly, cpu_z, cpu_warmup = check_anomaly("CPU", cpu_usage, cpu_history, agent_id)
            mem_anomaly, mem_z, mem_warmup = check_anomaly("Memory", mem_usage, mem_history, agent_id)
            
            cpu_anomaly = bool(cpu_anomaly)
            mem_anomaly = bool(mem_anomaly)
            cpu_z = float(cpu_z)
            mem_z = float(mem_z)
            
            current_z_score.labels(metric_type='cpu', agent_id=agent_id).set(cpu_z)
            current_z_score.labels(metric_type='memory', agent_id=agent_id).set(mem_z)

            if cpu_anomaly or mem_anomaly:
                if cpu_anomaly:
                    anomalies_detected_total.labels(metric_type='cpu', agent_id=agent_id).inc()
                if mem_anomaly:
                    anomalies_detected_total.labels(metric_type='memory', agent_id=agent_id).inc()
                
                print(f"\n{COLOR_RED_BG}!!! ANOMALY DETECTED !!!{COLOR_RESET}")
                if cpu_anomaly:
                    print(f"{COLOR_YELLOW}[CPU SPIKE] Agent: {agent_id} | Value: {cpu_usage:.2f}% | Z-Score: {cpu_z:.2f}{COLOR_RESET}")
                if mem_anomaly:
                    print(f"{COLOR_YELLOW}[MEM SPIKE] Agent: {agent_id} | Value: {mem_usage:.2f}% | Z-Score: {mem_z:.2f}{COLOR_RESET}")
                print(f"{COLOR_CYAN}Timestamp: {timestamp}{COLOR_RESET}\n")
                
                anomalies_detected.append({
                    "agent_id": agent_id,
                    "timestamp_ms": timestamp,
                    "cpu_anomaly": cpu_anomaly,
                    "mem_anomaly": mem_anomaly,
                    "cpu_z_score": cpu_z,
                    "mem_z_score": mem_z
                })
            else:
                if cpu_warmup or mem_warmup:
                    print(f"{COLOR_MAGENTA}[WARMUP]{COLOR_RESET} Agent: {agent_id} | CPU: {cpu_usage:.2f}%, MEM: {mem_usage:.2f}%")
                else:
                    # Normal operational log
                    print(f"{COLOR_GREEN}[OK]{COLOR_RESET} Agent: {agent_id} | CPU: {cpu_usage:.2f}%, MEM: {mem_usage:.2f}%")

        return {"status": "analyzed", "anomalies_found": len(anomalies_detected), "details": anomalies_detected}
        
    except Exception as e:
        print(f"{COLOR_RED_BG}Error processing data: {e}{COLOR_RESET}")
        return {"status": "error", "message": str(e)}

if __name__ == "__main__":
    print(f"Starting ML Processor on port 5000 (Prometheus at /metrics)...")
    uvicorn.run(app, host="0.0.0.0", port=5000)
