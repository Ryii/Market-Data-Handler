#include <iostream>
#include <random>
#include <thread>
#include <chrono>
#include <vector>
#include <atomic>
#include <signal.h>
#include <iomanip>
#include "core/market_data_types.hpp"
#include "core/lock_free_queue.hpp"
#include "feed_handler/order_book.hpp"

namespace market_data
{

    class RealisticMarketSimulator
    {
    private:
        // Market state
        struct SymbolState
        {
            Symbol symbol;
            Price current_price;
            double volatility;
            double drift;
            Quantity daily_volume;
            std::mt19937 rng;
            std::normal_distribution<double> price_dist;
            std::exponential_distribution<double> arrival_dist;

            SymbolState(const Symbol &sym, Price initial_price, double vol = 0.02)
                : symbol(sym), current_price(initial_price), volatility(vol), drift(0.0001),
                  daily_volume(0), rng(std::random_device{}()),
                  price_dist(0.0, vol), arrival_dist(1000.0) {} // 1000 events/sec avg
        };

        std::vector<SymbolState> symbols_;
        MarketDataQueue &output_queue_;
        std::atomic<bool> running_{false};
        std::atomic<uint64_t> messages_generated_{0};
        std::atomic<uint64_t> trades_generated_{0};
        std::atomic<uint64_t> quotes_generated_{0};

        // Simulation parameters
        double trade_probability_ = 0.3;       // 30% trades, 70% quotes
        double market_hours_multiplier_ = 1.0; // Speed multiplier

    public:
        RealisticMarketSimulator(MarketDataQueue &queue) : output_queue_(queue)
        {
            initialize_symbols();
        }

        void start()
        {
            running_.store(true, std::memory_order_release);
            std::cout << "Starting realistic market data simulation...\n";
            std::cout << "Generating data for " << symbols_.size() << " symbols\n";
            std::cout << "⚡ Target: ~10,000 messages/second per symbol\n\n";
        }

        void stop()
        {
            running_.store(false, std::memory_order_release);
            std::cout << "\n📈 Simulation Statistics:\n";
            std::cout << "   Total Messages: " << messages_generated_.load() << "\n";
            std::cout << "   Trades: " << trades_generated_.load() << "\n";
            std::cout << "   Quotes: " << quotes_generated_.load() << "\n";
        }

        bool is_running() const { return running_.load(std::memory_order_acquire); }

