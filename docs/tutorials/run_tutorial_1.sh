#!/bin/bash
# run_tutorial_1.sh
# Script to launch Tutorial 1 with three agents in separate processes

echo "Starting Tutorial 1: Maximum Consensus"
echo "======================================="
echo ""
echo "This script launches 3 agents:"
echo "  Agent 1 (port 20000): local_value = 42"
echo "  Agent 2 (port 20001): local_value = 73"
echo "  Agent 3 (port 20002): local_value = 15"
echo ""
echo "Expected result: All agents should converge to Max = 73"
echo ""
echo "Press Ctrl+C to stop all agents"
echo ""
echo ""
echo "Port        Local Value      Time        Max Value"
echo "----------------------------------------------------------------------"

# Set logging info to CRITICAL to suppress most logging output
export LOGURU_LEVEL=CRITICAL

# Launch agents in background with & and redirect output
python tutorial_1_max.py \
    --no-print-header \
    --port 20000 \
    --nbr-ports 20001,20002 \
    --local-value 42 &
AGENT1_PID=$!

python tutorial_1_max.py \
    --no-print-header \
    --port 20001 \
    --nbr-ports 20000,20002 \
    --local-value 73 &
AGENT2_PID=$!

python tutorial_1_max.py \
    --no-print-header \
    --port 20002 \
    --nbr-ports 20000,20001 \
    --local-value 15 &
AGENT3_PID=$!

# Function to clean up background processes on exit
cleanup() {
    echo ""
    echo "Stopping all agents..."
    kill $AGENT1_PID $AGENT2_PID $AGENT3_PID 2>/dev/null
    wait $AGENT1_PID $AGENT2_PID $AGENT3_PID 2>/dev/null
    echo "All agents stopped."
    exit 0
}

# Trap Ctrl+C and call cleanup
trap cleanup SIGINT SIGTERM

# Wait for all background processes
wait
