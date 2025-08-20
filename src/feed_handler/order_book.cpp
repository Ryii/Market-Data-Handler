#include "feed_handler/order_book.hpp"
#include <nlohmann/json.hpp>
#include <sstream>
#include <iomanip>

namespace market_data {

OrderBook::OrderBook(const Symbol& symbol) : symbol_(symbol) {
    statistics_.symbol = symbol;
    statistics_.last_update = now();
}

void OrderBook::add_order(Price price, Quantity quantity, Side side, Timestamp timestamp) {
    const auto start_time = now();
    
    auto& levels = (side == Side::BUY) ? bids_ : asks_;
    auto it = find_or_create_level(levels, price);
    
    it->second.quantity += quantity;
    it->second.order_count++;
    
    update_best_prices();
    
    // Update performance metrics
    const auto latency = duration_ns(start_time, now());
    update_count_.fetch_add(1, std::memory_order_relaxed);
    total_latency_ns_.fetch_add(latency, std::memory_order_relaxed);
}

void OrderBook::modify_order(Price old_price, Price new_price, Quantity new_quantity, Side side, Timestamp timestamp) {
    auto& levels = (side == Side::BUY) ? bids_ : asks_;
    
    // Remove from old price level
    auto old_it = levels.find(old_price);
    if (old_it != levels.end()) {
        if (old_it->second.quantity >= new_quantity) {
            old_it->second.quantity -= new_quantity;
            if (old_it->second.order_count > 0) {
                old_it->second.order_count--;
            }
            
            if (old_it->second.quantity == 0) {
                levels.erase(old_it);
            }
        }
    }
    
    // Add to new price level
    add_order(new_price, new_quantity, side, timestamp);
}

void OrderBook::delete_order(Price price, Quantity quantity, Side side, Timestamp timestamp) {
    auto& levels = (side == Side::BUY) ? bids_ : asks_;
    auto it = levels.find(price);
    
    if (it != levels.end()) {
        if (it->second.quantity >= quantity) {
            it->second.quantity -= quantity;
            if (it->second.order_count > 0) {
                it->second.order_count--;
            }
            
            if (it->second.quantity == 0) {
                levels.erase(it);
            }
        }
        
        update_best_prices();
    }
}

void OrderBook::update_trade(const MarketTrade& trade) {
    std::lock_guard<std::mutex> lock(stats_mutex_);
    statistics_.update_trade(trade.price, trade.quantity);
}

void OrderBook::update_quote(const MarketQuote& quote) {
    // Update order book levels
    bids_.clear();
    asks_.clear();
    
    if (quote.bid_price > 0 && quote.bid_size > 0) {
        bids_[quote.bid_price] = OrderBookLevel(quote.bid_price, quote.bid_size);
    }
    
    if (quote.ask_price > 0 && quote.ask_size > 0) {
        asks_[quote.ask_price] = OrderBookLevel(quote.ask_price, quote.ask_size);
    }
    
    update_best_prices();
    
    std::lock_guard<std::mutex> lock(stats_mutex_);
    statistics_.update_quote(quote.bid_price, quote.ask_price);
}

void OrderBook::update_level2(const std::vector<OrderBookLevel>& bids, const std::vector<OrderBookLevel>& asks, Timestamp timestamp) {
    // Full book refresh
    bids_.clear();
    asks_.clear();
    
    for (const auto& level : bids) {
        if (level.quantity > 0) {
            bids_[level.price] = level;
        }
    }
    
    for (const auto& level : asks) {
        if (level.quantity > 0) {
            asks_[level.price] = level;
        }
    }
    
    update_best_prices();
}

Price OrderBook::get_best_bid() const {
    return cached_best_bid_.load(std::memory_order_acquire);
}

Price OrderBook::get_best_ask() const {
    return cached_best_ask_.load(std::memory_order_acquire);
}

Price OrderBook::get_mid_price() const {
    const Price bid = get_best_bid();
    const Price ask = get_best_ask();
    return (bid > 0 && ask > 0) ? (bid + ask) / 2 : 0;
}

Price OrderBook::get_spread() const {
    const Price bid = get_best_bid();
    const Price ask = get_best_ask();
    return (bid > 0 && ask > 0) ? ask - bid : 0;
}

std::vector<OrderBookLevel> OrderBook::get_bids(size_t depth) const {
    std::vector<OrderBookLevel> result;
    result.reserve(depth);
    
    auto it = bids_.rbegin();  // Start from highest price
    for (size_t i = 0; i < depth && it != bids_.rend(); ++i, ++it) {
        result.push_back(it->second);
    }
    
    return result;
}

std::vector<OrderBookLevel> OrderBook::get_asks(size_t depth) const {
    std::vector<OrderBookLevel> result;
    result.reserve(depth);
    
    auto it = asks_.begin();  // Start from lowest price
    for (size_t i = 0; i < depth && it != asks_.end(); ++i, ++it) {
        result.push_back(it->second);
    }
    
    return result;
}

const MarketStatistics& OrderBook::get_statistics() const {
    std::lock_guard<std::mutex> lock(stats_mutex_);
    return statistics_;
}

double OrderBook::get_imbalance() const {
    const auto top_bids = get_bids(5);
    const auto top_asks = get_asks(5);
    
    if (top_bids.empty() || top_asks.empty()) {
        return 0.0;
    }
    
    const uint64_t bid_volume = std::accumulate(top_bids.begin(), top_bids.end(), 0ULL,
        [](uint64_t sum, const OrderBookLevel& level) { return sum + level.quantity; });
    
    const uint64_t ask_volume = std::accumulate(top_asks.begin(), top_asks.end(), 0ULL,
        [](uint64_t sum, const OrderBookLevel& level) { return sum + level.quantity; });
    
    const uint64_t total_volume = bid_volume + ask_volume;
    if (total_volume == 0) return 0.0;
    
    return static_cast<double>(bid_volume - ask_volume) / total_volume;
}

double OrderBook::get_weighted_mid() const {
    const Price best_bid = get_best_bid();
    const Price best_ask = get_best_ask();
    
    if (best_bid == 0 || best_ask == 0) {
        return 0.0;
    }
    
    const auto bid_it = bids_.find(best_bid);
    const auto ask_it = asks_.find(best_ask);
    
    if (bid_it == bids_.end() || ask_it == asks_.end()) {
        return to_double(get_mid_price());
    }
    
    const uint64_t bid_size = bid_it->second.quantity;
    const uint64_t ask_size = ask_it->second.quantity;
    const uint64_t total_size = bid_size + ask_size;
    
    if (total_size == 0) {
        return to_double(get_mid_price());
    }
    
    // Weight by opposite side size
    const double weighted_price = (to_double(best_bid) * ask_size + to_double(best_ask) * bid_size) / total_size;
    return weighted_price;
}

double OrderBook::get_average_latency_ns() const {
    const uint64_t count = update_count_.load();
    const uint64_t total = total_latency_ns_.load();
    return count > 0 ? static_cast<double>(total) / count : 0.0;
}

std::string OrderBook::to_json() const {
    nlohmann::json j;
    
    j["symbol"] = symbol_to_string(symbol_);
    j["timestamp"] = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::high_resolution_clock::now().time_since_epoch()).count();
    
