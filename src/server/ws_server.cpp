#include <iostream>
#include <thread>
#include <chrono>
#include <atomic>
#include <memory>
#include <string>
#include <vector>
#include <map>
#include <set>
#include <mutex>
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <nlohmann/json.hpp>

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#include <poll.h>

#include <openssl/sha.h>
#include <openssl/bio.h>
#include <openssl/evp.h>
#include <openssl/buffer.h>

#include "core/market_data_types.hpp"
#include "core/lock_free_queue.hpp"
#include "feed_handler/order_book.hpp"

namespace market_data
{

    enum class WSOpcode : uint8_t
    {
        CONTINUATION = 0x0,
        TEXT = 0x1,
        BINARY = 0x2,
        CLOSE = 0x8,
        PING = 0x9,
        PONG = 0xA
    };

    class SimpleWebSocketServer
    {
    private:
        int server_fd_;
        int port_;
        std::atomic<bool> running_{false};
        std::thread server_thread_;
        std::thread broadcast_thread_;

        std::set<int> client_sockets_;
        std::mutex clients_mutex_;

        std::unique_ptr<MarketDataQueue> queue_;
        std::unique_ptr<MarketDataAggregator> aggregator_;
        std::thread data_generator_thread_;

        std::atomic<uint64_t> messages_sent_{0};
        std::atomic<uint64_t> clients_connected_{0};

    public:
        explicit SimpleWebSocketServer(int port = 9001) : port_(port)
        {
            std::cout << "🌐 Initializing WebSocket Server on port " << port << "\n";

            queue_ = std::make_unique<MarketDataQueue>();
            aggregator_ = std::make_unique<MarketDataAggregator>(*queue_);
        }

        ~SimpleWebSocketServer()
        {
            stop();
        }

