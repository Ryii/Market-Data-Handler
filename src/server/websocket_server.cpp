#include <iostream>
#include <thread>
#include <chrono>
#include <set>
#include <mutex>
#include <atomic>
#include <websocketpp/config/asio_no_tls.hpp>
#include <websocketpp/server.hpp>
#include <nlohmann/json.hpp>
#include "core/market_data_types.hpp"
#include "feed_handler/order_book.hpp"

namespace market_data {

using WebSocketServer = websocketpp::server<websocketpp::config::asio>;
using ConnectionPtr = WebSocketServer::connection_ptr;
using MessagePtr = WebSocketServer::message_ptr;
using ConnectionHdl = websocketpp::connection_hdl;

class MarketDataWebSocketServer {
private:
    WebSocketServer server_;
    std::thread server_thread_;
    std::thread broadcast_thread_;
    
    // Client management
    std::set<ConnectionHdl, std::owner_less<ConnectionHdl>> connections_;
    std::mutex connections_mutex_;
    
    // Market data
    OrderBookManager& book_manager_;
    std::atomic<bool> running_{false};
    
    // Broadcasting settings
    std::chrono::milliseconds broadcast_interval_{50};  // 20 FPS
    std::atomic<uint64_t> messages_sent_{0};
    std::atomic<uint64_t> clients_connected_{0};
    
    // Subscription management
    std::map<ConnectionHdl, std::set<std::string>, std::owner_less<ConnectionHdl>> client_subscriptions_;
    std::mutex subscriptions_mutex_;
    
public:
    explicit MarketDataWebSocketServer(OrderBookManager& manager, uint16_t port = 9001)
        : book_manager_(manager) {
        
        // Configure server
        server_.set_access_channels(websocketpp::log::alevel::all);
        server_.clear_access_channels(websocketpp::log::alevel::frame_payload);
        server_.set_error_channels(websocketpp::log::elevel::all);
        
        // Initialize Asio
        server_.init_asio();
        server_.set_reuse_addr(true);
        
        // Set handlers
        server_.set_message_handler([this](ConnectionHdl hdl, MessagePtr msg) {
            on_message(hdl, msg);
        });
        
        server_.set_open_handler([this](ConnectionHdl hdl) {
            on_open(hdl);
        });
        
        server_.set_close_handler([this](ConnectionHdl hdl) {
            on_close(hdl);
        });
        
        server_.set_fail_handler([this](ConnectionHdl hdl) {
            on_fail(hdl);
        });
        
        // Listen on specified port
        server_.listen(port);
        server_.start_accept();
        
        std::cout << "🌐 WebSocket server configured on port " << port << "\n";
    }
    
    ~MarketDataWebSocketServer() {
        stop();
    }
    
    void start() {
        running_.store(true, std::memory_order_release);
        
        // Start server thread
        server_thread_ = std::thread([this]() {
            std::cout << "🚀 WebSocket server thread started\n";
            server_.run();
        });
        
        // Start broadcast thread
        broadcast_thread_ = std::thread([this]() {
            broadcast_loop();
        });
        
        std::cout << "✅ WebSocket server started successfully\n";
        std::cout << "🔗 Connect to: ws://localhost:9001\n";
        std::cout << "📱 Web dashboard: http://localhost:3000\n\n";
    }
    
    void stop() {
        if (!running_.load()) return;
        
        running_.store(false, std::memory_order_release);
        
        // Stop server
        server_.stop();
        
        // Join threads
        if (server_thread_.joinable()) {
            server_thread_.join();
        }
        if (broadcast_thread_.joinable()) {
            broadcast_thread_.join();
        }
        
        std::cout << "🛑 WebSocket server stopped\n";
    }
    
    size_t get_client_count() const {
        return clients_connected_.load();
    }
    
    uint64_t get_messages_sent() const {
        return messages_sent_.load();
    }
    
private:
    void on_open(ConnectionHdl hdl) {
        {
            std::lock_guard<std::mutex> lock(connections_mutex_);
            connections_.insert(hdl);
        }
        
        clients_connected_.fetch_add(1, std::memory_order_relaxed);
        std::cout << "👤 Client connected (total: " << clients_connected_.load() << ")\n";
        
        // Send welcome message with available symbols
        nlohmann::json welcome;
        welcome["type"] = "welcome";
        welcome["message"] = "Connected to Market Data Feed";
        welcome["available_symbols"] = book_manager_.get_active_symbols();
        welcome["timestamp"] = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::high_resolution_clock::now().time_since_epoch()).count();
        
        send_to_client(hdl, welcome.dump());
    }
    
    void on_close(ConnectionHdl hdl) {
        {
            std::lock_guard<std::mutex> lock(connections_mutex_);
            connections_.erase(hdl);
        }
        
        {
            std::lock_guard<std::mutex> lock(subscriptions_mutex_);
            client_subscriptions_.erase(hdl);
        }
        
        clients_connected_.fetch_sub(1, std::memory_order_relaxed);
        std::cout << "👋 Client disconnected (total: " << clients_connected_.load() << ")\n";
    }
    
    void on_fail(ConnectionHdl hdl) {
        std::cout << "❌ Client connection failed\n";
        on_close(hdl);
    }
    
    void on_message(ConnectionHdl hdl, MessagePtr msg) {
        try {
            const auto payload = msg->get_payload();
            auto json_msg = nlohmann::json::parse(payload);
            
            const std::string type = json_msg.value("type", "");
            
            if (type == "subscribe") {
                handle_subscription(hdl, json_msg);
            } else if (type == "unsubscribe") {
                handle_unsubscription(hdl, json_msg);
            } else if (type == "ping") {
                handle_ping(hdl);
            } else {
                std::cout << "⚠️ Unknown message type: " << type << "\n";
            }
            
        } catch (const std::exception& e) {
            std::cout << "❌ Error processing client message: " << e.what() << "\n";
        }
    }
    