    // Best bid/ask
    j["best_bid"] = to_double(get_best_bid());
    j["best_ask"] = to_double(get_best_ask());
    j["mid_price"] = to_double(get_mid_price());
    j["spread"] = to_double(get_spread());
    j["weighted_mid"] = get_weighted_mid();
    j["imbalance"] = get_imbalance();
    
    // Order book levels
    auto bids = get_bids(10);
    auto asks = get_asks(10);
    
    j["bids"] = nlohmann::json::array();
    for (const auto& level : bids) {
        j["bids"].push_back({
            {"price", to_double(level.price)},
            {"quantity", level.quantity},
            {"orders", level.order_count}
        });
    }
    
    j["asks"] = nlohmann::json::array();
    for (const auto& level : asks) {
        j["asks"].push_back({
            {"price", to_double(level.price)},
            {"quantity", level.quantity},
            {"orders", level.order_count}
        });
    }
    
    // Statistics
    std::lock_guard<std::mutex> lock(stats_mutex_);
    j["statistics"] = {
        {"last_price", to_double(statistics_.last_price)},
        {"high", to_double(statistics_.high_price)},
        {"low", to_double(statistics_.low_price)},
        {"open", to_double(statistics_.open_price)},
        {"vwap", to_double(statistics_.vwap)},
        {"volume", statistics_.total_volume},
        {"trade_count", statistics_.trade_count},
        {"volatility", statistics_.calculate_volatility()}
    };
    
