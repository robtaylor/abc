#!/bin/bash
./abc -f node_ret/v6.script &
PID=$!

# Sample until the process exits
sample $PID -mayDie -file abc_profile.txt &
SAMPLE_PID=$!

# Wait for the main process to finish
wait $PID

# Kill the sample process if it's still running
kill $SAMPLE_PID 2>/dev/null

echo "Profiling complete. Results in abc_profile.txt"