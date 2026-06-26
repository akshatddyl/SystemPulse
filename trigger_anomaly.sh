#!/bin/bash

echo "=========================================================="
echo "    Triggering CPU Spike Anomaly Stress Test on agent-1   "
echo "=========================================================="

# Uses docker compose exec to target the agent-1 container and run stress-ng.
# By maxing out 4 CPU workers for 10 seconds, this will reliably cause
# the C++ MetricsCollector to capture a 99%+ CPU spike and stream it to the Broker.
docker compose exec agent-1 stress-ng --cpu 4 --timeout 10s

echo "=========================================================="
echo "    Stress test completed. Check ML-Processor Logs!       "
echo "=========================================================="