    return j.dump();
}

std::string OrderBook::get_top_of_book_json() const {
    nlohmann::json j;
    
    j["symbol"] = symbol_to_string(symbol_);
    j["timestamp"] = std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::high_resolution_clock::now().time_since_epoch()).count();
    j["best_bid"] = to_double(get_best_bid());
    j["best_ask"] = to_double(get_best_ask());
    j["mid_price"] = to_double(get_mid_price());
    j["spread"] = to_double(get_spread());
    
    return j.dump();
}

void OrderBook::update_best_prices() {
    Price best_bid = 0;
    Price best_ask = 0;
    
    if (!bids_.empty()) {
        best_bid = bids_.rbegin()->first;  // Highest bid
    }
    
    if (!asks_.empty()) {
        best_ask = asks_.begin()->first;   // Lowest ask
    }
    
    cached_best_bid_.store(best_bid, std::memory_order_release);
    cached_best_ask_.store(best_ask, std::memory_order_release);
}

void OrderBook::update_statistics_unsafe(Price price, Quantity quantity, Timestamp timestamp) {
    statistics_.update_trade(price, quantity);
}

OrderBook::PriceLevelMap::iterator OrderBook::find_or_create_level(PriceLevelMap& levels, Price price) {
    auto it = levels.find(price);
    if (it == levels.end()) {
        it = levels.emplace(price, OrderBookLevel(price, 0)).first;
    }
    return it;
}

// OrderBookManager implementation
OrderBook* OrderBookManager::get_or_create_book(const Symbol& symbol) {
    const std::string symbol_str = symbol_to_string(symbol);
    
    {
        std::shared_lock<std::shared_mutex> lock(books_mutex_);
        auto it = books_.find(symbol_str);
        if (it != books_.end()) {
            return it->second.get();
        }
    }
    
    // Create new book
    std::unique_lock<std::shared_mutex> lock(books_mutex_);
    auto [it, inserted] = books_.emplace(symbol_str, std::make_unique<OrderBook>(symbol));
    
    if (inserted) {
        active_symbols_.fetch_add(1, std::memory_order_relaxed);
    }
    
    return it->second.get();
}

OrderBook* OrderBookManager::get_book(const Symbol& symbol) const {
    const std::string symbol_str = symbol_to_string(symbol);
    
    std::shared_lock<std::shared_mutex> lock(books_mutex_);
    auto it = books_.find(symbol_str);
    return (it != books_.end()) ? it->second.get() : nullptr;
}

void OrderBookManager::remove_book(const Symbol& symbol) {
    const std::string symbol_str = symbol_to_string(symbol);
    
    std::unique_lock<std::shared_mutex> lock(books_mutex_);
    auto it = books_.find(symbol_str);
    if (it != books_.end()) {
        books_.erase(it);
        active_symbols_.fetch_sub(1, std::memory_order_relaxed);
    }
}

void OrderBookManager::update_trade(const MarketTrade& trade) {
    auto* book = get_or_create_book(trade.symbol);
    book->update_trade(trade);
    total_updates_.fetch_add(1, std::memory_order_relaxed);
}

void OrderBookManager::update_quote(const MarketQuote& quote) {
    auto* book = get_or_create_book(quote.symbol);
    book->update_quote(quote);
    total_updates_.fetch_add(1, std::memory_order_relaxed);
}

void OrderBookManager::process_message(const MarketDataMessage& message) {
    switch (message.type) {
        case MessageType::TRADE:
            update_trade(message.trade_data);
            break;
        case MessageType::QUOTE:
            update_quote(message.quote_data);
            break;
        default:
            // Handle other message types
            break;
    }
}

