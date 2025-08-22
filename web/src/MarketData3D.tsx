import React, { useEffect, useState, useRef } from 'react';
import { Box, Typography } from '@mui/material';
import { keyframes } from '@emotion/react';
import styled from '@emotion/styled';

interface Market3DProps {
  symbols: Array<{
    symbol: string;
    last_price: number;
    change_percent: number;
    volume: number;
  }>;
}

// 3D rotation keyframes
const rotate3D = keyframes`
  from {
    transform: rotateY(0deg) rotateX(10deg);
  }
  to {
    transform: rotateY(360deg) rotateX(10deg);
  }
`;

const float = keyframes`
  0%, 100% {
    transform: translateY(0px);
  }
  50% {
    transform: translateY(-20px);
  }
`;

const pulse3D = keyframes`
  0%, 100% {
    transform: scale3d(1, 1, 1);
    opacity: 1;
  }
  50% {
    transform: scale3d(1.1, 1.1, 1.1);
    opacity: 0.8;
  }
`;

// Styled components
const Scene3D = styled(Box)`
  width: 100%;
  height: 400px;
  perspective: 1000px;
  position: relative;
  overflow: hidden;
  background: radial-gradient(ellipse at center, rgba(0, 0, 0, 0.2), transparent);
`;

const Cube3D = styled(Box)<{ size: number; rotationSpeed: number }>`
  width: ${props => props.size}px;
  height: ${props => props.size}px;
  position: absolute;
  top: 50%;
  left: 50%;
  transform-style: preserve-3d;
  transform: translate(-50%, -50%);
  animation: ${rotate3D} ${props => props.rotationSpeed}s linear infinite;
`;

const CubeFace = styled(Box)<{ 
  transform: string; 
  isPositive?: boolean; 
  size: number;
  opacity?: number;
}>`
  position: absolute;
  width: ${props => props.size}px;
  height: ${props => props.size}px;
  background: ${props => props.isPositive 
    ? 'linear-gradient(135deg, rgba(76, 175, 80, 0.8), rgba(129, 199, 132, 0.6))' 
    : 'linear-gradient(135deg, rgba(244, 67, 54, 0.8), rgba(239, 83, 80, 0.6))'};
  border: 2px solid ${props => props.isPositive ? '#4caf50' : '#f44336'};
  transform: ${props => props.transform};
  display: flex;
  flex-direction: column;
  align-items: center;
  justify-content: center;
  backdrop-filter: blur(5px);
  opacity: ${props => props.opacity || 0.9};
  transition: all 0.3s ease;
  
  &:hover {
    opacity: 1;
    background: ${props => props.isPositive 
      ? 'linear-gradient(135deg, rgba(76, 175, 80, 1), rgba(129, 199, 132, 0.8))' 
      : 'linear-gradient(135deg, rgba(244, 67, 54, 1), rgba(239, 83, 80, 0.8))'};
  }
`;

const FloatingParticle = styled(Box)<{ 
  x: number; 
  y: number; 
  delay: number;
  isPositive: boolean;
}>`
  position: absolute;
  width: 4px;
  height: 4px;
  background: ${props => props.isPositive ? '#4caf50' : '#f44336'};
  border-radius: 50%;
  left: ${props => props.x}%;
  top: ${props => props.y}%;
  animation: ${float} 3s ease-in-out ${props => props.delay}s infinite;
  box-shadow: 0 0 10px ${props => props.isPositive ? '#4caf50' : '#f44336'};
`;

const DataStream = styled(Box)<{ angle: number; delay: number }>`
  position: absolute;
  width: 2px;
  height: 100%;
  background: linear-gradient(to bottom, 
    transparent, 
    rgba(33, 150, 243, 0.8), 
    transparent
  );
  top: 0;
  left: 50%;
  transform: translateX(-50%) rotate(${props => props.angle}deg);
  transform-origin: center;
  opacity: 0;
  animation: ${keyframes`
    0% { opacity: 0; transform: translateX(-50%) rotate(${props => props.angle}deg) scaleY(0); }
    50% { opacity: 1; transform: translateX(-50%) rotate(${props => props.angle}deg) scaleY(1); }
    100% { opacity: 0; transform: translateX(-50%) rotate(${props => props.angle}deg) scaleY(0); }
  `} 4s ease-in-out ${props => props.delay}s infinite;
`;

const HologramGrid = styled(Box)`
  position: absolute;
  width: 100%;
  height: 100%;
  background-image: 
    linear-gradient(0deg, rgba(0, 255, 255, 0.1) 1px, transparent 1px),
    linear-gradient(90deg, rgba(0, 255, 255, 0.1) 1px, transparent 1px);
  background-size: 50px 50px;
  animation: ${keyframes`
    0% { transform: translate(0, 0); }
    100% { transform: translate(50px, 50px); }
  `} 10s linear infinite;
  pointer-events: none;
`;

