import React, { useState, useEffect, useRef, useCallback } from 'react';
import {
  ThemeProvider,
  createTheme,
  CssBaseline,
  Box,
  Paper,
  Typography,
  Grid,
  Card,
  CardContent,
  IconButton,
  Chip,
  LinearProgress,
  Fade,
  Zoom,
  Collapse,
  Alert,
  Tooltip,
  Badge,
  Drawer,
  List,
  ListItem,
  ListItemIcon,
  ListItemText,
  ListItemButton,
  AppBar,
  Toolbar,
  Switch,
  FormControlLabel,
  Divider,
  Button,
  CircularProgress,
  Skeleton
} from '@mui/material';
import {
  ShowChart,
  TrendingUp,
  TrendingDown,
  Speed,
  Brightness4,
  Brightness7,
  Dashboard,
  Analytics,
  Timeline,
  BookmarkBorder,
  Notifications,
  Settings,
  Menu as MenuIcon,
  Close,
  PlayArrow,
  Pause,
  Refresh,
  NetworkCheck,
  CandlestickChart,
  WaterDrop,
  LocalFireDepartment,
  AttachMoney,
  ArrowDropUp,
  ArrowDropDown,
  FlashOn,
  SignalCellularAlt,
  Circle
} from '@mui/icons-material';
import {
  LineChart,
  Line,
  AreaChart,
  Area,
  BarChart,
  Bar,
  XAxis,
  YAxis,
  CartesianGrid,
  Tooltip as RechartsTooltip,
  ResponsiveContainer,
  Cell,
  PieChart,
  Pie,
  RadarChart,
  Radar,
  PolarGrid,
  PolarAngleAxis,
  PolarRadiusAxis,
  ComposedChart,
  Scatter
} from 'recharts';
import { keyframes } from '@emotion/react';
import styled from '@emotion/styled';
import io, { Socket } from 'socket.io-client';
import moment from 'moment';
import numeral from 'numeral';
import './App.css';
import MarketData3D from './MarketData3D';

// Define interfaces
interface MarketSymbol {
  symbol: string;
  bid_price: number;
  ask_price: number;
  last_price: number;
  volume: number;
  change_percent: number;
  high_price: number;
  low_price: number;
  open_price: number;
  trade_count: number;
  bid_size: number;
  ask_size: number;
  spread: number;
  vwap: number;
}

interface MarketUpdate {
  type: string;
  timestamp: number;
  server_timestamp: number;
  total_messages: number;
  symbols: MarketSymbol[];
  performance: {
    messages_per_second: number;
    avg_latency_ms: number;
    memory_usage_mb: number;
  };
}

interface OrderBookLevel {
  price: number;
  size: number;
  total: number;
}

// Animation keyframes
const pulse = keyframes`
  0% { opacity: 1; transform: scale(1); }
  50% { opacity: 0.7; transform: scale(1.05); }
  100% { opacity: 1; transform: scale(1); }
`;

const glow = keyframes`
  0% { box-shadow: 0 0 5px rgba(0, 255, 0, 0.5); }
  50% { box-shadow: 0 0 20px rgba(0, 255, 0, 0.8), 0 0 30px rgba(0, 255, 0, 0.6); }
  100% { box-shadow: 0 0 5px rgba(0, 255, 0, 0.5); }
`;

const slideIn = keyframes`
  from { transform: translateX(-100%); opacity: 0; }
  to { transform: translateX(0); opacity: 1; }
`;

// Styled components
const AnimatedCard = styled(Card)<{ isPositive?: boolean }>`
  transition: all 0.3s ease;
  border: 1px solid ${props => props.isPositive ? '#4caf50' : '#f44336'};
  position: relative;
  overflow: hidden;
  
  &:hover {
    transform: translateY(-4px);
    box-shadow: 0 8px 24px rgba(0, 0, 0, 0.15);
  }
  
  &::before {
    content: '';
    position: absolute;
    top: 0;
    left: -100%;
    width: 100%;
    height: 100%;
    background: linear-gradient(90deg, transparent, 
      ${props => props.isPositive ? 'rgba(76, 175, 80, 0.2)' : 'rgba(244, 67, 54, 0.2)'}, 
      transparent);
    animation: ${slideIn} 3s infinite;
  }
`;

