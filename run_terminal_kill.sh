#!/bin/bash

echo "🛑 Killing processes on ports 8001 and 9001..."

# Kill processes on port 8001
echo "Checking port 8001..."
PIDS_8001=$(lsof -t -i:8001 2>/dev/null || true)
if [ -n "$PIDS_8001" ]; then
    echo "Killing processes on port 8001: $PIDS_8001"
    kill -15 $PIDS_8001 2>/dev/null || true
    sleep 2
    # Force kill if still running
    kill -9 $PIDS_8001 2>/dev/null || true
    echo "✅ Port 8001 cleared"
else
    echo "✅ Port 8001 is already free"
fi

# Kill processes on port 9001
echo "Checking port 9001..."
PIDS_9001=$(lsof -t -i:9001 2>/dev/null || true)
if [ -n "$PIDS_9001" ]; then
    echo "Killing processes on port 9001: $PIDS_9001"
    kill -15 $PIDS_9001 2>/dev/null || true
    sleep 2
    # Force kill if still running
    kill -9 $PIDS_9001 2>/dev/null || true
    echo "✅ Port 9001 cleared"
else
    echo "✅ Port 9001 is already free"
fi

# Kill any market data related processes
echo "Cleaning up market data processes..."
pkill -f "market_simulator" 2>/dev/null || true
pkill -f "websocket_server" 2>/dev/null || true
pkill -f "feed_handler" 2>/dev/null || true
pkill -f "rag_api_server" 2>/dev/null || true
pkill -f "start_api.py" 2>/dev/null || true

echo "🎯 All processes cleaned up!"

# Verify ports are free
echo ""
echo "📊 Port status:"
if lsof -i:8001 >/dev/null 2>&1; then
    echo "❌ Port 8001 still occupied:"
    lsof -i:8001
else
    echo "✅ Port 8001 is free"
fi

if lsof -i:9001 >/dev/null 2>&1; then
    echo "❌ Port 9001 still occupied:"
    lsof -i:9001
else
    echo "✅ Port 9001 is free"
fi 