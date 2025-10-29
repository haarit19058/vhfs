#!/bin/bash
# run_fs.sh — build and run server/client with logs

# Stop on error
set -e

# Clean up old logs
rm -f server.log client.log

# Build both
echo "🧱 Building server and client..."
g++  -o server.o ./server/server.cpp
g++  -o client.o ./client/client.cpp

# Run both and log outputs
echo "🚀 Starting server and client..."
./server.o . 3030 > server.log 2>&1 &
SERVER_PID=$!
echo "Server running with PID $SERVER_PID"

# Wait a moment for the server to initialize
sleep 1

./client.o mntdir 127.0.0.1 3030 > client.log 2>&1  &
CLIENT_PID=$!
echo "Client running with PID $CLIENT_PID"

# Wait for both to finish (or press Ctrl+C to stop)
wait $SERVER_PID $CLIENT_PID


rm server.o client.o

