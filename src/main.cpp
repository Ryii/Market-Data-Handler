#include <iostream>
#include <thread>
#include <chrono>
#include <signal.h>
#include <atomic>
#include <memory>
#include <iomanip>
#include <vector>
#include <string>
#include <algorithm>

#include "core/market_data_types.hpp"
#include "core/lock_free_queue.hpp"
#include "feed_handler/order_book.hpp"
#include "protocols/fix_parser.hpp"

namespace market_data {

class MarketDataEngine {
private:
    // Core components
    std::unique_ptr<MarketDataQueue> queue_;
    std::unique_ptr<MarketDataAggregator> aggregator_;
    
    // Control
    std::atomic<bool> running_{false};
    std::thread stats_thread_;
    
    // Performance tracking
    std::chrono::time_point<std::chrono::high_resolution_clock> start_time_;
    
public:
    MarketDataEngine() {
        std::cout << "🚀 Initializing Market Data Engine...\n";
        
        // Initialize queue (fixed size of 128K)
        queue_ = std::make_unique<MarketDataQueue>();
        
        // Initialize aggregator
        aggregator_ = std::make_unique<MarketDataAggregator>(*queue_);
        
        std::cout << "📊 Components initialized:\n";
        std::cout << "   - Lock-free queue: 128K capacity\n";
        std::cout << "   - Order book aggregator: Ready\n";
        std::cout << "   - Market data processor: Ready\n";
    }
    
    ~MarketDataEngine() {
        stop();
    }
    
    void start() {
        if (running_.exchange(true)) {
            return; // Already running
        }
        
        start_time_ = std::chrono::high_resolution_clock::now();
        
        std::cout << "\n🎯 Starting Market Data Engine...\n";
        
        // Start aggregator
        aggregator_->start();
        
        // Start statistics thread
        stats_thread_ = std::thread([this]() {
            print_statistics();
        });
        
        std::cout << "✅ Market Data Engine started successfully!\n";
        std::cout << "📡 Generating simulated market data...\n\n";
        
        // Generate some sample market data
        generate_sample_data();
    }
    
    void stop() {
        if (!running_.exchange(false)) {
            return; // Already stopped
        }
        
        std::cout << "\n🛑 Stopping Market Data Engine...\n";
        
        // Stop components
        aggregator_->stop();
        
        // Join threads
        if (stats_thread_.joinable()) {
            stats_thread_.join();
        }
        
        // Print final statistics
        print_final_stats();
        
        std::cout << "✅ Market Data Engine stopped.\n";
    }
    
    bool is_running() const {
        return running_.load();
    }
    
private:
    void generate_sample_data() {
        // Create a simple market data generator
        std::thread generator([this]() {
            const std::vector<std::string> symbols = {
                "AAPL", "GOOGL", "MSFT", "AMZN", "TSLA",
                "JPM", "BAC", "GS", "MS", "C"
            };
            
            std::vector<double> base_prices = {
                150.0, 2800.0, 300.0, 3300.0, 800.0,
                140.0, 30.0, 350.0, 80.0, 60.0
            };
            
            size_t message_count = 0;
            
            while (running_.load()) {
                for (size_t i = 0; i < symbols.size(); ++i) {
                    // Generate trade
                    MarketDataMessage msg(MessageType::TRADE);
                    msg.sequence_number = ++message_count;
                    msg.receive_timestamp = std::chrono::high_resolution_clock::now();
                    msg.exchange_timestamp = msg.receive_timestamp;
                    
                    // Create symbol
                    Symbol sym{};
                    std::copy(symbols[i].begin(), symbols[i].end(), sym.begin());
                    
                    // Random price movement
                    double price_change = (rand() % 100 - 50) / 100.0;
                    double new_price = base_prices[i] + price_change;
                    
                    msg.trade_data = MarketTrade(
                        msg.receive_timestamp,
                        sym,
                        from_double(new_price),
                        100 + rand() % 1000,
                        (rand() % 2) ? Side::BUY : Side::SELL,
                        message_count
                    );
                    
                    queue_->enqueue(std::move(msg));
                    
                    // Generate quote
                    MarketDataMessage quote_msg(MessageType::QUOTE);
                    quote_msg.sequence_number = ++message_count;
                    quote_msg.receive_timestamp = std::chrono::high_resolution_clock::now();
                    quote_msg.exchange_timestamp = quote_msg.receive_timestamp;
                    
                    double spread = 0.01 + (rand() % 10) / 1000.0;
                    quote_msg.quote_data = MarketQuote(
                        quote_msg.receive_timestamp,
                        sym,
                        from_double(new_price - spread/2),
                        from_double(new_price + spread/2),
                        1000 + rand() % 5000,
                        1000 + rand() % 5000
                    );
                    
                    queue_->enqueue(std::move(quote_msg));
                }
                
                // Simulate market data rate
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            }
        });
        
        generator.detach();
    }
    
