#include <iostream>
#include <thread>
#include <chrono>
#include <atomic>
#include <string>
#include <sstream>
#include <nlohmann/json.hpp>
#include "core/market_data_types.hpp"
#include "feed_handler/order_book.hpp"

namespace market_data {

class SimpleMarketDataServer {
private:
    OrderBookManager& book_manager_;
    std::atomic<bool> running_{false};
    std::thread server_thread_;
    
public:
    explicit SimpleMarketDataServer(OrderBookManager& manager) 
        : book_manager_(manager) {}
    
    ~SimpleMarketDataServer() {
        stop();
    }
    
    void start() {
        running_.store(true, std::memory_order_release);
        server_thread_ = std::thread(&SimpleMarketDataServer::server_loop, this);
        
        std::cout << "🌐 Simple Market Data Server Started\n";
        std::cout << "====================================\n";
        std::cout << "📊 Serving market data on console output\n";
        std::cout << "⌨️  Press Ctrl+C to stop\n\n";
    }
    
    void stop() {
        if (!running_.load()) return;
        
        running_.store(false, std::memory_order_release);
        if (server_thread_.joinable()) {
            server_thread_.join();
        }
        
        std::cout << "🛑 Simple server stopped\n";
    }
    
private:
    void server_loop() {
        auto last_output = std::chrono::steady_clock::now();
        
        while (running_.load(std::memory_order_acquire)) {
            const auto now = std::chrono::steady_clock::now();
            
            // Output market summary every 5 seconds
            if (now - last_output > std::chrono::seconds(5)) {
                print_market_summary();
                last_output = now;
            }
            
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
    }
    
    void print_market_summary() {
        const auto summary_json = book_manager_.get_market_summary_json();
        
        try {
            auto summary = nlohmann::json::parse(summary_json);
            
            std::cout << "📊 Market Data Summary (" 
                      << std::chrono::duration_cast<std::chrono::seconds>(
                          std::chrono::system_clock::now().time_since_epoch()).count()
                      << "):\n";
            std::cout << "   Total symbols: " << summary["total_symbols"] << "\n";
            std::cout << "   Total updates: " << summary["total_updates"] << "\n";
            
            if (summary.contains("symbols") && !summary["symbols"].empty()) {
                std::cout << "   Active symbols:\n";
                
                for (const auto& symbol : summary["symbols"]) {
                    std::cout << "     " << symbol["symbol"].get<std::string>() 
                              << ": $" << std::fixed << std::setprecision(2) 
                              << symbol["mid_price"].get<double>()
                              << " (vol: " << symbol["volume"] << ")\n";
                }
            }
            
            std::cout << "\n";
            
        } catch (const std::exception& e) {
            std::cout << "Error parsing market summary: " << e.what() << "\n";
        }
    }
};

} // namespace market_data

int main(int argc, char* argv[]) {
    std::cout << "🌐 Simple Market Data Server\n";
    std::cout << "============================\n\n";
    
    try {
        // Create market data infrastructure
        market_data::MarketDataQueue queue;
        market_data::MarketDataAggregator aggregator(queue);
        market_data::SimpleMarketDataServer server(aggregator.get_book_manager());
        
        // Start services
        aggregator.start();
        server.start();
        
        std::cout << "🎯 Services started successfully!\n";
        std::cout << "📊 Market data aggregator running\n";
        std::cout << "🖥️  Console output server running\n";
        std::cout << "⌨️  Press Ctrl+C to stop\n\n";
        
        // Keep server running
        while (true) {
            std::this_thread::sleep_for(std::chrono::seconds(1));
            
            // Print periodic stats
            static auto last_stats = std::chrono::steady_clock::now();
            const auto now = std::chrono::steady_clock::now();
            
            if (now - last_stats > std::chrono::seconds(30)) {
                const auto& metrics = aggregator.get_metrics();
                std::cout << "📊 Server Stats - Messages processed: " 
                          << metrics.messages_processed.load()
                          << ", Avg latency: " << metrics.avg_latency_ns.load() << "ns"
                          << ", Symbols: " << aggregator.get_book_manager().get_symbol_count() << "\n";
                last_stats = now;
            }
        }
        
    } catch (const std::exception& e) {
        std::cerr << "❌ Server error: " << e.what() << "\n";
        return 1;
    }
    
    return 0;
} 