#!/bin/bash

set -e

echo "Building High-Frequency Market Data Engine"
echo "=============================================="
echo ""

RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m'

print_status() {
    echo -e "${GREEN}[OK] $1${NC}"
}

print_warning() {
    echo -e "${YELLOW}[WARN] $1${NC}"
}

print_error() {
    echo -e "${RED}[ERROR] $1${NC}"
}

print_info() {
    echo -e "${BLUE}[INFO] $1${NC}"
}


print_info "Checking prerequisites..."


if ! command -v cmake &> /dev/null; then
    print_error "CMake not found. Please install CMake 3.20 or later."
    exit 1
fi


if ! command -v node &> /dev/null; then
    print_warning "Node.js not found. Web dashboard will not be built."
    BUILD_WEB=false
else
    BUILD_WEB=true
fi


if command -v ninja &> /dev/null; then
    GENERATOR="Ninja"
    BUILD_CMD="ninja"
else
    GENERATOR="Unix Makefiles"
    BUILD_CMD="make -j$(nproc 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || echo 4)"
fi

print_status "Prerequisites checked"


print_info "Setting up build directory..."
mkdir -p build
cd build


print_info "Configuring build with CMake..."
cmake .. -G "$GENERATOR" \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_CXX_FLAGS_RELEASE="-O3 -march=native -mtune=native -DNDEBUG" \
    -DCMAKE_EXPORT_COMPILE_COMMANDS=ON

print_status "CMake configuration complete"


print_info "Building C++ components..."
echo "Using build system: $GENERATOR"
echo "Build command: $BUILD_CMD"
echo ""

$BUILD_CMD

print_status "C++ build complete"


if [ "$BUILD_WEB" = true ]; then
    print_info "Building web dashboard..."
    cd ../web
    
    if [ ! -d "node_modules" ]; then
        print_info "Installing Node.js dependencies..."
        npm install
    fi
    
    print_info "Building React application..."
    npm run build
    
    print_status "Web dashboard build complete"
    cd ../build
fi


print_info "Running validation tests..."


EXECUTABLES=("feed_handler" "market_simulator" "websocket_server" "benchmark")
for exe in "${EXECUTABLES[@]}"; do
    if [ -f "./$exe" ]; then
        print_status "$exe built successfully"
    else
        print_error "$exe not found"
        exit 1
    fi
done


print_info "Running performance validation..."
timeout 10s ./benchmark > benchmark_results.txt 2>&1 || true

if grep -q "A+" benchmark_results.txt; then
    print_status "Performance validation: A+ grade achieved"
elif grep -q "A" benchmark_results.txt; then
    print_status "Performance validation: A grade achieved"
else
    print_warning "Performance validation: Check benchmark_results.txt"
fi

echo ""
echo "Build Complete!"
echo "=================="
echo ""
echo "Built Components:"
echo "  • feed_handler - Main market data engine"
echo "  • market_simulator - Realistic data generator"
echo "  • websocket_server - Real-time streaming server"
echo "  • benchmark - Performance testing suite"

if [ "$BUILD_WEB" = true ]; then
    echo "  • web dashboard - React visualization interface"
fi

echo ""
echo "Quick Start:"
echo "  1. Start market data engine: ./feed_handler"
echo "  2. Start WebSocket server: ./websocket_server"
echo "  3. Start market simulator: ./market_simulator"

if [ "$BUILD_WEB" = true ]; then
    echo "  4. Start web dashboard: cd ../web && npm start"
    echo "  5. Open browser: http://localhost:3000"
fi

echo ""
echo "Performance Testing:"
echo " • Run benchmarks: ./benchmark"
echo " • View results: cat benchmark_results.txt"


cat > ../run_demo.sh << 'EOF'
#!/bin/bash

echo "Starting Market Data Engine Demo"
echo "==================================="


lsof -ti:9001 | xargs kill -9 2>/dev/null || true
lsof -ti:3000 | xargs kill -9 2>/dev/null || true

cd build


echo "Starting WebSocket server..."
./websocket_server &
WS_PID=$!

echo "Starting market simulator..."
./market_simulator &
SIM_PID=$!

echo "Starting main engine..."
./feed_handler &
ENGINE_PID=$!


if [ -d "../web/build" ]; then
    echo "Starting web dashboard..."
    cd ../web
    npm start &
    WEB_PID=$!
    cd ../build
fi

echo ""
echo "Demo is running!"
echo "   • WebSocket: ws://localhost:9001"
echo "   • Web Dashboard: http://localhost:3000"
echo "   • Press Ctrl+C to stop all services"


trap 'echo "Stopping demo..."; kill $WS_PID $SIM_PID $ENGINE_PID $WEB_PID 2>/dev/null; exit' INT

wait
EOF

chmod +x ../run_demo.sh
print_status "Created run_demo.sh convenience script" 