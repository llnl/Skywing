#!/bin/bash
# run_tutorial_3.sh
# Script to launch Tutorial 3 with three agents in separate processes

echo "Starting Tutorial 3: Custom Jacobi Processor"
echo "=============================================="
echo ""
echo "Solving the linear system:"
echo "  4x + y + z = 7"
echo "  x + 4y + z = 12"
echo "  x + y + 4z = 15"
echo ""
echo "This script launches 3 agents:"
echo "  Agent 0 (port 20000): Solves for x (expected: 1)"
echo "  Agent 1 (port 20001): Solves for y (expected: 2)"
echo "  Agent 2 (port 20002): Solves for z (expected: 3)"
echo ""
echo "Expected solution: x=1, y=2, z=3"
echo ""
echo "Press Ctrl+C to stop all agents"
echo ""
echo ""
echo "Agent   Iteration    x            y            z"
echo "--------------------------------------------------------"

# Set logging info to CRITICAL to suppress most logging output
export LOGURU_LEVEL=CRITICAL

# Launch agents in background with & and redirect output
python tutorial_3_custom_processor.py \
    --no-print-header \
    --port 20000 \
    --nbr-ports 20001,20002 \
    --agent-id 0 &
AGENT1_PID=$!

python tutorial_3_custom_processor.py \
    --no-print-header \
    --port 20001 \
    --nbr-ports 20000,20002 \
    --agent-id 1 &
AGENT2_PID=$!

python tutorial_3_custom_processor.py \
    --no-print-header \
    --port 20002 \
    --nbr-ports 20000,20001 \
    --agent-id 2 &
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
