#!/usr/bin/env python3

import asyncio
import websockets
import json
import random
import time
from datetime import datetime
import signal
import sys

running = True

def signal_handler(sig, frame):
    global running
    print('\nShutting down...')
    running = False
    sys.exit(0)

signal.signal(signal.SIGINT, signal_handler)
SYMBOLS = [
    {"symbol": "AAPL", "base_price": 150.0},
    {"symbol": "GOOGL", "base_price": 2800.0},
    {"symbol": "MSFT", "base_price": 300.0},
    {"symbol": "AMZN", "base_price": 3300.0},
    {"symbol": "TSLA", "base_price": 800.0},
    {"symbol": "JPM", "base_price": 140.0},
    {"symbol": "BAC", "base_price": 30.0},
    {"symbol": "GS", "base_price": 350.0},
    {"symbol": "MS", "base_price": 80.0},
    {"symbol": "C", "base_price": 60.0}
]
current_prices = {s["symbol"]: s["base_price"] for s in SYMBOLS}
open_prices = current_prices.copy()
high_prices = current_prices.copy()
low_prices = current_prices.copy()
volumes = {s["symbol"]: 0 for s in SYMBOLS}
trade_counts = {s["symbol"]: 0 for s in SYMBOLS}

clients = set()

def generate_market_data():
    symbols_data = []
    
    for symbol_info in SYMBOLS:
        symbol = symbol_info["symbol"]
        
        change = random.uniform(-0.01, 0.01)
        current_prices[symbol] *= (1 + change)
        
        high_prices[symbol] = max(high_prices[symbol], current_prices[symbol])
        low_prices[symbol] = min(low_prices[symbol], current_prices[symbol])
        
        volume = random.randint(1000, 100000)
        volumes[symbol] += volume
        trade_counts[symbol] += random.randint(10, 100)
        
        spread = random.uniform(0.01, 0.05)
        bid_price = current_prices[symbol] - spread/2
        ask_price = current_prices[symbol] + spread/2
        
        change_percent = ((current_prices[symbol] - open_prices[symbol]) / open_prices[symbol]) * 100
        
        vwap = current_prices[symbol] * random.uniform(0.98, 1.02)
        
        symbols_data.append({
            "symbol": symbol,
            "bid_price": round(bid_price, 4),
            "ask_price": round(ask_price, 4),
            "last_price": round(current_prices[symbol], 4),
            "volume": volumes[symbol],
            "change_percent": round(change_percent, 2),
            "high_price": round(high_prices[symbol], 4),
            "low_price": round(low_prices[symbol], 4),
            "open_price": round(open_prices[symbol], 4),
            "trade_count": trade_counts[symbol],
            "bid_size": random.randint(1000, 5000),
            "ask_size": random.randint(1000, 5000),
            "spread": round(spread, 4),
            "vwap": round(vwap, 4)
        })
    
    return {
        "type": "market_update",
        "timestamp": int(time.time() * 1000),
        "server_timestamp": int(time.time() * 1000),
        "total_messages": sum(trade_counts.values()),
        "symbols": symbols_data,
        "performance": {
            "messages_per_second": random.randint(1000, 10000),
            "avg_latency_ms": round(random.uniform(0.1, 1.0), 2),
            "memory_usage_mb": random.randint(50, 200)
        }
    }

async def handle_client(websocket, path):
    print(f"Client connected from {websocket.remote_address} (total: {len(clients) + 1})")
    clients.add(websocket)
    
    try:
        welcome = {
            "type": "welcome",
            "message": "Connected to Market Data Feed",
            "timestamp": int(time.time() * 1000),
            "available_symbols": [s["symbol"] for s in SYMBOLS]
        }
        await websocket.send(json.dumps(welcome))
        
        async for message in websocket:
            try:
                data = json.loads(message)
                if data.get("type") == "ping":
                    pong = {
                        "type": "pong",
                        "timestamp": int(time.time() * 1000)
                    }
                    await websocket.send(json.dumps(pong))
                elif data.get("type") == "subscribe":
                    confirm = {
                        "type": "subscription_confirmed",
                        "symbols": data.get("symbols", []),
                        "timestamp": int(time.time() * 1000)
                    }
                    await websocket.send(json.dumps(confirm))
            except json.JSONDecodeError:
                print(f"Invalid JSON from client: {message}")
                
    except websockets.exceptions.ConnectionClosed:
        pass
    finally:
        clients.remove(websocket)
        print(f"Client disconnected (total: {len(clients)})")

async def broadcast_market_data():
    while running:
        if clients:
            market_data = generate_market_data()
            message = json.dumps(market_data)
            
            disconnected = set()
            for client in clients:
                try:
                    await client.send(message)
                except websockets.exceptions.ConnectionClosed:
                    disconnected.add(client)
            
            for client in disconnected:
                clients.remove(client)
        
        await asyncio.sleep(0.05)

async def main():
    print("Market Data WebSocket Server")
    print("Real-time Data Streaming")
    
    server = await websockets.serve(handle_client, "localhost", 9001)
    
    print("WebSocket server started on ws://localhost:9001")
    print("Generating market data...")
    print("WebSocket endpoint: ws://localhost:9001")
    print("Connect your web dashboard to see real-time data")
    print("Press Ctrl+C to stop")
    
    await broadcast_market_data()
    
    await server.wait_closed()

if __name__ == "__main__":
    try:
        asyncio.run(main())
    except KeyboardInterrupt:
        print("Server stopped") 