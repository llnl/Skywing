#!/bin/bash
# run_tutorial_2.sh
# Script to launch Tutorial 2 with three agents in separate processes

echo "Starting Tutorial 2: Multiple Iterations - Computing Range"
echo "==========================================================="
echo ""
echo "This script launches 3 agents that compute the range (max - min)"
echo "using two concurrent iterations:"
echo "  - First iteration: finds the maximum value"
echo "  - Second iteration: finds the maximum distance from max"
echo ""
echo "Agent configurations:"
echo "  Agent 1 (port 20000): local_value = 42"
echo "  Agent 2 (port 20001): local_value = 73"
echo "  Agent 3 (port 20002): local_value = 15"
echo ""
echo "Expected results:"
echo "  Maximum = 73"
echo "  Range (max - min) = 73 - 15 = 58"
echo ""
echo "Press Ctrl+C to stop all agents"
echo ""
echo ""
echo "Port     Query    Local Val    Max          Distance     Range"
echo "-------------------------------------------------------------------"

# Set logging info to CRITICAL to suppress most logging output
# export LOGURU_LEVEL=CRITICAL

# Launch agents in background with & and redirect output
python tutorial_2_range.py \
    --no-print-header \
    --port 20000 \
    --nbr-ports 20001 \
    --local-value 42 &
AGENT1_PID=$!

python tutorial_2_range.py \
    --no-print-header \
    --port 20001 \
    --nbr-ports 20002 \
    --local-value 73 &
AGENT2_PID=$!

python tutorial_2_range.py \
    --no-print-header \
    --port 20002 \
    --nbr-ports 20000 \
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
