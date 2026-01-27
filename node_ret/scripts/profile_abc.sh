#!/bin/bash
./abc -f node_ret/v6.script &
PID=$!
sleep 0.5  # give it a moment to start
sample $PID 10 -file abc_profile.txt
wait $PID