        void run_simulation()
        {
            auto last_stats_time = now();
            uint64_t last_message_count = 0;

            while (running_.load(std::memory_order_acquire))
            {
                // Generate market data for each symbol
                for (auto &symbol_state : symbols_)
                {
                    generate_market_data(symbol_state);
                }

                // Print statistics every 5 seconds
                const auto current_time = now();
                if (duration_ns(last_stats_time, current_time) > 5'000'000'000ULL)
                { // 5 seconds
                    print_statistics(last_stats_time, last_message_count);
                    last_stats_time = current_time;
                    last_message_count = messages_generated_.load();
                }

                // Brief pause to control message rate
                std::this_thread::sleep_for(std::chrono::microseconds(10));
            }
        }

    private:
        void initialize_symbols()
        {
            // Major tech stocks
            symbols_.emplace_back(make_symbol("AAPL"), from_double(150.25), 0.025);
            symbols_.emplace_back(make_symbol("GOOGL"), from_double(2800.50), 0.030);
            symbols_.emplace_back(make_symbol("MSFT"), from_double(320.75), 0.022);
            symbols_.emplace_back(make_symbol("TSLA"), from_double(800.00), 0.045);
            symbols_.emplace_back(make_symbol("NVDA"), from_double(450.30), 0.040);

            // Financial stocks
            symbols_.emplace_back(make_symbol("JPM"), from_double(145.80), 0.028);
            symbols_.emplace_back(make_symbol("BAC"), from_double(35.60), 0.032);
            symbols_.emplace_back(make_symbol("GS"), from_double(380.25), 0.035);

            // Crypto pairs (higher volatility)
            symbols_.emplace_back(make_symbol("BTCUSD"), from_double(45000.00), 0.08);
            symbols_.emplace_back(make_symbol("ETHUSD"), from_double(3200.00), 0.10);

            std::cout << "📋 Initialized " << symbols_.size() << " symbols for simulation\n";
        }

        void generate_market_data(SymbolState &state)
        {
            // Determine message type
            std::uniform_real_distribution<double> type_dist(0.0, 1.0);
            const bool is_trade = type_dist(state.rng) < trade_probability_;

            if (is_trade)
            {
                generate_trade(state);
            }
            else
            {
                generate_quote(state);
            }
        }

        void generate_trade(SymbolState &state)
        {
            // Update price using geometric Brownian motion
            const double dt = 1.0 / (365.0 * 24.0 * 3600.0); // 1 second
            const double price_change = state.drift * dt + state.volatility * std::sqrt(dt) * state.price_dist(state.rng);

            state.current_price = std::max(Price(1),
                                           static_cast<Price>(state.current_price * (1.0 + price_change)));

            // Generate trade size (log-normal distribution)
            std::lognormal_distribution<double> size_dist(6.0, 1.5); // Mean ~400 shares
            const auto trade_size = static_cast<Quantity>(std::max(1.0, size_dist(state.rng)));

            // Random side
            std::uniform_int_distribution<int> side_dist(0, 1);
            const Side side = static_cast<Side>(side_dist(state.rng));

            // Create trade message
            MarketDataMessage msg(MessageType::TRADE);
            msg.receive_timestamp = now();
            msg.exchange_timestamp = msg.receive_timestamp;
            msg.sequence_number = messages_generated_.fetch_add(1);

            msg.trade_data = MarketTrade(
                msg.receive_timestamp,
                state.symbol,
                state.current_price,
                trade_size,
                side,
                static_cast<uint32_t>(trades_generated_.fetch_add(1)));

            // Add small price noise for realism
            std::uniform_real_distribution<double> noise_dist(-0.0001, 0.0001);
            const Price noise = static_cast<Price>(state.current_price * noise_dist(state.rng));
            msg.trade_data.price += noise;

            state.daily_volume += trade_size;

            // Enqueue (non-blocking)
            if (!output_queue_.enqueue(std::move(msg)))
            {
                // Queue full - in real system might use backpressure
            }
        }

        void generate_quote(SymbolState &state)
        {
            // Generate realistic bid-ask spread (0.01% to 0.1% of price)
            std::uniform_real_distribution<double> spread_dist(0.0001, 0.001);
            const Price spread = static_cast<Price>(state.current_price * spread_dist(state.rng));
            const Price half_spread = spread / 2;

            const Price bid_price = state.current_price - half_spread;
            const Price ask_price = state.current_price + half_spread;

            // Generate realistic bid/ask sizes
            std::lognormal_distribution<double> size_dist(7.0, 1.0); // Mean ~1100 shares
            const auto bid_size = static_cast<Quantity>(std::max(100.0, size_dist(state.rng)));
            const auto ask_size = static_cast<Quantity>(std::max(100.0, size_dist(state.rng)));

            // Create quote message
            MarketDataMessage msg(MessageType::QUOTE);
            msg.receive_timestamp = now();
            msg.exchange_timestamp = msg.receive_timestamp;
            msg.sequence_number = messages_generated_.fetch_add(1);

            msg.quote_data = MarketQuote(
                msg.receive_timestamp,
                state.symbol,
                bid_price,
                ask_price,
                bid_size,
                ask_size);

            quotes_generated_.fetch_add(1);

            // Enqueue
            if (!output_queue_.enqueue(std::move(msg)))
            {
                // Queue full
            }
        }

        void print_statistics(Timestamp last_time, uint64_t last_count)
        {
            const auto current_time = now();
            const auto elapsed_ns = duration_ns(last_time, current_time);
            const auto current_count = messages_generated_.load();

            const double elapsed_seconds = elapsed_ns / 1e9;
            const double messages_per_second = (current_count - last_count) / elapsed_seconds;

            std::cout << "Market Data Stats (last " << std::fixed << std::setprecision(1)
                      << elapsed_seconds << "s):\n";
            std::cout << "   Messages/sec: " << std::fixed << std::setprecision(0)
                      << messages_per_second << "\n";
            std::cout << "   Queue utilization: " << std::fixed << std::setprecision(1)
                      << output_queue_.utilization() * 100 << "%\n";
            std::cout << "   Dropped messages: " << output_queue_.dropped_count() << "\n";

            // Symbol-specific stats
            std::cout << "   Symbol prices: ";
            for (size_t i = 0; i < std::min(size_t(5), symbols_.size()); ++i)
            {
                std::cout << symbol_to_string(symbols_[i].symbol) << "=$"
                          << std::fixed << std::setprecision(2) << to_double(symbols_[i].current_price);
                if (i < std::min(size_t(4), symbols_.size() - 1))
                    std::cout << ", ";
            }
            std::cout << "\n\n";
        }
    };

} // namespace market_data

// Global simulation instance for signal handling
market_data::RealisticMarketSimulator *g_simulator = nullptr;

void signal_handler(int signal)
{
    if (g_simulator)
    {
        std::cout << "\nReceived signal " << signal << ", stopping simulation...\n";
        g_simulator->stop();
    }
}

int main(int argc, char *argv[])
{
    std::cout << "🏦 High-Frequency Market Data Simulator\n";
    std::cout << "========================================\n\n";

    // Set up signal handling
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);

    try
    {
        // Create market data infrastructure
        market_data::MarketDataQueue queue;
        market_data::MarketDataAggregator aggregator(queue);
        market_data::RealisticMarketSimulator simulator(queue);

        g_simulator = &simulator;

        // Start aggregator
        aggregator.start();
        std::cout << "Market data aggregator started\n";

        // Start simulator
        simulator.start();

        // Run simulation
        simulator.run_simulation();

        // Cleanup
        simulator.stop();
        aggregator.stop();

        std::cout << "🏁 Simulation completed successfully\n";

        // Print final performance metrics
        const auto &metrics = aggregator.get_metrics();
        std::cout << "\n📈 Final Performance Metrics:\n";
        std::cout << "   Messages processed: " << metrics.messages_processed.load() << "\n";
        std::cout << "   Average latency: " << std::fixed << std::setprecision(1)
                  << metrics.avg_latency_ns.load() << " ns\n";
        std::cout << "   Max latency: " << metrics.max_latency_ns.load() << " ns\n";
        std::cout << "   Active symbols: " << aggregator.get_book_manager().get_symbol_count() << "\n";
    }
    catch (const std::exception &e)
    {
        std::cerr << "Error: " << e.what() << "\n";
        return 1;
    }

    return 0;
}