const MarketData3D: React.FC<Market3DProps> = ({ symbols }) => {
  const [selectedIndex, setSelectedIndex] = useState(0);
  const sceneRef = useRef<HTMLDivElement>(null);
  
  useEffect(() => {
    const interval = setInterval(() => {
      setSelectedIndex((prev) => (prev + 1) % Math.min(symbols.length, 6));
    }, 3000);
    
    return () => clearInterval(interval);
  }, [symbols.length]);
  
  const renderCube = (symbol: any, index: number) => {
    const isSelected = index === selectedIndex;
    const size = isSelected ? 120 : 80;
    const radius = 150;
    const angle = (index / 6) * 2 * Math.PI;
    const x = Math.cos(angle) * radius;
    const z = Math.sin(angle) * radius;
    const isPositive = symbol.change_percent >= 0;
    
    return (
      <Box
        key={symbol.symbol}
        sx={{
          position: 'absolute',
          top: '50%',
          left: '50%',
          transform: `translate(-50%, -50%) translateX(${x}px) translateZ(${z}px)`,
          transformStyle: 'preserve-3d',
          transition: 'all 0.5s ease',
          cursor: 'pointer',
        }}
        onClick={() => setSelectedIndex(index)}
      >
        <Cube3D size={size} rotationSpeed={isSelected ? 10 : 20}>
          {/* Front face */}
          <CubeFace
            transform={`translateZ(${size/2}px)`}
            isPositive={isPositive}
            size={size}
          >
            <Typography variant="h6" sx={{ color: 'white', fontWeight: 'bold' }}>
              {symbol.symbol}
            </Typography>
            <Typography variant="body2" sx={{ color: 'white' }}>
              ${symbol.last_price.toFixed(4)}
            </Typography>
          </CubeFace>
          
          {/* Back face */}
          <CubeFace
            transform={`rotateY(180deg) translateZ(${size/2}px)`}
            isPositive={isPositive}
            size={size}
          >
            <Typography variant="body2" sx={{ color: 'white' }}>
              Volume
            </Typography>
            <Typography variant="h6" sx={{ color: 'white' }}>
              {(symbol.volume / 1000).toFixed(1)}K
            </Typography>
          </CubeFace>
          
          {/* Right face */}
          <CubeFace
            transform={`rotateY(90deg) translateZ(${size/2}px)`}
            isPositive={isPositive}
            size={size}
          >
            <Typography variant="h6" sx={{ color: 'white' }}>
              {isPositive ? '+' : ''}{symbol.change_percent.toFixed(2)}%
            </Typography>
          </CubeFace>
          
          {/* Left face */}
          <CubeFace
            transform={`rotateY(-90deg) translateZ(${size/2}px)`}
            isPositive={isPositive}
            size={size}
          >
            <Typography variant="body2" sx={{ color: 'white' }}>
              {isPositive ? '📈' : '📉'}
            </Typography>
          </CubeFace>
          
          {/* Top face */}
          <CubeFace
            transform={`rotateX(90deg) translateZ(${size/2}px)`}
            isPositive={isPositive}
            size={size}
            opacity={0.6}
          />
          
          {/* Bottom face */}
          <CubeFace
            transform={`rotateX(-90deg) translateZ(${size/2}px)`}
            isPositive={isPositive}
            size={size}
            opacity={0.6}
          />
        </Cube3D>
      </Box>
    );
  };
  
  // Generate floating particles
  const particles = Array.from({ length: 20 }, (_, i) => ({
    x: Math.random() * 100,
    y: Math.random() * 100,
    delay: Math.random() * 3,
    isPositive: Math.random() > 0.5
  }));
  
  // Generate data streams
  const dataStreams = Array.from({ length: 6 }, (_, i) => ({
    angle: i * 60,
    delay: i * 0.5
  }));
  
  return (
    <Scene3D ref={sceneRef}>
      <HologramGrid />
      
      {/* Data streams */}
      {dataStreams.map((stream, index) => (
        <DataStream key={index} angle={stream.angle} delay={stream.delay} />
      ))}
      
      {/* Floating particles */}
      {particles.map((particle, index) => (
        <FloatingParticle
          key={index}
          x={particle.x}
          y={particle.y}
          delay={particle.delay}
          isPositive={particle.isPositive}
        />
      ))}
      
      {/* 3D Cubes */}
      <Box
        sx={{
          position: 'absolute',
          top: '50%',
          left: '50%',
          transform: 'translate(-50%, -50%)',
          transformStyle: 'preserve-3d',
          width: '100%',
          height: '100%',
        }}
      >
        {symbols.slice(0, 6).map((symbol, index) => renderCube(symbol, index))}
      </Box>
      
      {/* Center display */}
      {symbols[selectedIndex] && (
        <Box
          sx={{
            position: 'absolute',
            bottom: 20,
            left: '50%',
            transform: 'translateX(-50%)',
            textAlign: 'center',
            background: 'rgba(0, 0, 0, 0.8)',
            padding: 2,
            borderRadius: 2,
            backdropFilter: 'blur(10px)',
            border: '1px solid rgba(255, 255, 255, 0.2)',
          }}
        >
          <Typography variant="h5" sx={{ color: 'white', mb: 1 }}>
            {symbols[selectedIndex].symbol}
          </Typography>
          <Typography variant="body1" sx={{ color: '#90caf9' }}>
            Price: ${symbols[selectedIndex].last_price.toFixed(4)}
          </Typography>
          <Typography 
            variant="body1" 
            sx={{ 
              color: symbols[selectedIndex].change_percent >= 0 ? '#4caf50' : '#f44336' 
            }}
          >
            Change: {symbols[selectedIndex].change_percent >= 0 ? '+' : ''}
            {symbols[selectedIndex].change_percent.toFixed(2)}%
          </Typography>
        </Box>
      )}
    </Scene3D>
  );
};

export default MarketData3D; 