const PulsingDot = styled(Box)<{ color?: string }>`
  width: 8px;
  height: 8px;
  border-radius: 50%;
  background-color: ${props => props.color || '#4caf50'};
  animation: ${pulse} 2s infinite;
`;

const GlowingBadge = styled(Badge)`
  & .MuiBadge-badge {
    animation: ${glow} 2s infinite;
  }
`;

const StyledPaper = styled(Paper)`
  backdrop-filter: blur(10px);
  background-color: rgba(255, 255, 255, 0.9);
  border-radius: 16px;
  overflow: hidden;
`;

const App: React.FC = () => {
  // State management
  const [darkMode, setDarkMode] = useState(true);
  const [drawerOpen, setDrawerOpen] = useState(false);
  const [connected, setConnected] = useState(false);
  const [marketData, setMarketData] = useState<MarketUpdate | null>(null);
  const [selectedSymbol, setSelectedSymbol] = useState<string>('');
  const [priceHistory, setPriceHistory] = useState<{ [key: string]: any[] }>({});
  const [volumeData, setVolumeData] = useState<any[]>([]);
  const [performanceData, setPerformanceData] = useState<any[]>([]);
  const [isPaused, setIsPaused] = useState(false);
  const [showSettings, setShowSettings] = useState(false);
  const [autoScroll, setAutoScroll] = useState(true);
  const [notifications, setNotifications] = useState<string[]>([]);
  
  const socketRef = useRef<Socket | null>(null);
  const priceHistoryRef = useRef<{ [key: string]: any[] }>({});
  
  // Theme configuration
  const theme = createTheme({
    palette: {
      mode: darkMode ? 'dark' : 'light',
      primary: {
        main: darkMode ? '#90caf9' : '#1976d2',
      },
      secondary: {
        main: darkMode ? '#f48fb1' : '#dc004e',
      },
      success: {
        main: '#4caf50',
      },
      error: {
        main: '#f44336',
      },
      background: {
        default: darkMode ? '#0a0e27' : '#f5f5f5',
        paper: darkMode ? '#1a1f3a' : '#ffffff',
      },
    },
    typography: {
      fontFamily: '"Inter", "Roboto", "Helvetica", "Arial", sans-serif',
      h4: {
        fontWeight: 700,
      },
      h6: {
        fontWeight: 600,
      },
    },
    shape: {
      borderRadius: 12,
    },
    components: {
      MuiCard: {
        styleOverrides: {
          root: {
            backgroundImage: darkMode 
              ? 'linear-gradient(rgba(255, 255, 255, 0.05), rgba(255, 255, 255, 0.05))'
              : 'none',
          },
        },
      },
    },
  });

  // WebSocket connection
  useEffect(() => {
    const connectWebSocket = () => {
      socketRef.current = io('ws://localhost:9001', {
        transports: ['websocket'],
        reconnection: true,
        reconnectionAttempts: 5,
        reconnectionDelay: 1000,
      });

      socketRef.current.on('connect', () => {
        console.log('Connected to WebSocket server');
        setConnected(true);
        addNotification('Connected to market data feed');
        
        // Subscribe to all symbols
        socketRef.current?.emit('message', JSON.stringify({
          type: 'subscribe',
          symbols: []
        }));
      });

      socketRef.current.on('disconnect', () => {
        console.log('Disconnected from WebSocket server');
        setConnected(false);
        addNotification('Disconnected from market data feed');
      });

      socketRef.current.on('message', (data: string) => {
        if (isPaused) return;
        
        try {
          const update: MarketUpdate = JSON.parse(data);
          if (update.type === 'market_update') {
            setMarketData(update);
            updatePriceHistory(update);
            updateVolumeData(update);
            updatePerformanceData(update);
            
            // Check for significant price movements
            update.symbols.forEach(symbol => {
              if (Math.abs(symbol.change_percent) > 2) {
                addNotification(`${symbol.symbol}: ${symbol.change_percent > 0 ? '📈' : '📉'} ${Math.abs(symbol.change_percent).toFixed(2)}% move!`);
              }
            });
          }
        } catch (error) {
          console.error('Error parsing market data:', error);
        }
      });
    };

    connectWebSocket();

    return () => {
      socketRef.current?.disconnect();
    };
  }, [isPaused]);

  // Helper functions
  const addNotification = (message: string) => {
    setNotifications(prev => [message, ...prev.slice(0, 4)]);
  };

  const updatePriceHistory = (update: MarketUpdate) => {
    const timestamp = moment(update.timestamp).format('HH:mm:ss');
    
    update.symbols.forEach(symbol => {
      const key = symbol.symbol;
      if (!priceHistoryRef.current[key]) {
        priceHistoryRef.current[key] = [];
      }
      
      priceHistoryRef.current[key].push({
        time: timestamp,
        price: symbol.last_price,
        bid: symbol.bid_price,
        ask: symbol.ask_price,
        volume: symbol.volume,
        spread: symbol.spread
      });
      
      // Keep last 50 data points
      if (priceHistoryRef.current[key].length > 50) {
        priceHistoryRef.current[key].shift();
      }
    });
    
    setPriceHistory({ ...priceHistoryRef.current });
  };

  const updateVolumeData = (update: MarketUpdate) => {
    const data = update.symbols.map(s => ({
      symbol: s.symbol,
      volume: s.volume,
      trades: s.trade_count
    })).sort((a, b) => b.volume - a.volume).slice(0, 5);
    
    setVolumeData(data);
  };

  const updatePerformanceData = (update: MarketUpdate) => {
    setPerformanceData(prev => {
      const newData = [...prev, {
        time: moment(update.timestamp).format('HH:mm:ss'),
        mps: update.performance.messages_per_second,
        latency: update.performance.avg_latency_ms,
        memory: update.performance.memory_usage_mb
      }];
      
      return newData.slice(-20);
    });
  };

  const formatPrice = (price: number) => numeral(price).format('0,0.0000');
  const formatVolume = (volume: number) => numeral(volume).format('0.0a').toUpperCase();
  const formatPercent = (percent: number) => {
    const formatted = numeral(percent).format('0.00');
    return percent >= 0 ? `+${formatted}%` : `${formatted}%`;
  };

  const getSymbolData = (symbol: string) => {
    return marketData?.symbols.find(s => s.symbol === symbol);
  };

  // Render helpers
  const renderSymbolCard = (symbol: MarketSymbol) => {
    const isPositive = symbol.change_percent >= 0;
    const Icon = isPositive ? TrendingUp : TrendingDown;
    
    return (
      <AnimatedCard 
        key={symbol.symbol}
        isPositive={isPositive}
        sx={{ height: '100%', cursor: 'pointer' }}
        onClick={() => setSelectedSymbol(symbol.symbol)}
      >
        <CardContent>
          <Box display="flex" justifyContent="space-between" alignItems="center" mb={2}>
            <Typography variant="h6" component="div" fontWeight="bold">
              {symbol.symbol}
            </Typography>
            <Chip 
              icon={<Icon />}
              label={formatPercent(symbol.change_percent)}
              color={isPositive ? 'success' : 'error'}
              size="small"
            />
          </Box>
          
          <Typography variant="h4" component="div" mb={1}>
            ${formatPrice(symbol.last_price)}
          </Typography>
          
          <Grid container spacing={1}>
            <Grid item xs={6}>
              <Typography variant="caption" color="text.secondary">Bid</Typography>
              <Typography variant="body2">${formatPrice(symbol.bid_price)}</Typography>
            </Grid>
            <Grid item xs={6}>
              <Typography variant="caption" color="text.secondary">Ask</Typography>
              <Typography variant="body2">${formatPrice(symbol.ask_price)}</Typography>
            </Grid>
            <Grid item xs={6}>
              <Typography variant="caption" color="text.secondary">Volume</Typography>
              <Typography variant="body2">{formatVolume(symbol.volume)}</Typography>
            </Grid>
            <Grid item xs={6}>
              <Typography variant="caption" color="text.secondary">Spread</Typography>
              <Typography variant="body2">{symbol.spread.toFixed(4)}</Typography>
            </Grid>
          </Grid>
          
          <Box mt={2}>
            <LinearProgress 
              variant="determinate" 
              value={(symbol.last_price - symbol.low_price) / (symbol.high_price - symbol.low_price) * 100}
              sx={{ height: 6, borderRadius: 3 }}
            />
            <Box display="flex" justifyContent="space-between" mt={0.5}>
              <Typography variant="caption">${formatPrice(symbol.low_price)}</Typography>
              <Typography variant="caption">${formatPrice(symbol.high_price)}</Typography>
            </Box>
          </Box>
        </CardContent>
      </AnimatedCard>
    );
  };

  const renderPriceChart = () => {
    if (!selectedSymbol || !priceHistory[selectedSymbol]) {
      return (
        <Box p={4} textAlign="center">
          <Typography color="text.secondary">Select a symbol to view price chart</Typography>
        </Box>
      );
    }
    
    const data = priceHistory[selectedSymbol];
    const symbolData = getSymbolData(selectedSymbol);
    const isPositive = symbolData?.change_percent ?? 0 >= 0;
    
    return (
      <Box>
        <Box display="flex" justifyContent="space-between" alignItems="center" mb={2}>
          <Typography variant="h6">{selectedSymbol} Price Chart</Typography>
          <Box display="flex" gap={2} alignItems="center">
            <Chip 
              icon={isPositive ? <TrendingUp /> : <TrendingDown />}
              label={formatPercent(symbolData?.change_percent ?? 0)}
              color={isPositive ? 'success' : 'error'}
            />
            <Typography variant="h5" fontWeight="bold">
              ${formatPrice(symbolData?.last_price ?? 0)}
            </Typography>
          </Box>
        </Box>
        
        <ResponsiveContainer width="100%" height={300}>
          <ComposedChart data={data}>
            <defs>
              <linearGradient id="colorPrice" x1="0" y1="0" x2="0" y2="1">
                <stop offset="5%" stopColor={isPositive ? '#4caf50' : '#f44336'} stopOpacity={0.8}/>
                <stop offset="95%" stopColor={isPositive ? '#4caf50' : '#f44336'} stopOpacity={0}/>
              </linearGradient>
            </defs>
            <XAxis dataKey="time" />
            <YAxis domain={['dataMin - 0.01', 'dataMax + 0.01']} />
            <CartesianGrid strokeDasharray="3 3" opacity={0.3} />
            <RechartsTooltip 
              contentStyle={{ 
                backgroundColor: theme.palette.background.paper,
                border: `1px solid ${theme.palette.divider}`,
                borderRadius: 8
              }}
            />
            <Area 
              type="monotone" 
              dataKey="price" 
              stroke={isPositive ? '#4caf50' : '#f44336'}
              fillOpacity={1} 
              fill="url(#colorPrice)" 
              strokeWidth={2}
            />
            <Line 
              type="monotone" 
              dataKey="bid" 
              stroke="#2196f3" 
              strokeWidth={1}
              strokeDasharray="3 3"
              dot={false}
            />
            <Line 
              type="monotone" 
              dataKey="ask" 
              stroke="#ff9800" 
              strokeWidth={1}
              strokeDasharray="3 3"
              dot={false}
            />
          </ComposedChart>
        </ResponsiveContainer>
        
        <Box display="flex" justifyContent="center" gap={2} mt={2}>
          <Box display="flex" alignItems="center" gap={0.5}>
            <Box width={20} height={2} bgcolor={isPositive ? '#4caf50' : '#f44336'} />
            <Typography variant="caption">Price</Typography>
          </Box>
          <Box display="flex" alignItems="center" gap={0.5}>
            <Box width={20} height={2} bgcolor="#2196f3" sx={{ border: '1px dashed' }} />
            <Typography variant="caption">Bid</Typography>
          </Box>
          <Box display="flex" alignItems="center" gap={0.5}>
            <Box width={20} height={2} bgcolor="#ff9800" sx={{ border: '1px dashed' }} />
            <Typography variant="caption">Ask</Typography>
          </Box>
        </Box>
      </Box>
    );
  };

  const renderVolumeChart = () => {
    return (
      <Box>
        <Typography variant="h6" mb={2}>Top Volume</Typography>
        <ResponsiveContainer width="100%" height={250}>
          <BarChart data={volumeData}>
            <CartesianGrid strokeDasharray="3 3" opacity={0.3} />
            <XAxis dataKey="symbol" />
            <YAxis />
            <RechartsTooltip 
              contentStyle={{ 
                backgroundColor: theme.palette.background.paper,
                border: `1px solid ${theme.palette.divider}`,
                borderRadius: 8
              }}
            />
            <Bar dataKey="volume" fill="#8884d8">
              {volumeData.map((entry, index) => (
                <Cell key={`cell-${index}`} fill={['#0088FE', '#00C49F', '#FFBB28', '#FF8042', '#8884d8'][index % 5]} />
              ))}
            </Bar>
          </BarChart>
        </ResponsiveContainer>
      </Box>
    );
  };

  const renderPerformanceMetrics = () => {
    const currentPerf = marketData?.performance;
    
    return (
      <Box>
        <Typography variant="h6" mb={2}>System Performance</Typography>
        
        <Grid container spacing={2} mb={2}>
          <Grid item xs={4}>
            <Box textAlign="center">
              <FlashOn color="warning" />
              <Typography variant="h4">{currentPerf?.messages_per_second || 0}</Typography>
              <Typography variant="caption" color="text.secondary">Messages/sec</Typography>
            </Box>
          </Grid>
          <Grid item xs={4}>
            <Box textAlign="center">
              <Speed color="success" />
              <Typography variant="h4">{currentPerf?.avg_latency_ms?.toFixed(2) || 0}</Typography>
              <Typography variant="caption" color="text.secondary">Latency (ms)</Typography>
            </Box>
          </Grid>
          <Grid item xs={4}>
            <Box textAlign="center">
              <SignalCellularAlt color="info" />
              <Typography variant="h4">{currentPerf?.memory_usage_mb?.toFixed(1) || 0}</Typography>
              <Typography variant="caption" color="text.secondary">Memory (MB)</Typography>
            </Box>
          </Grid>
        </Grid>
        
        <ResponsiveContainer width="100%" height={200}>
          <LineChart data={performanceData}>
            <CartesianGrid strokeDasharray="3 3" opacity={0.3} />
            <XAxis dataKey="time" />
            <YAxis yAxisId="left" />
            <YAxis yAxisId="right" orientation="right" />
            <RechartsTooltip 
              contentStyle={{ 
                backgroundColor: theme.palette.background.paper,
                border: `1px solid ${theme.palette.divider}`,
                borderRadius: 8
              }}
            />
            <Line yAxisId="left" type="monotone" dataKey="mps" stroke="#ff9800" strokeWidth={2} dot={false} />
            <Line yAxisId="right" type="monotone" dataKey="latency" stroke="#4caf50" strokeWidth={2} dot={false} />
          </LineChart>
        </ResponsiveContainer>
      </Box>
    );
  };

  const renderOrderBookHeatmap = () => {
    // Simulated order book data
    const generateOrderBookLevels = (): OrderBookLevel[] => {
      const levels: OrderBookLevel[] = [];
      const basePrice = marketData?.symbols[0]?.last_price || 100;
      
      for (let i = 0; i < 10; i++) {
        levels.push({
          price: basePrice + (i + 1) * 0.01,
          size: Math.random() * 10000,
          total: 0
        });
      }
      
      // Calculate cumulative totals
      let total = 0;
      levels.forEach(level => {
        total += level.size;
        level.total = total;
      });
      
      return levels;
    };
    
    const bidLevels = generateOrderBookLevels().reverse();
    const askLevels = generateOrderBookLevels();
    
    const renderLevel = (level: OrderBookLevel, type: 'bid' | 'ask') => {
      const maxSize = Math.max(...bidLevels.map(l => l.size), ...askLevels.map(l => l.size));
      const intensity = level.size / maxSize;
      
      return (
        <Box
          key={level.price}
          display="flex"
          alignItems="center"
          p={0.5}
          sx={{
            background: type === 'bid' 
              ? `linear-gradient(to right, transparent, rgba(76, 175, 80, ${intensity * 0.3}))`
              : `linear-gradient(to left, transparent, rgba(244, 67, 54, ${intensity * 0.3}))`,
            '&:hover': {
              backgroundColor: 'action.hover'
            }
          }}
        >
          <Grid container alignItems="center">
            <Grid item xs={4}>
              <Typography variant="body2" color={type === 'bid' ? 'success.main' : 'error.main'}>
                ${level.price.toFixed(4)}
              </Typography>
            </Grid>
            <Grid item xs={4} textAlign="center">
              <Typography variant="body2">{formatVolume(level.size)}</Typography>
            </Grid>
            <Grid item xs={4} textAlign="right">
              <Typography variant="body2" color="text.secondary">
                {formatVolume(level.total)}
              </Typography>
            </Grid>
          </Grid>
        </Box>
      );
    };
    
    return (
      <Box>
        <Typography variant="h6" mb={2}>Order Book Depth</Typography>
        
        <Grid container spacing={2}>
          <Grid item xs={6}>
            <Typography variant="subtitle2" color="success.main" mb={1}>Bids</Typography>
            {bidLevels.map(level => renderLevel(level, 'bid'))}
          </Grid>
          <Grid item xs={6}>
            <Typography variant="subtitle2" color="error.main" mb={1}>Asks</Typography>
            {askLevels.map(level => renderLevel(level, 'ask'))}
          </Grid>
        </Grid>
      </Box>
    );
  };

  // Main render
  return (
    <ThemeProvider theme={theme}>
      <CssBaseline />
      <Box sx={{ display: 'flex', minHeight: '100vh' }}>
        {/* Sidebar */}
        <Drawer
          variant="temporary"
          open={drawerOpen}
          onClose={() => setDrawerOpen(false)}
          sx={{
            width: 240,
            flexShrink: 0,
            '& .MuiDrawer-paper': {
              width: 240,
              boxSizing: 'border-box',
              background: theme.palette.background.paper,
            },
          }}
        >
          <Box sx={{ p: 2 }}>
            <Typography variant="h6" gutterBottom>
              Market Data Engine
            </Typography>
            <Divider sx={{ my: 2 }} />
            
            <List>
              <ListItemButton>
                <ListItemIcon><Dashboard /></ListItemIcon>
                <ListItemText primary="Dashboard" />
              </ListItemButton>
              <ListItemButton>
                <ListItemIcon><Analytics /></ListItemIcon>
                <ListItemText primary="Analytics" />
              </ListItemButton>
              <ListItemButton>
                <ListItemIcon><Timeline /></ListItemIcon>
                <ListItemText primary="Charts" />
              </ListItemButton>
              <ListItemButton>
                <ListItemIcon><BookmarkBorder /></ListItemIcon>
                <ListItemText primary="Watchlist" />
              </ListItemButton>
              <ListItemButton onClick={() => setShowSettings(!showSettings)}>
                <ListItemIcon><Settings /></ListItemIcon>
                <ListItemText primary="Settings" />
              </ListItemButton>
            </List>
            
            <Collapse in={showSettings}>
              <Box sx={{ p: 2 }}>
                <FormControlLabel
                  control={<Switch checked={autoScroll} onChange={(e) => setAutoScroll(e.target.checked)} />}
                  label="Auto-scroll"
                />
              </Box>
            </Collapse>
          </Box>
        </Drawer>

        {/* Main content */}
        <Box component="main" sx={{ flexGrow: 1, p: 3 }}>
          {/* App Bar */}
          <AppBar position="static" elevation={0} sx={{ mb: 3, borderRadius: 2 }}>
            <Toolbar>
              <IconButton
                color="inherit"
                edge="start"
                onClick={() => setDrawerOpen(true)}
                sx={{ mr: 2 }}
              >
                <MenuIcon />
              </IconButton>
              
              <Typography variant="h6" sx={{ flexGrow: 1 }}>
                Real-Time Market Data Dashboard
              </Typography>
              
              <Box display="flex" alignItems="center" gap={2}>
                <GlowingBadge 
                  variant="dot" 
                  color={connected ? "success" : "error"}
                  invisible={!connected}
                >
                  <Chip
                    icon={<NetworkCheck />}
                    label={connected ? "Connected" : "Disconnected"}
                    color={connected ? "success" : "error"}
                    size="small"
                  />
                </GlowingBadge>
                
                <IconButton onClick={() => setIsPaused(!isPaused)} color="inherit">
                  {isPaused ? <PlayArrow /> : <Pause />}
                </IconButton>
                
                <IconButton onClick={() => window.location.reload()} color="inherit">
                  <Refresh />
                </IconButton>
                
                <IconButton onClick={() => setDarkMode(!darkMode)} color="inherit">
                  {darkMode ? <Brightness7 /> : <Brightness4 />}
                </IconButton>
              </Box>
            </Toolbar>
          </AppBar>

          {/* Notifications */}
          <Collapse in={notifications.length > 0}>
            <Box mb={2}>
              {notifications.map((notification, index) => (
                <Fade in={true} key={index}>
                  <Alert 
                    severity="info" 
                    sx={{ mb: 1 }}
                    action={
                      <IconButton
                        size="small"
                        onClick={() => setNotifications(prev => prev.filter((_, i) => i !== index))}
                      >
                        <Close fontSize="small" />
                      </IconButton>
                    }
                  >
                    {notification}
                  </Alert>
                </Fade>
              ))}
            </Box>
          </Collapse>

          {/* Market Overview */}
          <Grid container spacing={3}>
            {/* 3D Visualization */}
            <Grid item xs={12}>
              <Typography variant="h5" gutterBottom>
                3D Market Visualization
              </Typography>
              <StyledPaper sx={{ p: 3 }}>
                {marketData?.symbols && marketData.symbols.length > 0 && (
                  <MarketData3D symbols={marketData.symbols} />
                )}
              </StyledPaper>
            </Grid>

            {/* Symbol Cards */}
            <Grid item xs={12}>
              <Typography variant="h5" gutterBottom>
                Market Overview
              </Typography>
              <Grid container spacing={2}>
                {marketData?.symbols.slice(0, 6).map(symbol => (
                  <Grid item xs={12} sm={6} md={4} lg={2} key={symbol.symbol}>
                    <Zoom in={true} style={{ transitionDelay: '100ms' }}>
                      <Box>{renderSymbolCard(symbol)}</Box>
                    </Zoom>
                  </Grid>
                ))}
              </Grid>
            </Grid>

            {/* Price Chart */}
            <Grid item xs={12} lg={8}>
              <StyledPaper sx={{ p: 3, height: '100%' }}>
                {renderPriceChart()}
              </StyledPaper>
            </Grid>

            {/* Order Book */}
            <Grid item xs={12} lg={4}>
              <StyledPaper sx={{ p: 3, height: '100%' }}>
                {renderOrderBookHeatmap()}
              </StyledPaper>
            </Grid>

            {/* Volume Chart */}
            <Grid item xs={12} md={6}>
              <StyledPaper sx={{ p: 3 }}>
                {renderVolumeChart()}
              </StyledPaper>
            </Grid>

            {/* Performance Metrics */}
            <Grid item xs={12} md={6}>
              <StyledPaper sx={{ p: 3 }}>
                {renderPerformanceMetrics()}
              </StyledPaper>
            </Grid>

            {/* Market Statistics */}
            <Grid item xs={12}>
              <StyledPaper sx={{ p: 3 }}>
                <Typography variant="h6" mb={2}>Market Statistics</Typography>
                <Grid container spacing={3}>
                  {marketData?.symbols.slice(0, 4).map(symbol => (
                    <Grid item xs={12} sm={6} md={3} key={`stats-${symbol.symbol}`}>
                      <Box>
                        <Typography variant="subtitle2" color="text.secondary">
                          {symbol.symbol}
                        </Typography>
                        <Box display="flex" alignItems="baseline" gap={1} mb={1}>
                          <Typography variant="h5">${formatPrice(symbol.last_price)}</Typography>
                          <Chip 
                            size="small"
                            label={formatPercent(symbol.change_percent)}
                            color={symbol.change_percent >= 0 ? 'success' : 'error'}
                          />
                        </Box>
                        <Grid container spacing={1}>
                          <Grid item xs={6}>
                            <Typography variant="caption" color="text.secondary">High</Typography>
                            <Typography variant="body2">${formatPrice(symbol.high_price)}</Typography>
                          </Grid>
                          <Grid item xs={6}>
                            <Typography variant="caption" color="text.secondary">Low</Typography>
                            <Typography variant="body2">${formatPrice(symbol.low_price)}</Typography>
                          </Grid>
                          <Grid item xs={6}>
                            <Typography variant="caption" color="text.secondary">VWAP</Typography>
                            <Typography variant="body2">${formatPrice(symbol.vwap)}</Typography>
                          </Grid>
                          <Grid item xs={6}>
                            <Typography variant="caption" color="text.secondary">Trades</Typography>
                            <Typography variant="body2">{symbol.trade_count}</Typography>
                          </Grid>
                        </Grid>
                      </Box>
                    </Grid>
                  ))}
                </Grid>
              </StyledPaper>
            </Grid>
          </Grid>
        </Box>
      </Box>
    </ThemeProvider>
  );
};

export default App; 