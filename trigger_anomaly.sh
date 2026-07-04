#!/bin/bash

# Default values if no arguments are provided
TARGET_INDEX=${1:-1}
ANOMALY_TYPE=${2:-cpu}
TIMEOUT_SECS=${3:-10}

echo "=========================================================="
echo " INITIATING CHAOS TEST: SYSTEMPULSE TELEMETRY FLEET"
echo "=========================================================="
echo " Target Service Index : agent-$TARGET_INDEX"
echo " Anomaly Type         : $ANOMALY_TYPE"
echo " Duration             : $TIMEOUT_SECS seconds"
echo "=========================================================="

if [ "$ANOMALY_TYPE" = "cpu" ]; then
    echo "[Agent-$TARGET_INDEX] Maxing out 4 CPU workers..."
    docker compose exec --index=$TARGET_INDEX agent stress-ng --cpu 4 --timeout ${TIMEOUT_SECS}s
elif [ "$ANOMALY_TYPE" = "mem" ]; then
    echo "[Agent-$TARGET_INDEX] Allocating and thrashing memory (vm workers)..."
    docker compose exec --index=$TARGET_INDEX agent stress-ng --vm 2 --vm-bytes 50% --timeout ${TIMEOUT_SECS}s
else
    echo "Error: Invalid anomaly type. Please use 'cpu' or 'mem'."
    exit 1
fi

echo "=========================================================="
echo " CHAOS TEST COMPLETE. Check Grafana & ML-Processor Logs!"
echo "=========================================================="