std::vector<std::string> OrderBookManager::get_active_symbols() const {
    std::vector<std::string> symbols;
    
    std::shared_lock<std::shared_mutex> lock(books_mutex_);
    symbols.reserve(books_.size());
    
    for (const auto& [symbol, book] : books_) {
        symbols.push_back(symbol);
    }
    
    return symbols;
}

std::string OrderBookManager::get_market_summary_json() const {
    nlohmann::json summary;
    
    std::shared_lock<std::shared_mutex> lock(books_mutex_);
    
    summary["total_symbols"] = books_.size();
    summary["total_updates"] = total_updates_.load();
    summary["timestamp"] = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::high_resolution_clock::now().time_since_epoch()).count();
    
    summary["symbols"] = nlohmann::json::array();
    for (const auto& [symbol_str, book] : books_) {
        nlohmann::json symbol_data;
        symbol_data["symbol"] = symbol_str;
        symbol_data["best_bid"] = to_double(book->get_best_bid());
        symbol_data["best_ask"] = to_double(book->get_best_ask());
        symbol_data["mid_price"] = to_double(book->get_mid_price());
        symbol_data["spread"] = to_double(book->get_spread());
        symbol_data["imbalance"] = book->get_imbalance();
        
        const auto& stats = book->get_statistics();
        symbol_data["volume"] = stats.total_volume;
        symbol_data["trade_count"] = stats.trade_count;
        symbol_data["volatility"] = stats.calculate_volatility();
        
        summary["symbols"].push_back(symbol_data);
    }
    
    return summary.dump();
}

void OrderBookManager::clear_stale_books(std::chrono::seconds max_age) {
    const auto cutoff_time = now() - max_age;
    std::vector<std::string> stale_symbols;
    
    {
        std::shared_lock<std::shared_mutex> lock(books_mutex_);
        for (const auto& [symbol, book] : books_) {
            const auto& stats = book->get_statistics();
            if (stats.last_update < cutoff_time) {
                stale_symbols.push_back(symbol);
            }
        }
    }
    
    if (!stale_symbols.empty()) {
        std::unique_lock<std::shared_mutex> lock(books_mutex_);
        for (const auto& symbol : stale_symbols) {
            books_.erase(symbol);
            active_symbols_.fetch_sub(1, std::memory_order_relaxed);
        }
    }
}

// MarketDataAggregator implementation
MarketDataAggregator::MarketDataAggregator(MarketDataQueue& queue) 
    : input_queue_(queue) {}

MarketDataAggregator::~MarketDataAggregator() {
    stop();
}

void MarketDataAggregator::start() {
    running_.store(true, std::memory_order_release);
    processing_thread_ = std::thread(&MarketDataAggregator::processing_loop, this);
}

void MarketDataAggregator::stop() {
    running_.store(false, std::memory_order_release);
    if (processing_thread_.joinable()) {
        processing_thread_.join();
    }
}

void MarketDataAggregator::processing_loop() {
    constexpr size_t BATCH_SIZE = 64;
    std::vector<MarketDataMessage> batch;
    batch.reserve(BATCH_SIZE);
    
    while (running_.load(std::memory_order_acquire)) {
        // Process messages one by one for simplicity
        MarketDataMessage msg;
        if (input_queue_.dequeue(msg)) {
            book_manager_.process_message(msg);
            
            // Update latency metrics
            const auto latency = duration_ns(msg.receive_timestamp, now());
            metrics_.update_latency(latency);
        } else {
            // Brief pause when no data available
            std::this_thread::sleep_for(std::chrono::microseconds(1));
        }
    }
}

void MarketDataAggregator::process_batch(MarketDataMessage* messages, size_t count) {
    const auto start_time = now();
    
    for (size_t i = 0; i < count; ++i) {
        book_manager_.process_message(messages[i]);
        
        // Update latency metrics
        const auto latency = duration_ns(messages[i].receive_timestamp, now());
        metrics_.update_latency(latency);
    }
    
    metrics_.queue_depth.store(input_queue_.size(), std::memory_order_relaxed);
}

} // namespace market_data 