    void handle_subscription(ConnectionHdl hdl, const nlohmann::json& msg) {
        const auto symbols = msg.value("symbols", std::vector<std::string>{});
        
        {
            std::lock_guard<std::mutex> lock(subscriptions_mutex_);
            auto& client_subs = client_subscriptions_[hdl];
            for (const auto& symbol : symbols) {
                client_subs.insert(symbol);
            }
        }
        
        // Send confirmation
        nlohmann::json response;
        response["type"] = "subscription_confirmed";
        response["symbols"] = symbols;
        response["timestamp"] = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::high_resolution_clock::now().time_since_epoch()).count();
        
        send_to_client(hdl, response.dump());
        
        std::cout << "📺 Client subscribed to " << symbols.size() << " symbols\n";
    }
    
    void handle_unsubscription(ConnectionHdl hdl, const nlohmann::json& msg) {
        const auto symbols = msg.value("symbols", std::vector<std::string>{});
        
        {
            std::lock_guard<std::mutex> lock(subscriptions_mutex_);
            auto it = client_subscriptions_.find(hdl);
            if (it != client_subscriptions_.end()) {
                for (const auto& symbol : symbols) {
                    it->second.erase(symbol);
                }
            }
        }
        
        std::cout << "📺 Client unsubscribed from " << symbols.size() << " symbols\n";
    }
    
    void handle_ping(ConnectionHdl hdl) {
        nlohmann::json pong;
        pong["type"] = "pong";
        pong["timestamp"] = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::high_resolution_clock::now().time_since_epoch()).count();
        
        send_to_client(hdl, pong.dump());
    }
    
    void broadcast_loop() {
        std::cout << "📡 Broadcast thread started\n";
        
        while (running_.load(std::memory_order_acquire)) {
            broadcast_market_data();
            std::this_thread::sleep_for(broadcast_interval_);
        }
        
        std::cout << "📡 Broadcast thread stopped\n";
    }
    
    void broadcast_market_data() {
        if (connections_.empty()) return;
        
        // Get market summary
        const auto market_summary = book_manager_.get_market_summary_json();
        auto summary_json = nlohmann::json::parse(market_summary);
        
        // Add metadata
        summary_json["type"] = "market_update";
        summary_json["server_timestamp"] = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::high_resolution_clock::now().time_since_epoch()).count();
        
        const std::string broadcast_message = summary_json.dump();
        
        // Broadcast to all connected clients
        std::lock_guard<std::mutex> conn_lock(connections_mutex_);
        std::lock_guard<std::mutex> sub_lock(subscriptions_mutex_);
        
        for (const auto& hdl : connections_) {
            // Check if client has subscriptions
            const auto sub_it = client_subscriptions_.find(hdl);
            if (sub_it == client_subscriptions_.end() || sub_it->second.empty()) {
                // Send to all clients without specific subscriptions
                send_to_client(hdl, broadcast_message);
            } else {
                // Send filtered data based on subscriptions
                auto filtered_data = filter_data_for_client(summary_json, sub_it->second);
                send_to_client(hdl, filtered_data.dump());
            }
        }
    }
    
    nlohmann::json filter_data_for_client(const nlohmann::json& market_data, 
                                         const std::set<std::string>& subscriptions) {
        nlohmann::json filtered = market_data;
        
        // Filter symbols array
        if (filtered.contains("symbols")) {
            nlohmann::json filtered_symbols = nlohmann::json::array();
            
            for (const auto& symbol_data : filtered["symbols"]) {
                const std::string symbol = symbol_data.value("symbol", "");
                if (subscriptions.count(symbol) > 0) {
                    filtered_symbols.push_back(symbol_data);
                }
            }
            
            filtered["symbols"] = filtered_symbols;
        }
        
        return filtered;
    }
    
    void send_to_client(ConnectionHdl hdl, const std::string& message) {
        try {
            server_.send(hdl, message, websocketpp::frame::opcode::text);
            messages_sent_.fetch_add(1, std::memory_order_relaxed);
        } catch (const std::exception& e) {
            std::cout << "❌ Error sending to client: " << e.what() << "\n";
        }
    }
};

} // namespace market_data

int main(int argc, char* argv[]) {
    std::cout << "🌐 Market Data WebSocket Server\n";
    std::cout << "===============================\n\n";
    
    try {
        // Create market data infrastructure
        market_data::MarketDataQueue queue;
        market_data::MarketDataAggregator aggregator(queue);
        market_data::MarketDataWebSocketServer ws_server(aggregator.get_book_manager());
        
        // Start services
        aggregator.start();
        ws_server.start();
        
        std::cout << "🎯 Services started successfully!\n";
        std::cout << "🔗 WebSocket endpoint: ws://localhost:9001\n";
        std::cout << "📊 Market data aggregator running\n";
        std::cout << "⌨️  Press Ctrl+C to stop\n\n";
        
        // Keep server running
        while (ws_server.get_client_count() >= 0) {  // Always true, but allows for future conditions
            std::this_thread::sleep_for(std::chrono::seconds(1));
            
            // Print periodic stats
            static auto last_stats = std::chrono::steady_clock::now();
            const auto now = std::chrono::steady_clock::now();
            
            if (now - last_stats > std::chrono::seconds(30)) {
                std::cout << "📊 Server Stats - Clients: " << ws_server.get_client_count() 
                          << ", Messages sent: " << ws_server.get_messages_sent()
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