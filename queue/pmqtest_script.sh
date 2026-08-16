#!/bin/bash

set -e

FILE=PMQTEST_WHOLE_LOAD

STRESS_TIMEOUT=180
STABILIZATION_TIME=15
PMQTEST_DURATION=60

STRESS_PID=""

cleanup() {
    if [ -n "$STRESS_PID" ] && kill -0 "$STRESS_PID" 2>/dev/null; then
        echo "Cleaning up stress-ng PID: $STRESS_PID"
        kill "$STRESS_PID" 2>/dev/null || true
        wait "$STRESS_PID" 2>/dev/null || true
    fi
}

trap cleanup EXIT INT TERM

OUT_DIR="${FILE}_results_$(date +%Y%m%d_%H%M%S)"
mkdir -p "$OUT_DIR"

PMQTEST_LOG="$OUT_DIR/${FILE}.log"
STRESS_LOG="$OUT_DIR/stress_ng.log"

echo "Output directory: $OUT_DIR"

if ! command -v pmqtest >/dev/null 2>&1; then
    echo "Error: pmqtest is not installed or not found."
    echo "Install rt-tests package:"
    echo "sudo apt install rt-tests"
    exit 1
fi

if ! command -v stress-ng >/dev/null 2>&1; then
    echo "Error: stress-ng is not installed or not found."
    echo "Install stress-ng:"
    echo "sudo apt install stress-ng"
    exit 1
fi

echo "--------------------------------------------------"
echo "Starting whole load scenario..."

stress-ng \
    --cpu 4 --cpu-method matrixprod \
    --vm 2 --vm-bytes 80% --vm-keep \
    --iomix 4 \
    --pipe 8 \
    --hdd 2 --hdd-bytes 1G \
    --timeout ${STRESS_TIMEOUT}s \
    --metrics-brief > "$STRESS_LOG" &

STRESS_PID=$!

echo "stress-ng PID: $STRESS_PID"
echo "Waiting ${STABILIZATION_TIME} seconds for load stabilization..."
sleep "$STABILIZATION_TIME"

if ! kill -0 "$STRESS_PID" 2>/dev/null; then
    echo "ERROR: stress-ng stopped before pmqtest started!"
    echo "This test is invalid because whole load was not active at pmqtest start."
    echo "Check stress-ng log: $STRESS_LOG"
    exit 1
fi

echo "stress-ng is active."
echo "Starting pmqtest..."
echo "Command: pmqtest -Sp98 -i100 -d0 -D${PMQTEST_DURATION}"
echo "Output file: $PMQTEST_LOG"

pmqtest -Sp98 -i100 -d0 -D${PMQTEST_DURATION} > "$PMQTEST_LOG"

echo "pmqtest finished."

if ! kill -0 "$STRESS_PID" 2>/dev/null; then
    echo "WARNING: stress-ng stopped before cleanup."
    echo "pmqtest may still have completed, but check the stress-ng log:"
    echo "$STRESS_LOG"
else
    echo "Stopping whole load scenario..."
fi

cleanup
STRESS_PID=""

echo "--------------------------------------------------"
echo "Test completed."
echo "pmqtest result: $PMQTEST_LOG"
echo "stress-ng log: $STRESS_LOG"