    void print_statistics() {
        while (running_.load()) {
            std::this_thread::sleep_for(std::chrono::seconds(5));
            
            if (!running_.load()) break;
            
            auto& book_manager = aggregator_->get_book_manager();
            
            std::cout << "\n📊 Market Data Engine Statistics:\n";
            std::cout << "================================\n";
            std::cout << "Queue utilization: " << std::fixed << std::setprecision(1) 
                      << queue_->utilization() * 100 << "%\n";
            std::cout << "Active symbols: " << book_manager.get_symbol_count() << "\n";
            
            // Print top symbols
            auto symbols = book_manager.get_active_symbols();
            if (!symbols.empty()) {
                std::cout << "\nActive Symbols:\n";
                for (const auto& symbol_str : symbols) {
                    Symbol sym{};
                    std::copy(symbol_str.begin(), symbol_str.end(), sym.begin());
                    auto* book = book_manager.get_book(sym);
                    if (book) {
                        std::cout << "  " << symbol_str 
                                  << " - Bid: $" << to_double(book->get_best_bid())
                                  << " Ask: $" << to_double(book->get_best_ask())
                                  << " Spread: $" << to_double(book->get_spread()) << "\n";
                    }
                }
            }
            
            std::cout << "================================\n";
        }
    }
    
    void print_final_stats() {
        auto end_time = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::seconds>(
            end_time - start_time_
        );
        
        std::cout << "\n📈 Final Statistics:\n";
        std::cout << "==================\n";
        std::cout << "Run time: " << duration.count() << " seconds\n";
        std::cout << "Dropped messages: " << queue_->dropped_count() << "\n";
    }
};

// Global instance for signal handling
std::unique_ptr<MarketDataEngine> g_engine;

void signal_handler(int signal) {
    std::cout << "\n⚠️  Received signal " << signal << ", shutting down...\n";
    if (g_engine) {
        g_engine->stop();
    }
    exit(0);
}

} // namespace market_data

int main() {
    // Print banner
    std::cout << R"(
╔══════════════════════════════════════════╗
║     HIGH-FREQUENCY MARKET DATA ENGINE    ║
║          Ultra-Low Latency Feed          ║
╚══════════════════════════════════════════╝
)" << std::endl;

    // Set up signal handling
    signal(SIGINT, market_data::signal_handler);
    signal(SIGTERM, market_data::signal_handler);
    
    try {
        // Create and start engine
        market_data::g_engine = std::make_unique<market_data::MarketDataEngine>();
        market_data::g_engine->start();
        
        std::cout << "\n💡 Press Ctrl+C to stop the engine.\n\n";
        
        // Keep running until interrupted
        while (market_data::g_engine->is_running()) {
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
        
    } catch (const std::exception& e) {
        std::cerr << "\n❌ Fatal error: " << e.what() << "\n";
        return 1;
    }
    
    return 0;
} 