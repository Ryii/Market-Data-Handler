#include "core/market_data_types.hpp"
#include <algorithm>
#include <cmath>
#include <numeric>

namespace market_data {

void MarketStatistics::update_trade(Price price, Quantity quantity) {
    last_update = now();
    last_price = price;
    
    // Update OHLC
    if (trade_count == 0) {
        open_price = price;
        high_price = price;
        low_price = price;
    } else {
        high_price = std::max(high_price, price);
        low_price = std::min(low_price, price);
    }
    
    // Update volume-weighted average price
    const uint64_t old_total_value = static_cast<uint64_t>(vwap) * total_volume;
    const uint64_t new_trade_value = static_cast<uint64_t>(price) * quantity;
    
    total_volume += quantity;
    ++trade_count;
    
    if (total_volume > 0) {
        vwap = static_cast<Price>((old_total_value + new_trade_value) / total_volume);
    }
}

void MarketStatistics::update_quote(Price bid, Price ask) {
    last_update = now();
    bid_ask_spread = ask - bid;
}

double MarketStatistics::calculate_volatility() const {
    // Simplified realized volatility calculation
    // In practice, you'd use a rolling window of returns
    if (trade_count < 2 || high_price == low_price) {
        return 0.0;
    }
    
    const double price_range = to_double(high_price - low_price);
    const double mid_price = to_double((high_price + low_price) / 2);
    
    if (mid_price == 0.0) return 0.0;
    
    // Parkinson's volatility estimator (simplified)
    return (price_range / mid_price) * std::sqrt(252.0);  // Annualized
}

void PerformanceMetrics::update_latency(uint64_t latency_ns) {
    messages_processed.fetch_add(1, std::memory_order_relaxed);
    
    // Update average latency using exponential moving average
    const uint64_t current_avg = avg_latency_ns.load(std::memory_order_relaxed);
    const uint64_t new_avg = (current_avg * 15 + latency_ns) / 16;  // Alpha = 1/16
    avg_latency_ns.store(new_avg, std::memory_order_relaxed);
    
    // Update max latency
    uint64_t current_max = max_latency_ns.load(std::memory_order_relaxed);
    while (latency_ns > current_max) {
        if (max_latency_ns.compare_exchange_weak(current_max, latency_ns, std::memory_order_relaxed)) {
            break;
        }
    }
}

void PerformanceMetrics::reset_counters() {
    messages_processed.store(0, std::memory_order_relaxed);
    messages_per_second.store(0, std::memory_order_relaxed);
    avg_latency_ns.store(0, std::memory_order_relaxed);
    max_latency_ns.store(0, std::memory_order_relaxed);
    queue_depth.store(0, std::memory_order_relaxed);
    memory_usage_bytes.store(0, std::memory_order_relaxed);
}

} // namespace market_data 