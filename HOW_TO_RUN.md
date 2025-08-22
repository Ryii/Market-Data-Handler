# 🚀 How to Run the Market Data Engine with Stunning UI

## Overview
This guide will help you run the complete Market Data Engine system with the amazing interactive web dashboard.

## Prerequisites

### For the C++ Engine (Optional)
- C++ compiler with C++20 support
- CMake 3.20+
- Ninja build system

### For the WebSocket Server
- Python 3.7+ (usually pre-installed on macOS)
- websockets library (we'll install this)

### For the Web UI
- Node.js 16+ and npm
- Modern web browser (Chrome, Firefox, Safari, Edge)

## Step-by-Step Instructions

### Option 1: Quick Start (Python WebSocket Server + UI)

This is the easiest way to see the stunning UI in action!

#### 1. Start the WebSocket Server

```bash
# Navigate to the project directory
cd /Users/rysun/Desktop/market-data-engine

# Install Python websockets if not already installed
python3 -m pip install websockets

# Run the WebSocket server
python3 ws_server.py
```

You should see:
```
╔══════════════════════════════════════════╗
║      MARKET DATA WEBSOCKET SERVER        ║
║         Real-time Data Streaming         ║
╚══════════════════════════════════════════╝

✅ WebSocket server started on ws://localhost:9001
📊 Generating market data...
🔗 WebSocket endpoint: ws://localhost:9001
📱 Connect your web dashboard to see real-time data
⌨️  Press Ctrl+C to stop
```

#### 2. Install Node.js (if not installed)

**Option A: Using Homebrew**
```bash
# Install Homebrew if you don't have it
/bin/bash -c "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/HEAD/install.sh)"

# Install Node.js
brew install node
```

**Option B: Download from nodejs.org**
- Visit https://nodejs.org/
- Download and install the LTS version

#### 3. Run the Web UI

Open a new terminal window:

```bash
# Navigate to the web directory
cd /Users/rysun/Desktop/market-data-engine/web

# Install dependencies (first time only)
npm install

# Start the React development server
npm start
```

The UI will automatically open in your browser at `http://localhost:3000`

### Option 2: Full C++ Engine + UI

#### 1. Build the C++ Engine

```bash
cd /Users/rysun/Desktop/market-data-engine
./scripts/build.sh
```

#### 2. Run the Market Data Engine

```bash
./build/feed_handler
```

#### 3. Run the WebSocket Server

In a new terminal:
```bash
python3 ws_server.py
```

#### 4. Run the Web UI

In another new terminal:
```bash
cd web
npm install  # First time only
npm start
```

## 🎨 What You'll See

Once everything is running, you'll experience:

### 1. **3D Market Visualization**
- Rotating 3D cubes with market symbols
- Interactive click to select symbols
- Floating particles and data streams
- Holographic grid overlay

### 2. **Real-time Market Data**
- Live price updates every 50ms (20 FPS)
- Animated price cards with hover effects
- Color-coded gains (green) and losses (red)
- Smooth transitions and animations

### 3. **Advanced Charts**
- TradingView-style price charts
- Multi-series visualization (price, bid, ask)
- Gradient area fills
- Interactive tooltips

### 4. **Order Book Heatmap**
- Visual market depth representation
- Gradient intensity based on order size
- Real-time bid/ask updates

### 5. **Performance Dashboard**
- System metrics (messages/sec, latency, memory)
- Animated gauges and indicators
- Real-time performance graphs

### 6. **Interactive Controls**
- Dark/Light theme toggle
- Play/Pause data streaming
- Sidebar navigation
- Settings panel

## 🛠️ Troubleshooting

### WebSocket Server Issues

**Problem**: `ModuleNotFoundError: No module named 'websockets'`
```bash
python3 -m pip install websockets
```

**Problem**: Port 9001 already in use
```bash
# Find and kill the process using port 9001
lsof -ti:9001 | xargs kill -9
```

### Web UI Issues

**Problem**: `command not found: npm`
- Install Node.js using the instructions above

**Problem**: Port 3000 already in use
```bash
# Kill the process using port 3000
lsof -ti:3000 | xargs kill -9
```

**Problem**: Module not found errors
```bash
cd web
rm -rf node_modules package-lock.json
npm install
```

### Connection Issues

**Problem**: UI shows "Disconnected"
- Make sure the WebSocket server is running (`python3 ws_server.py`)
- Check that it's running on port 9001
- Check browser console for errors (F12)

## 📊 System Architecture

```
┌─────────────────┐     ┌──────────────────┐     ┌─────────────────┐
│   C++ Engine    │────▶│ WebSocket Server │────▶│   React UI      │
│ (Market Data)   │     │  (Python/9001)   │     │ (Port 3000)     │
└─────────────────┘     └──────────────────┘     └─────────────────┘
       Optional               Required                 Required
```

## 🎯 Quick Commands Reference

```bash
# Terminal 1: WebSocket Server (Required)
cd /Users/rysun/Desktop/market-data-engine
python3 ws_server.py

# Terminal 2: Web UI (Required)
cd /Users/rysun/Desktop/market-data-engine/web
npm install  # First time only
npm start

# Terminal 3: C++ Engine (Optional)
cd /Users/rysun/Desktop/market-data-engine
./build/feed_handler
```

## 🌟 Features to Explore

1. **Click on any symbol card** to see detailed price charts
2. **Toggle dark/light mode** with the sun/moon icon
3. **Watch the 3D visualization** - cubes rotate and respond to market data
4. **Monitor performance metrics** in real-time
5. **Pause/resume data flow** to examine specific moments
6. **Check notifications** for significant price movements (>2%)

## 📱 Mobile Support

The UI is fully responsive and works on tablets and phones. Just navigate to `http://[your-computer-ip]:3000` from your mobile device (make sure you're on the same network).

## 🎉 Enjoy!

You now have a professional-grade market data visualization system running! The combination of high-performance C++ backend, real-time WebSocket streaming, and stunning React UI creates an impressive demonstration of modern financial technology. 