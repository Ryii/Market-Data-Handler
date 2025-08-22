# 🚀 Market Data Engine - Interactive Web Dashboard

A stunning, real-time market data visualization dashboard with 3D effects, advanced charting, and beautiful animations.

## ✨ Features

### 🎨 Visual Excellence
- **3D Market Visualization**: Interactive 3D cubes displaying market data with rotation and hover effects
- **Real-time WebSocket Connection**: Live data streaming with automatic reconnection
- **Dark/Light Theme**: Smooth theme transitions with custom Material-UI theming
- **Animated Components**: Pulse effects, sliding animations, and smooth transitions throughout

### 📊 Data Visualizations
- **Advanced Price Charts**: Multi-series area charts with bid/ask spread visualization
- **Order Book Heatmap**: Visual depth representation with gradient intensity
- **Volume Analysis**: Color-coded bar charts for top trading volumes
- **Performance Metrics**: Real-time system performance monitoring with dual-axis charts

### 🎯 Interactive Features
- **Symbol Selection**: Click any symbol card to view detailed charts
- **Live Notifications**: Alert system for significant market movements (>2% changes)
- **Pause/Resume**: Control data flow with play/pause functionality
- **Auto-scroll Settings**: Configurable automatic scrolling for new data

### 📱 Responsive Design
- Mobile-optimized layout with responsive grids
- Touch-friendly interactions
- Adaptive component sizing

## 🛠️ Technology Stack

- **React 18** with TypeScript
- **Material-UI v5** for component library
- **Emotion** for styled components and animations
- **Recharts** for data visualization
- **Socket.io Client** for WebSocket connections
- **Moment.js** for time formatting
- **Numeral.js** for number formatting

## 🚀 Getting Started

### Prerequisites
- Node.js 16+ and npm/yarn
- The Market Data Engine WebSocket server running on `ws://localhost:9001`

### Installation

1. Navigate to the web directory:
```bash
cd market-data-engine/web
```

2. Install dependencies:
```bash
npm install
```

3. Start the development server:
```bash
npm start
```

The app will open at `http://localhost:3000`

## 🎨 UI Components

### 1. **3D Market Visualization**
- Rotating cubes with market symbols
- Color-coded based on price movements (green/red)
- Interactive selection on click
- Floating particles and data streams
- Holographic grid overlay

### 2. **Symbol Cards**
- Animated entrance effects
- Real-time price updates
- Visual spread indicators
- High/Low price range bars
- Hover lift effects

### 3. **Price Charts**
- Gradient area fills
- Multiple data series (price, bid, ask)
- Responsive tooltips
- Smooth animations

### 4. **Order Book Heatmap**
- Gradient intensity based on order size
- Bid/Ask separation
- Cumulative volume display
- Hover highlighting

### 5. **Performance Dashboard**
- Messages per second
- Average latency tracking
- Memory usage monitoring
- Real-time line charts

## 🎮 Controls

- **Menu Icon**: Toggle sidebar navigation
- **Dark/Light Mode**: Theme switcher in the top bar
- **Play/Pause**: Control real-time data flow
- **Refresh**: Reload the application
- **Connection Status**: Visual indicator with pulse animation

## 🌟 Special Effects

1. **Glowing Badges**: Connection status with animated glow
2. **Price Flash**: Background flash on price updates
3. **Sliding Animations**: Card hover effects with gradient overlays
4. **Pulse Indicators**: Live data indicators with ring animations
5. **Skeleton Loading**: Shimmer effects during data loading

## 📈 Market Data Format

The dashboard expects market data in the following format:

```typescript
interface MarketSymbol {
  symbol: string;
  bid_price: number;
  ask_price: number;
  last_price: number;
  volume: number;
  change_percent: number;
  high_price: number;
  low_price: number;
  spread: number;
}
```

## 🔧 Customization

### Theme Colors
Edit the theme configuration in `App.tsx`:
```typescript
const theme = createTheme({
  palette: {
    mode: darkMode ? 'dark' : 'light',
    primary: { main: darkMode ? '#90caf9' : '#1976d2' },
    // ... customize colors
  }
});
```

### Animation Speed
Modify animation durations in styled components:
```typescript
animation: ${rotate3D} 20s linear infinite;
```

### Chart Configuration
Customize chart appearance in the render functions:
```typescript
<Line type="monotone" dataKey="price" stroke="#4caf50" />
```

## 🚨 Troubleshooting

1. **Connection Issues**: Ensure the WebSocket server is running on port 9001
2. **Performance**: The 3D visualization may impact performance on older devices
3. **Module Errors**: Run `npm install` to ensure all dependencies are installed

## 📝 License

Part of the Market Data Engine project. 