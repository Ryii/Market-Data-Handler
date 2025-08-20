import React, { useState, useEffect, useCallback, useMemo } from 'react';
import {
  Box,
  Container,
  Grid,
  Paper,
  Typography,
  AppBar,
  Toolbar,
  Chip,
  Alert,
  Card,
  CardContent,
  Switch,
  FormControlLabel,
  Select,
  MenuItem,
  FormControl,
  InputLabel
} from '@mui/material';
import { TrendingUp, Speed, ShowChart, Assessment } from '@mui/icons-material';
import { LineChart, Line, XAxis, YAxis, CartesianGrid, Tooltip, ResponsiveContainer, 
         BarChart, Bar, ScatterChart, Scatter, Cell } from 'recharts';
import { io, Socket } from 'socket.io-client';
import numeral from 'numeral';
import moment from 'moment';
import './App.css';

// Types
interface MarketData {
  symbol: string;
  best_bid: number;
  best_ask: number;
  mid_price: number;
  spread: number;
  imbalance: number;
  volume: number;
  trade_count: number;
  volatility: number;
  timestamp: number;
}

interface OrderBookLevel {
  price: number;
  quantity: number;
  orders: number;
}

interface OrderBookData {
  symbol: string;
  timestamp: number;
  best_bid: number;
  best_ask: number;
  mid_price: number;
  spread: number;
  weighted_mid: number;
  imbalance: number;
  bids: OrderBookLevel[];
  asks: OrderBookLevel[];
  statistics: {
    last_price: number;
    high: number;
    low: number;
    open: number;
    vwap: number;
    volume: number;
    trade_count: number;
    volatility: number;
  };
}

interface PerformanceMetrics {
  messages_processed: number;
  avg_latency_ns: number;
  max_latency_ns: number;
  queue_depth: number;
  messages_per_second: number;
}