        bool start()
        {

            server_fd_ = socket(AF_INET, SOCK_STREAM, 0);
            if (server_fd_ < 0)
            {
                std::cerr << "❌ Failed to create socket\n";
                return false;
            }

            int opt = 1;
            if (setsockopt(server_fd_, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0)
            {
                std::cerr << "❌ Failed to set socket options\n";
                return false;
            }

            sockaddr_in address{};
            address.sin_family = AF_INET;
            address.sin_addr.s_addr = INADDR_ANY;
            address.sin_port = htons(port_);

            if (bind(server_fd_, (struct sockaddr *)&address, sizeof(address)) < 0)
            {
                std::cerr << "❌ Failed to bind to port " << port_ << "\n";
                return false;
            }

            if (listen(server_fd_, 10) < 0)
            {
                std::cerr << "❌ Failed to listen\n";
                return false;
            }

            fcntl(server_fd_, F_SETFL, O_NONBLOCK);

            running_.store(true);

            aggregator_->start();

            server_thread_ = std::thread(&SimpleWebSocketServer::accept_loop, this);

            broadcast_thread_ = std::thread(&SimpleWebSocketServer::broadcast_loop, this);

            data_generator_thread_ = std::thread(&SimpleWebSocketServer::generate_market_data, this);

            std::cout << "✅ WebSocket server started on ws://localhost:" << port_ << "\n";
            std::cout << "📊 Generating market data...\n";

            return true;
        }

        void stop()
        {
            if (!running_.exchange(false))
                return;

            std::cout << "\n🛑 Stopping WebSocket server...\n";

            {
                std::lock_guard<std::mutex> lock(clients_mutex_);
                for (int client : client_sockets_)
                {
                    close(client);
                }
                client_sockets_.clear();
            }

            if (server_fd_ >= 0)
            {
                close(server_fd_);
            }

            if (aggregator_)
            {
                aggregator_->stop();
            }

            if (server_thread_.joinable())
                server_thread_.join();
            if (broadcast_thread_.joinable())
                broadcast_thread_.join();
            if (data_generator_thread_.joinable())
                data_generator_thread_.join();

            std::cout << "✅ WebSocket server stopped\n";
            std::cout << "📊 Total messages sent: " << messages_sent_.load() << "\n";
        }

    private:
        void accept_loop()
        {
            while (running_.load())
            {
                sockaddr_in client_addr{};
                socklen_t client_len = sizeof(client_addr);

                int client_fd = accept(server_fd_, (struct sockaddr *)&client_addr, &client_len);
                if (client_fd < 0)
                {
                    std::this_thread::sleep_for(std::chrono::milliseconds(10));
                    continue;
                }

                if (handle_handshake(client_fd))
                {
                    std::lock_guard<std::mutex> lock(clients_mutex_);
                    client_sockets_.insert(client_fd);
                    clients_connected_.fetch_add(1);

                    std::cout << "👤 Client connected from "
                              << inet_ntoa(client_addr.sin_addr)
                              << " (total: " << clients_connected_.load() << ")\n";

                    send_welcome_message(client_fd);
                }
                else
                {
                    close(client_fd);
                }
            }
        }

        bool handle_handshake(int client_fd)
        {
            char buffer[4096];
            ssize_t bytes = recv(client_fd, buffer, sizeof(buffer), 0);
            if (bytes <= 0)
                return false;

            std::string request(buffer, bytes);

            size_t key_pos = request.find("Sec-WebSocket-Key: ");
            if (key_pos == std::string::npos)
                return false;

            key_pos += 19;
            size_t key_end = request.find("\r\n", key_pos);
            std::string ws_key = request.substr(key_pos, key_end - key_pos);

            std::string magic = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";
            std::string accept_key = base64_encode(sha1(ws_key + magic));

            std::string response =
                "HTTP/1.1 101 Switching Protocols\r\n"
                "Upgrade: websocket\r\n"
                "Connection: Upgrade\r\n"
                "Sec-WebSocket-Accept: " +
                accept_key + "\r\n"
                             "Access-Control-Allow-Origin: *\r\n"
                             "\r\n";

            send(client_fd, response.c_str(), response.length(), 0);
            return true;
        }

        std::string sha1(const std::string &input)
        {
            unsigned char hash[SHA_DIGEST_LENGTH];
            SHA1((unsigned char *)input.c_str(), input.length(), hash);
            return std::string((char *)hash, SHA_DIGEST_LENGTH);
        }

        std::string base64_encode(const std::string &input)
        {
            BIO *bmem, *b64;
            BUF_MEM *bptr;

            b64 = BIO_new(BIO_f_base64());
            bmem = BIO_new(BIO_s_mem());
            b64 = BIO_push(b64, bmem);

            BIO_set_flags(b64, BIO_FLAGS_BASE64_NO_NL);
            BIO_write(b64, input.c_str(), input.length());
            BIO_flush(b64);
            BIO_get_mem_ptr(b64, &bptr);

            std::string result(bptr->data, bptr->length);
            BIO_free_all(b64);

            return result;
        }

        void send_welcome_message(int client_fd)
        {
            nlohmann::json welcome;
            welcome["type"] = "welcome";
            welcome["message"] = "Connected to Market Data Feed";
            welcome["timestamp"] = std::chrono::duration_cast<std::chrono::milliseconds>(
                                       std::chrono::system_clock::now().time_since_epoch())
                                       .count();

            send_ws_frame(client_fd, welcome.dump());
        }

        void broadcast_loop()
        {
            while (running_.load())
            {

                nlohmann::json update = create_market_update();
                std::string message = update.dump();

                std::lock_guard<std::mutex> lock(clients_mutex_);
                auto it = client_sockets_.begin();
                while (it != client_sockets_.end())
                {
                    if (!send_ws_frame(*it, message))
                    {

                        close(*it);
                        it = client_sockets_.erase(it);
                        clients_connected_.fetch_sub(1);
                        std::cout << "👋 Client disconnected (total: " << clients_connected_.load() << ")\n";
                    }
                    else
                    {
                        ++it;
                        messages_sent_.fetch_add(1);
                    }
                }

                std::this_thread::sleep_for(std::chrono::milliseconds(50)); // 20 FPS
            }
        }

        nlohmann::json create_market_update()
        {
            nlohmann::json update;
            update["type"] = "market_update";
            update["timestamp"] = std::chrono::duration_cast<std::chrono::milliseconds>(
                                      std::chrono::system_clock::now().time_since_epoch())
                                      .count();
            update["server_timestamp"] = update["timestamp"];

            auto &book_manager = aggregator_->get_book_manager();
            auto symbols = book_manager.get_active_symbols();

            nlohmann::json symbols_array = nlohmann::json::array();

            for (const auto &symbol_str : symbols)
            {
                Symbol sym{};
                std::copy(symbol_str.begin(), symbol_str.end(), sym.begin());

                auto *book = book_manager.get_book(sym);
                if (book)
                {
                    nlohmann::json symbol_data;
                    symbol_data["symbol"] = symbol_str;
                    symbol_data["bid_price"] = to_double(book->get_best_bid());
                    symbol_data["ask_price"] = to_double(book->get_best_ask());
                    symbol_data["last_price"] = (symbol_data["bid_price"].get<double>() +
                                                 symbol_data["ask_price"].get<double>()) /
                                                2.0;
                    symbol_data["spread"] = to_double(book->get_spread());

                    const auto &stats = book->get_statistics();
                    symbol_data["volume"] = stats.total_volume;
                    symbol_data["trade_count"] = stats.trade_count;
                    symbol_data["high_price"] = to_double(stats.high_price);
                    symbol_data["low_price"] = to_double(stats.low_price);
                    symbol_data["open_price"] = to_double(stats.open_price);
                    symbol_data["vwap"] = to_double(stats.vwap);

                    if (stats.open_price > 0)
                    {
                        double change = (symbol_data["last_price"].get<double>() - to_double(stats.open_price)) /
                                        to_double(stats.open_price) * 100.0;
                        symbol_data["change_percent"] = change;
                    }
                    else
                    {
                        symbol_data["change_percent"] = 0.0;
                    }

                    symbol_data["bid_size"] = 1000 + rand() % 5000;
                    symbol_data["ask_size"] = 1000 + rand() % 5000;

                    symbols_array.push_back(symbol_data);
                }
            }

            update["symbols"] = symbols_array;
            update["total_messages"] = aggregator_->get_metrics().messages_processed.load();

            nlohmann::json perf;
            perf["messages_per_second"] = 1000 + rand() % 9000;
            perf["avg_latency_ms"] = 0.1 + (rand() % 10) / 10.0;
            perf["memory_usage_mb"] = 50 + rand() % 150;
            update["performance"] = perf;

            return update;
        }

        bool send_ws_frame(int client_fd, const std::string &data)
        {
            std::vector<uint8_t> frame;

            frame.push_back(0x81);

            if (data.length() < 126)
            {
                frame.push_back(static_cast<uint8_t>(data.length()));
            }
            else if (data.length() < 65536)
            {
                frame.push_back(126);
                frame.push_back((data.length() >> 8) & 0xFF);
                frame.push_back(data.length() & 0xFF);
            }
            else
            {

                return false;
            }

            frame.insert(frame.end(), data.begin(), data.end());

            ssize_t sent = send(client_fd, frame.data(), frame.size(), MSG_NOSIGNAL);
            return sent == static_cast<ssize_t>(frame.size());
        }

        void generate_market_data()
        {
            const std::vector<std::string> symbols = {
                "AAPL", "GOOGL", "MSFT", "AMZN", "TSLA",
                "JPM", "BAC", "GS", "MS", "C"};

            std::vector<double> base_prices = {
                150.0, 2800.0, 300.0, 3300.0, 800.0,
                140.0, 30.0, 350.0, 80.0, 60.0};

            size_t message_count = 0;

            while (running_.load())
            {
                for (size_t i = 0; i < symbols.size(); ++i)
                {

                    MarketDataMessage msg(MessageType::TRADE);
                    msg.sequence_number = ++message_count;
                    msg.receive_timestamp = std::chrono::high_resolution_clock::now();
                    msg.exchange_timestamp = msg.receive_timestamp;

                    Symbol sym{};
                    std::copy(symbols[i].begin(), symbols[i].end(), sym.begin());

                    double price_change = (rand() % 100 - 50) / 100.0;
                    double new_price = base_prices[i] + price_change;
                    base_prices[i] = new_price;

                    msg.trade_data = MarketTrade(
                        msg.receive_timestamp,
                        sym,
                        from_double(new_price),
                        100 + rand() % 1000,
                        (rand() % 2) ? Side::BUY : Side::SELL,
                        message_count);

                    queue_->enqueue(std::move(msg));

                    MarketDataMessage quote_msg(MessageType::QUOTE);
                    quote_msg.sequence_number = ++message_count;
                    quote_msg.receive_timestamp = std::chrono::high_resolution_clock::now();
                    quote_msg.exchange_timestamp = quote_msg.receive_timestamp;

                    double spread = 0.01 + (rand() % 10) / 1000.0;
                    quote_msg.quote_data = MarketQuote(
                        quote_msg.receive_timestamp,
                        sym,
                        from_double(new_price - spread / 2),
                        from_double(new_price + spread / 2),
                        1000 + rand() % 5000,
                        1000 + rand() % 5000);

                    queue_->enqueue(std::move(quote_msg));
                }

                std::this_thread::sleep_for(std::chrono::milliseconds(100));
            }
        }
    };

} // namespace market_data

int main()
{
    std::cout << R"(
╔══════════════════════════════════════════╗
║      MARKET DATA WEBSOCKET SERVER        ║
║         Real-time Data Streaming         ║
╚══════════════════════════════════════════╝
)" << std::endl;

    market_data::SimpleWebSocketServer server(9001);

    if (!server.start())
    {
        std::cerr << "❌ Failed to start WebSocket server\n";
        return 1;
    }

    std::cout << "\n🔗 WebSocket endpoint: ws://localhost:9001\n";
    std::cout << "📱 Connect your web dashboard to see real-time data\n";
    std::cout << "⌨️  Press Ctrl+C to stop\n\n";

    std::atomic<bool> running{true};
    signal(SIGINT, [](int)
           { 
        std::cout << "\n⚠️  Shutting down...\n";
        exit(0); });

    while (running.load())
    {
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }

    return 0;
}