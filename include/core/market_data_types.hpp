#pragma once

#include <cstdint>
#include <string>
#include <chrono>
#include <array>
#include <atomic>
#include <cstring>
#include <iomanip>

namespace market_data
{

    using Timestamp = std::chrono::time_point<std::chrono::high_resolution_clock>;
    using Price = int64_t;
    using Quantity = uint64_t;
    using OrderId = uint64_t;
    using Symbol = std::array<char, 16>;

    constexpr Price PRICE_SCALE = 10000;
    constexpr double to_double(Price p) { return static_cast<double>(p) / PRICE_SCALE; }
    constexpr Price from_double(double d) { return static_cast<Price>(d * PRICE_SCALE); }

    enum class Side : uint8_t
    {
        BUY = 0,
        SELL = 1
    };

    enum class OrderType : uint8_t
    {
        MARKET = 0,
        LIMIT = 1,
        STOP = 2,
        STOP_LIMIT = 3
    };

    enum class MessageType : uint8_t
    {
        TRADE = 0,
        QUOTE = 1,
        ORDER_ADD = 2,
        ORDER_MODIFY = 3,
        ORDER_DELETE = 4,
        BOOK_SNAPSHOT = 5,
        HEARTBEAT = 6,
        STATISTICS = 7
    };

    struct alignas(64) MarketTrade
    {
        Timestamp timestamp;
        Symbol symbol;
        Price price;
        Quantity quantity;
        Side aggressor_side;
        uint32_t trade_id;
        uint16_t exchange_id;
        uint8_t trade_conditions;
        uint8_t padding[5];

        MarketTrade() : timestamp{}, symbol{}, price(0), quantity(0), aggressor_side(Side::BUY),
                        trade_id(0), exchange_id(0), trade_conditions(0), padding{} {}
        MarketTrade(Timestamp ts, const Symbol &sym, Price p, Quantity q, Side side, uint32_t id)
            : timestamp(ts), symbol(sym), price(p), quantity(q), aggressor_side(side), trade_id(id),
              exchange_id(0), trade_conditions(0), padding{} {}
    };

    struct alignas(64) MarketQuote
    {
        Timestamp timestamp;
        Symbol symbol;
        Price bid_price;
        Price ask_price;
        Quantity bid_size;
        Quantity ask_size;
        uint16_t bid_levels;
        uint16_t ask_levels;
        uint16_t exchange_id;
        uint8_t quote_condition;
        uint8_t padding[1];

        MarketQuote() : timestamp{}, symbol{}, bid_price(0), ask_price(0), bid_size(0), ask_size(0),
                        bid_levels(0), ask_levels(0), exchange_id(0), quote_condition(0), padding{} {}
        MarketQuote(Timestamp ts, const Symbol &sym, Price bp, Price ap, Quantity bs, Quantity as)
            : timestamp(ts), symbol(sym), bid_price(bp), ask_price(ap), bid_size(bs), ask_size(as),
              bid_levels(1), ask_levels(1), exchange_id(0), quote_condition(0), padding{} {}
    };

    struct alignas(64) OrderBookLevel
    {
        Price price;
        Quantity quantity;
        uint32_t order_count;
        uint32_t padding;

        OrderBookLevel() : price(0), quantity(0), order_count(0), padding(0) {}
        OrderBookLevel(Price p, Quantity q, uint32_t count = 1)
            : price(p), quantity(q), order_count(count), padding(0) {}
    };

    struct alignas(64) MarketStatistics
    {
        Symbol symbol;
        Timestamp last_update;
        Price last_price;
        Price high_price;
        Price low_price;
        Price open_price;
        Price vwap; // Volume-weighted average price
        Quantity total_volume;
        uint64_t trade_count;
        double volatility; // Realized volatility
        Price bid_ask_spread;
        uint32_t padding[2];

        MarketStatistics() : symbol{}, last_update{}, last_price(0), high_price(0), low_price(0),
                             open_price(0), vwap(0), total_volume(0), trade_count(0), volatility(0.0),
                             bid_ask_spread(0), padding{} {}
        void update_trade(Price price, Quantity quantity);
        void update_quote(Price bid, Price ask);
        double calculate_volatility() const;
    };

    struct alignas(64) MarketDataMessage
    {
        uint64_t sequence_number;
        Timestamp receive_timestamp;
        Timestamp exchange_timestamp;
        MessageType type;
        uint8_t padding[7];

        MarketTrade trade_data;
        MarketQuote quote_data;

        MarketDataMessage() : sequence_number(0), receive_timestamp{}, exchange_timestamp{},
                              type(MessageType::TRADE), padding{}, trade_data{}, quote_data{} {}

        MarketDataMessage(MessageType t) : sequence_number(0), receive_timestamp{}, exchange_timestamp{},
                                           type(t), padding{}, trade_data{}, quote_data{} {}
    };

    struct alignas(64) PerformanceMetrics
    {
        std::atomic<uint64_t> messages_processed{0};
        std::atomic<uint64_t> messages_per_second{0};
        std::atomic<uint64_t> avg_latency_ns{0};
        std::atomic<uint64_t> max_latency_ns{0};
        std::atomic<uint64_t> queue_depth{0};
        std::atomic<uint64_t> memory_usage_bytes{0};

        void update_latency(uint64_t latency_ns);
        void reset_counters();
    };

    inline Symbol make_symbol(const std::string &str)
    {
        Symbol sym{};
        const size_t len = std::min(str.length(), sym.size() - 1);
        std::copy_n(str.begin(), len, sym.begin());
        return sym;
    }

    inline std::string symbol_to_string(const Symbol &sym)
    {
        return std::string(sym.data(), strnlen(sym.data(), sym.size()));
    }

    inline Timestamp now()
    {
        return std::chrono::high_resolution_clock::now();
    }

    inline uint64_t duration_ns(Timestamp start, Timestamp end)
    {
        return std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();
    }

} // namespace market_data