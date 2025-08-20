#pragma once

#include <map>
#include <unordered_map>
#include <vector>
#include <memory>
#include <mutex>
#include <shared_mutex>
#include <thread>
#include "core/market_data_types.hpp"
#include "core/lock_free_queue.hpp"

namespace market_data {

// Forward declaration
class MarketDataQueue;

// High-performance order book with L2/L3 market data support
class OrderBook {
private:
    using PriceLevelMap = std::map<Price, OrderBookLevel>;
    
    Symbol symbol_;
    PriceLevelMap bids_;   // Descending order (highest first)
    PriceLevelMap asks_;   // Ascending order (lowest first)
    
    // Real-time statistics
    mutable std::mutex stats_mutex_;
    MarketStatistics statistics_;
    
    // Performance tracking
    std::atomic<uint64_t> update_count_{0};
    std::atomic<uint64_t> total_latency_ns_{0};
    
    // Best bid/ask cache for fast access
    mutable std::atomic<Price> cached_best_bid_{0};
    mutable std::atomic<Price> cached_best_ask_{0};
    
public:
    explicit OrderBook(const Symbol& symbol);
    
    // Core order book operations
    void add_order(Price price, Quantity quantity, Side side, Timestamp timestamp);
    void modify_order(Price old_price, Price new_price, Quantity new_quantity, Side side, Timestamp timestamp);
    void delete_order(Price price, Quantity quantity, Side side, Timestamp timestamp);
    
    // Market data updates
    void update_trade(const MarketTrade& trade);
    void update_quote(const MarketQuote& quote);
    void update_level2(const std::vector<OrderBookLevel>& bids, const std::vector<OrderBookLevel>& asks, Timestamp timestamp);
    
    // Market data access
    Price get_best_bid() const;
    Price get_best_ask() const;
    Price get_mid_price() const;
    Price get_spread() const;
    
    // Deep book access
    std::vector<OrderBookLevel> get_bids(size_t depth = 10) const;
    std::vector<OrderBookLevel> get_asks(size_t depth = 10) const;
    
    // Analytics
    const MarketStatistics& get_statistics() const;
    double get_imbalance() const;  // Order book imbalance ratio
    double get_weighted_mid() const;  // Size-weighted mid price
    
    // Performance metrics
    uint64_t get_update_count() const { return update_count_.load(); }
    double get_average_latency_ns() const;
    
    // Serialization for WebSocket streaming
    std::string to_json() const;
    std::string get_top_of_book_json() const;
    
private:
    void update_best_prices();
    void update_statistics_unsafe(Price price, Quantity quantity, Timestamp timestamp);
    PriceLevelMap::iterator find_or_create_level(PriceLevelMap& levels, Price price);
};

// Order book manager for multiple symbols
class OrderBookManager {
private:
    std::unordered_map<std::string, std::unique_ptr<OrderBook>> books_;
    mutable std::shared_mutex books_mutex_;
    
    // Global statistics
    std::atomic<uint64_t> total_updates_{0};
    std::atomic<uint64_t> active_symbols_{0};
    
public:
    OrderBookManager() = default;
    
    // Book management
    OrderBook* get_or_create_book(const Symbol& symbol);
    OrderBook* get_book(const Symbol& symbol) const;
    void remove_book(const Symbol& symbol);
    
    // Bulk operations
    void update_trade(const MarketTrade& trade);
    void update_quote(const MarketQuote& quote);
    void process_message(const MarketDataMessage& message);
    
    // Analytics across all symbols
    std::vector<std::string> get_active_symbols() const;
    std::string get_market_summary_json() const;
    
    // Performance metrics
    uint64_t get_total_updates() const { return total_updates_.load(); }
    size_t get_symbol_count() const { return active_symbols_.load(); }
    
    // Cleanup
    void clear_stale_books(std::chrono::seconds max_age);
};

// Real-time market data aggregator
class MarketDataAggregator {
private:
    OrderBookManager book_manager_;
    MarketDataQueue& input_queue_;
    
    // Threading
    std::atomic<bool> running_{false};
    std::thread processing_thread_;
    
    // Performance monitoring
    PerformanceMetrics metrics_;
    
public:
    explicit MarketDataAggregator(MarketDataQueue& queue);
    ~MarketDataAggregator();
    
    // Lifecycle
    void start();
    void stop();
    bool is_running() const { return running_.load(); }
    
    // Data access
    OrderBookManager& get_book_manager() { return book_manager_; }
    const PerformanceMetrics& get_metrics() const { return metrics_; }
    
private:
    void processing_loop();
    void process_batch(MarketDataMessage* messages, size_t count);
};

} // namespace market_data 