const App: React.FC = () => {
  // State management
  const [marketData, setMarketData] = useState<Record<string, MarketData>>({});
  const [orderBooks, setOrderBooks] = useState<Record<string, OrderBookData>>({});
  const [selectedSymbol, setSelectedSymbol] = useState<string>('AAPL');
  const [isConnected, setIsConnected] = useState(false);
  const [performanceMetrics, setPerformanceMetrics] = useState<PerformanceMetrics | null>(null);
  const [priceHistory, setPriceHistory] = useState<Record<string, Array<{time: number, price: number}>>>({});
  const [darkMode, setDarkMode] = useState(true);
  const [autoRefresh, setAutoRefresh] = useState(true);
  
  // WebSocket connection
  const [socket, setSocket] = useState<Socket | null>(null);
  
  // Connect to WebSocket server
  useEffect(() => {
    const newSocket = io('ws://localhost:9001', {
      transports: ['websocket'],
      upgrade: false
    });
    
    newSocket.on('connect', () => {
      console.log('🔗 Connected to market data server');
      setIsConnected(true);
      
      // Subscribe to all symbols
      newSocket.emit('subscribe', {
        type: 'subscribe',
        symbols: ['AAPL', 'GOOGL', 'MSFT', 'TSLA', 'NVDA', 'JPM', 'BAC', 'GS', 'BTCUSD', 'ETHUSD']
      });
    });
    
    newSocket.on('disconnect', () => {
      console.log('❌ Disconnected from market data server');
      setIsConnected(false);
    });
    
    newSocket.on('market_update', (data: any) => {
      handleMarketUpdate(data);
    });
    
    newSocket.on('welcome', (data: any) => {
      console.log('👋 Welcome message:', data);
    });
    
    setSocket(newSocket);
    
    return () => {
      newSocket.close();
    };
  }, []);
  
  const handleMarketUpdate = useCallback((data: any) => {
    if (!data.symbols) return;
    
    const newMarketData: Record<string, MarketData> = {};
    const newOrderBooks: Record<string, OrderBookData> = {};
    
    data.symbols.forEach((symbolData: any) => {
      const symbol = symbolData.symbol;
      
      // Update market data
      newMarketData[symbol] = {
        symbol,
        best_bid: symbolData.best_bid || 0,
        best_ask: symbolData.best_ask || 0,
        mid_price: symbolData.mid_price || 0,
        spread: symbolData.spread || 0,
        imbalance: symbolData.imbalance || 0,
        volume: symbolData.volume || 0,
        trade_count: symbolData.trade_count || 0,
        volatility: symbolData.volatility || 0,
        timestamp: data.timestamp || Date.now()
      };
      
      // Update price history
      setPriceHistory(prev => {
        const history = prev[symbol] || [];
        const newPoint = {
          time: Date.now(),
          price: symbolData.mid_price || 0
        };
        
        // Keep last 100 points
        const updatedHistory = [...history, newPoint].slice(-100);
        
        return {
          ...prev,
          [symbol]: updatedHistory
        };
      });
    });
    
    setMarketData(prev => ({ ...prev, ...newMarketData }));
    
    // Update performance metrics if available
    if (data.performance) {
      setPerformanceMetrics(data.performance);
    }
  }, []);
  
  // Memoized calculations
  const symbolList = useMemo(() => Object.keys(marketData).sort(), [marketData]);
  const selectedMarketData = useMemo(() => marketData[selectedSymbol], [marketData, selectedSymbol]);
  const selectedPriceHistory = useMemo(() => priceHistory[selectedSymbol] || [], [priceHistory, selectedSymbol]);
  
  // Format utilities
  const formatPrice = (price: number) => numeral(price).format('$0,0.00');
  const formatVolume = (volume: number) => numeral(volume).format('0.0a');
  const formatPercent = (value: number) => numeral(value).format('0.00%');
  const formatLatency = (ns: number) => `${(ns / 1000).toFixed(1)}μs`;
  
  return (
    <Box sx={{ 
      bgcolor: darkMode ? '#0a0e1a' : '#f5f5f5', 
      minHeight: '100vh',
      color: darkMode ? '#ffffff' : '#000000'
    }}>
      {/* Header */}
      <AppBar position="static" sx={{ bgcolor: darkMode ? '#1a1d29' : '#1976d2' }}>
        <Toolbar>
          <TrendingUp sx={{ mr: 2 }} />
          <Typography variant="h6" component="div" sx={{ flexGrow: 1 }}>
            High-Frequency Market Data Engine
          </Typography>
          
          <Chip 
            label={isConnected ? 'CONNECTED' : 'DISCONNECTED'} 
            color={isConnected ? 'success' : 'error'}
            variant="outlined"
            sx={{ mr: 2 }}
          />
          
          <FormControlLabel
            control={
              <Switch 
                checked={darkMode} 
                onChange={(e) => setDarkMode(e.target.checked)}
                color="default"
              />
            }
            label="Dark Mode"
          />
        </Toolbar>
      </AppBar>
      
      <Container maxWidth="xl" sx={{ mt: 2, mb: 2 }}>
        {!isConnected && (
          <Alert severity="warning" sx={{ mb: 2 }}>
            Not connected to market data server. Make sure the WebSocket server is running on port 9001.
          </Alert>
        )}
        
        {/* Controls */}
        <Paper sx={{ p: 2, mb: 2, bgcolor: darkMode ? '#1a1d29' : '#ffffff' }}>
          <Grid container spacing={2} alignItems="center">
            <Grid item xs={12} sm={6} md={3}>
              <FormControl fullWidth>
                <InputLabel>Symbol</InputLabel>
                <Select
                  value={selectedSymbol}
                  onChange={(e) => setSelectedSymbol(e.target.value)}
                  label="Symbol"
                >
                  {symbolList.map(symbol => (
                    <MenuItem key={symbol} value={symbol}>{symbol}</MenuItem>
                  ))}
                </Select>
              </FormControl>
            </Grid>
            
            <Grid item xs={12} sm={6} md={3}>
              <FormControlLabel
                control={
                  <Switch 
                    checked={autoRefresh} 
                    onChange={(e) => setAutoRefresh(e.target.checked)}
                  />
                }
                label="Auto Refresh"
              />
            </Grid>
            
            <Grid item xs={12} md={6}>
              <Typography variant="body2" color="textSecondary">
                Last Update: {moment().format('HH:mm:ss.SSS')} | 
                Active Symbols: {symbolList.length} | 
                Updates/sec: {performanceMetrics?.messages_per_second || 0}
              </Typography>
            </Grid>
          </Grid>
        </Paper>
        
        {/* Main Dashboard */}
        <Grid container spacing={2}>
          {/* Market Overview */}
          <Grid item xs={12} lg={8}>
            <Paper sx={{ p: 2, bgcolor: darkMode ? '#1a1d29' : '#ffffff' }}>
              <Typography variant="h6" gutterBottom>
                <ShowChart sx={{ mr: 1, verticalAlign: 'middle' }} />
                Real-Time Price Chart - {selectedSymbol}
              </Typography>
              
              {selectedPriceHistory.length > 0 ? (
                <ResponsiveContainer width="100%" height={300}>
                  <LineChart data={selectedPriceHistory}>
                    <CartesianGrid strokeDasharray="3 3" stroke={darkMode ? '#333' : '#ccc'} />
                    <XAxis 
                      dataKey="time"
                      type="number"
                      scale="time"
                      domain={['dataMin', 'dataMax']}
                      tickFormatter={(time) => moment(time).format('HH:mm:ss')}
                      stroke={darkMode ? '#888' : '#666'}
                    />
                    <YAxis 
                      domain={['dataMin - 0.1', 'dataMax + 0.1']}
                      tickFormatter={(value) => formatPrice(value)}
                      stroke={darkMode ? '#888' : '#666'}
                    />
                    <Tooltip 
                      labelFormatter={(time) => moment(time).format('HH:mm:ss.SSS')}
                      formatter={(value: number) => [formatPrice(value), 'Price']}
                      contentStyle={{
                        backgroundColor: darkMode ? '#2a2d3a' : '#ffffff',
                        border: `1px solid ${darkMode ? '#444' : '#ccc'}`,
                        color: darkMode ? '#ffffff' : '#000000'
                      }}
                    />
                    <Line 
                      type="monotone" 
                      dataKey="price" 
                      stroke="#00ff88" 
                      strokeWidth={2}
                      dot={false}
                      isAnimationActive={false}
                    />
                  </LineChart>
                </ResponsiveContainer>
              ) : (
                <Box sx={{ height: 300, display: 'flex', alignItems: 'center', justifyContent: 'center' }}>
                  <Typography color="textSecondary">Waiting for market data...</Typography>
                </Box>
              )}
            </Paper>
          </Grid>
          
          {/* Market Statistics */}
          <Grid item xs={12} lg={4}>
            <Paper sx={{ p: 2, bgcolor: darkMode ? '#1a1d29' : '#ffffff', height: '100%' }}>
              <Typography variant="h6" gutterBottom>
                <Assessment sx={{ mr: 1, verticalAlign: 'middle' }} />
                Market Statistics
              </Typography>
              
              {selectedMarketData ? (
                <Grid container spacing={2}>
                  <Grid item xs={6}>
                    <Card sx={{ bgcolor: darkMode ? '#2a2d3a' : '#f8f9fa' }}>
                      <CardContent sx={{ pb: '16px !important' }}>
                        <Typography color="textSecondary" gutterBottom variant="body2">
                          Best Bid
                        </Typography>
                        <Typography variant="h6" color="#00ff88">
                          {formatPrice(selectedMarketData.best_bid)}
                        </Typography>
                      </CardContent>
                    </Card>
                  </Grid>
                  
                  <Grid item xs={6}>
                    <Card sx={{ bgcolor: darkMode ? '#2a2d3a' : '#f8f9fa' }}>
                      <CardContent sx={{ pb: '16px !important' }}>
                        <Typography color="textSecondary" gutterBottom variant="body2">
                          Best Ask
                        </Typography>
                        <Typography variant="h6" color="#ff4757">
                          {formatPrice(selectedMarketData.best_ask)}
                        </Typography>
                      </CardContent>
                    </Card>
                  </Grid>
                  
                  <Grid item xs={6}>
                    <Card sx={{ bgcolor: darkMode ? '#2a2d3a' : '#f8f9fa' }}>
                      <CardContent sx={{ pb: '16px !important' }}>
                        <Typography color="textSecondary" gutterBottom variant="body2">
                          Spread
                        </Typography>
                        <Typography variant="h6">
                          {formatPrice(selectedMarketData.spread)}
                        </Typography>
                      </CardContent>
                    </Card>
                  </Grid>
                  
                  <Grid item xs={6}>
                    <Card sx={{ bgcolor: darkMode ? '#2a2d3a' : '#f8f9fa' }}>
                      <CardContent sx={{ pb: '16px !important' }}>
                        <Typography color="textSecondary" gutterBottom variant="body2">
                          Volume
                        </Typography>
                        <Typography variant="h6">
                          {formatVolume(selectedMarketData.volume)}
                        </Typography>
                      </CardContent>
                    </Card>
                  </Grid>
                  
                  <Grid item xs={12}>
                    <Card sx={{ bgcolor: darkMode ? '#2a2d3a' : '#f8f9fa' }}>
                      <CardContent>
                        <Typography color="textSecondary" gutterBottom variant="body2">
                          Order Book Imbalance
                        </Typography>
                        <Box sx={{ display: 'flex', alignItems: 'center', gap: 1 }}>
                          <Box 
                            sx={{ 
                              width: '100%', 
                              height: 8, 
                              bgcolor: darkMode ? '#333' : '#e0e0e0',
                              borderRadius: 1,
                              position: 'relative'
                            }}
                          >
                            <Box
                              sx={{
                                width: `${Math.abs(selectedMarketData.imbalance) * 100}%`,
                                height: '100%',
                                bgcolor: selectedMarketData.imbalance > 0 ? '#00ff88' : '#ff4757',
                                borderRadius: 1,
                                ml: selectedMarketData.imbalance < 0 ? `${(1 + selectedMarketData.imbalance) * 100}%` : 0
                              }}
                            />
                          </Box>
                          <Typography variant="body2" sx={{ minWidth: 60 }}>
                            {formatPercent(selectedMarketData.imbalance)}
                          </Typography>
                        </Box>
                      </CardContent>
                    </Card>
                  </Grid>
                  
                  <Grid item xs={12}>
                    <Card sx={{ bgcolor: darkMode ? '#2a2d3a' : '#f8f9fa' }}>
                      <CardContent>
                        <Typography color="textSecondary" gutterBottom variant="body2">
                          Volatility (Annualized)
                        </Typography>
                        <Typography variant="h6" color={selectedMarketData.volatility > 0.3 ? '#ff4757' : '#00ff88'}>
                          {formatPercent(selectedMarketData.volatility)}
                        </Typography>
                      </CardContent>
                    </Card>
                  </Grid>
                </Grid>
              ) : (
                <Typography color="textSecondary">Select a symbol to view statistics</Typography>
              )}
            </Paper>
          </Grid>
          
          {/* Order Book Visualization */}
          <Grid item xs={12} md={6}>
            <Paper sx={{ p: 2, bgcolor: darkMode ? '#1a1d29' : '#ffffff' }}>
              <Typography variant="h6" gutterBottom>
                Order Book Depth - {selectedSymbol}
              </Typography>
              
              {orderBooks[selectedSymbol] ? (
                <ResponsiveContainer width="100%" height={300}>
                  <BarChart
                    data={[
                      ...orderBooks[selectedSymbol].bids.slice(0, 10).reverse().map(level => ({
                        price: level.price,
                        quantity: -level.quantity,  // Negative for bids
                        side: 'bid'
                      })),
                      ...orderBooks[selectedSymbol].asks.slice(0, 10).map(level => ({
                        price: level.price,
                        quantity: level.quantity,
                        side: 'ask'
                      }))
                    ]}
                    margin={{ top: 20, right: 30, left: 20, bottom: 5 }}
                  >
                    <CartesianGrid strokeDasharray="3 3" stroke={darkMode ? '#333' : '#ccc'} />
                    <XAxis 
                      dataKey="price"
                      tickFormatter={(value) => formatPrice(value)}
                      stroke={darkMode ? '#888' : '#666'}
                    />
                    <YAxis 
                      tickFormatter={(value) => formatVolume(Math.abs(value))}
                      stroke={darkMode ? '#888' : '#666'}
                    />
                    <Tooltip 
                      formatter={(value: number, name: string, props: any) => [
                        formatVolume(Math.abs(value)), 
                        props.payload.side === 'bid' ? 'Bid Size' : 'Ask Size'
                      ]}
                      labelFormatter={(price) => `Price: ${formatPrice(price)}`}
                      contentStyle={{
                        backgroundColor: darkMode ? '#2a2d3a' : '#ffffff',
                        border: `1px solid ${darkMode ? '#444' : '#ccc'}`,
                        color: darkMode ? '#ffffff' : '#000000'
                      }}
                    />
                    <Bar dataKey="quantity">
                      {orderBooks[selectedSymbol] && [
                        ...orderBooks[selectedSymbol].bids.slice(0, 10).reverse(),
                        ...orderBooks[selectedSymbol].asks.slice(0, 10)
                      ].map((entry, index) => (
                        <Cell 
                          key={`cell-${index}`} 
                          fill={index < 10 ? '#00ff88' : '#ff4757'} 
                        />
                      ))}
                    </Bar>
                  </BarChart>
                </ResponsiveContainer>
              ) : (
                <Box sx={{ height: 300, display: 'flex', alignItems: 'center', justifyContent: 'center' }}>
                  <Typography color="textSecondary">Loading order book data...</Typography>
                </Box>
              )}
            </Paper>
          </Grid>
          
          {/* Performance Metrics */}
          <Grid item xs={12} md={6}>
            <Paper sx={{ p: 2, bgcolor: darkMode ? '#1a1d29' : '#ffffff' }}>
              <Typography variant="h6" gutterBottom>
                <Speed sx={{ mr: 1, verticalAlign: 'middle' }} />
                System Performance
              </Typography>
              
              {performanceMetrics ? (
                <Grid container spacing={2}>
                  <Grid item xs={6}>
                    <Card sx={{ bgcolor: darkMode ? '#2a2d3a' : '#f8f9fa' }}>
                      <CardContent>
                        <Typography color="textSecondary" gutterBottom variant="body2">
                          Messages/sec
                        </Typography>
                        <Typography variant="h6">
                          {numeral(performanceMetrics.messages_per_second).format('0,0')}
                        </Typography>
                      </CardContent>
                    </Card>
                  </Grid>
                  
                  <Grid item xs={6}>
                    <Card sx={{ bgcolor: darkMode ? '#2a2d3a' : '#f8f9fa' }}>
                      <CardContent>
                        <Typography color="textSecondary" gutterBottom variant="body2">
                          Avg Latency
                        </Typography>
                        <Typography variant="h6" color="#00ff88">
                          {formatLatency(performanceMetrics.avg_latency_ns)}
                        </Typography>
                      </CardContent>
                    </Card>
                  </Grid>
                  
                  <Grid item xs={6}>
                    <Card sx={{ bgcolor: darkMode ? '#2a2d3a' : '#f8f9fa' }}>
                      <CardContent>
                        <Typography color="textSecondary" gutterBottom variant="body2">
                          Max Latency
                        </Typography>
                        <Typography variant="h6" color="#ff4757">
                          {formatLatency(performanceMetrics.max_latency_ns)}
                        </Typography>
                      </CardContent>
                    </Card>
                  </Grid>
                  
                  <Grid item xs={6}>
                    <Card sx={{ bgcolor: darkMode ? '#2a2d3a' : '#f8f9fa' }}>
                      <CardContent>
                        <Typography color="textSecondary" gutterBottom variant="body2">
                          Queue Depth
                        </Typography>
                        <Typography variant="h6">
                          {numeral(performanceMetrics.queue_depth).format('0,0')}
                        </Typography>
                      </CardContent>
                    </Card>
                  </Grid>
                </Grid>
              ) : (
                <Typography color="textSecondary">Performance metrics loading...</Typography>
              )}
            </Paper>
          </Grid>
          
          {/* Market Heatmap */}
          <Grid item xs={12}>
            <Paper sx={{ p: 2, bgcolor: darkMode ? '#1a1d29' : '#ffffff' }}>
              <Typography variant="h6" gutterBottom>
                Market Heatmap - Price Changes & Volume
              </Typography>
              
              <ResponsiveContainer width="100%" height={200}>
                <ScatterChart>
                  <CartesianGrid strokeDasharray="3 3" stroke={darkMode ? '#333' : '#ccc'} />
                  <XAxis 
                    type="number"
                    dataKey="volatility"
                    name="Volatility"
                    tickFormatter={(value) => formatPercent(value)}
                    stroke={darkMode ? '#888' : '#666'}
                  />
                  <YAxis 
                    type="number"
                    dataKey="volume"
                    name="Volume"
                    tickFormatter={(value) => formatVolume(value)}
                    stroke={darkMode ? '#888' : '#666'}
                  />
                  <Tooltip 
                    cursor={{ strokeDasharray: '3 3' }}
                    formatter={(value, name) => [
                      name === 'volatility' ? formatPercent(value as number) : formatVolume(value as number),
                      name === 'volatility' ? 'Volatility' : 'Volume'
                    ]}
                    contentStyle={{
                      backgroundColor: darkMode ? '#2a2d3a' : '#ffffff',
                      border: `1px solid ${darkMode ? '#444' : '#ccc'}`,
                      color: darkMode ? '#ffffff' : '#000000'
                    }}
                  />
                  <Scatter 
                    name="Symbols"
                    data={Object.values(marketData)}
                    fill="#8884d8"
                  />
                </ScatterChart>
              </ResponsiveContainer>
            </Paper>
          </Grid>
          
          {/* Symbol Grid */}
          <Grid item xs={12}>
            <Paper sx={{ p: 2, bgcolor: darkMode ? '#1a1d29' : '#ffffff' }}>
              <Typography variant="h6" gutterBottom>
                All Symbols - Live Market Data
              </Typography>
              
              <Grid container spacing={1}>
                {Object.values(marketData).map((data) => (
                  <Grid item xs={12} sm={6} md={4} lg={3} xl={2.4} key={data.symbol}>
                    <Card 
                      sx={{ 
                        bgcolor: darkMode ? '#2a2d3a' : '#f8f9fa',
                        cursor: 'pointer',
                        border: selectedSymbol === data.symbol ? '2px solid #00ff88' : 'none',
                        '&:hover': {
                          bgcolor: darkMode ? '#3a3d4a' : '#e8f4fd'
                        }
                      }}
                      onClick={() => setSelectedSymbol(data.symbol)}
                    >
                      <CardContent sx={{ pb: '16px !important' }}>
                        <Typography variant="h6" gutterBottom>
                          {data.symbol}
                        </Typography>
                        <Typography variant="body2" color="textSecondary">
                          Mid: {formatPrice(data.mid_price)}
                        </Typography>
                        <Typography variant="body2" color="textSecondary">
                          Vol: {formatVolume(data.volume)}
                        </Typography>
                        <Typography variant="body2" color="textSecondary">
                          Spread: {formatPrice(data.spread)}
                        </Typography>
                        <Box sx={{ mt: 1 }}>
                          <Chip 
                            label={`${formatPercent(data.volatility)} vol`}
                            size="small"
                            color={data.volatility > 0.3 ? 'error' : 'success'}
                            variant="outlined"
                          />
                        </Box>
                      </CardContent>
                    </Card>
                  </Grid>
                ))}
              </Grid>
            </Paper>
          </Grid>
        </Grid>
      </Container>
    </Box>
  );
};

export default App; 