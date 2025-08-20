#pragma once

#include <string_view>
#include <unordered_map>
#include <array>
#include <optional>
#include <iomanip>
#include <sstream>
#include "core/market_data_types.hpp"

namespace market_data {

// FIX protocol field tags
enum class FixTag : uint32_t {
    BEGIN_STRING = 8,
    BODY_LENGTH = 9,
    MSG_TYPE = 35,
    SENDER_COMP_ID = 49,
    TARGET_COMP_ID = 56,
    MSG_SEQ_NUM = 34,
    SENDING_TIME = 52,
    
    // Market data specific
    SYMBOL = 55,
    SECURITY_ID = 48,
    MD_REQ_ID = 262,
    SUBSCRIPTION_REQUEST_TYPE = 263,
    MARKET_DEPTH = 264,
    MD_UPDATE_TYPE = 265,
    
    // Quote/Trade fields
    BID_PX = 132,
    OFFER_PX = 133,
    BID_SIZE = 134,
    OFFER_SIZE = 135,
    LAST_PX = 31,
    LAST_QTY = 32,
    TRADE_DATE = 75,
    TRANSACT_TIME = 60,
    
    // Order book fields
    MD_ENTRY_TYPE = 269,
    MD_ENTRY_PX = 270,
    MD_ENTRY_SIZE = 271,
    MD_ENTRY_TIME = 273,
    MD_ENTRY_ID = 278,
    
    CHECKSUM = 10
};

// FIX message types
enum class FixMsgType : char {
    HEARTBEAT = '0',
    TEST_REQUEST = '1',
    LOGON = 'A',
    LOGOUT = '5',
    MARKET_DATA_REQUEST = 'V',
    MARKET_DATA_SNAPSHOT = 'W',
    MARKET_DATA_INCREMENTAL_REFRESH = 'X',
    SECURITY_LIST_REQUEST = 'x',
    SECURITY_LIST = 'y'
};

// Zero-allocation FIX field structure
struct FixField {
    FixTag tag;
    std::string_view value;
    
    FixField() : tag(FixTag::BEGIN_STRING), value{} {}
    FixField(FixTag t, std::string_view v) : tag(t), value(v) {}
};

// High-performance FIX message parser
class FixParser {
private:
    // Pre-allocated field storage to avoid allocations
    static constexpr size_t MAX_FIELDS = 256;
    std::array<FixField, MAX_FIELDS> fields_;
    size_t field_count_ = 0;
    
    // Field lookup cache for fast access
    std::array<std::string_view, 512> tag_cache_;  // Indexed by tag number
    
    // Performance metrics
    std::atomic<uint64_t> messages_parsed_{0};
    std::atomic<uint64_t> parse_errors_{0};
    std::atomic<uint64_t> total_parse_time_ns_{0};
    
public:
    FixParser();
    
    // Main parsing interface
    bool parse_message(std::string_view message, Timestamp receive_time);
    
    // Field access (after successful parse)
    std::optional<std::string_view> get_field(FixTag tag) const;
    std::optional<int64_t> get_int_field(FixTag tag) const;
    std::optional<double> get_double_field(FixTag tag) const;
    std::optional<Price> get_price_field(FixTag tag) const;
    std::optional<Quantity> get_quantity_field(FixTag tag) const;
    std::optional<Timestamp> get_timestamp_field(FixTag tag) const;
    
    // Message type detection
    std::optional<FixMsgType> get_message_type() const;
    
    // Convert to market data structures
    std::optional<MarketTrade> to_trade(Timestamp receive_time) const;
    std::optional<MarketQuote> to_quote(Timestamp receive_time) const;
    std::optional<MarketDataMessage> to_market_data_message(Timestamp receive_time) const;
    
    // Validation
    bool validate_checksum(std::string_view message) const;
    bool validate_message_structure() const;
    
    // Performance monitoring
    uint64_t get_messages_parsed() const { return messages_parsed_.load(); }
    uint64_t get_parse_errors() const { return parse_errors_.load(); }
    double get_average_parse_time_ns() const;
    
    // Reset for reuse
    void reset();
    
private:
    bool parse_fields(std::string_view message);
    void build_tag_cache();
    uint8_t calculate_checksum(std::string_view message) const;
    
    // Fast integer parsing without allocations
    std::optional<int64_t> fast_parse_int(std::string_view str) const;
    std::optional<double> fast_parse_double(std::string_view str) const;
    
    // FIX timestamp parsing (YYYYMMDD-HH:MM:SS.sss format)
    std::optional<Timestamp> parse_fix_timestamp(std::string_view str) const;
};

// FIX message builder for creating market data requests
class FixMessageBuilder {
private:
    std::string buffer_;
    uint32_t seq_num_;
    std::string sender_comp_id_;
    std::string target_comp_id_;
    
public:
    FixMessageBuilder(const std::string& sender, const std::string& target);
    
    // Message construction
    std::string create_logon_message();
    std::string create_market_data_request(const std::vector<std::string>& symbols, int depth = 10);
    std::string create_heartbeat();
    std::string create_test_request(const std::string& test_req_id);
    
    // Utilities
    void set_sequence_number(uint32_t seq) { seq_num_ = seq; }
    uint32_t get_next_sequence_number() { return ++seq_num_; }
    
private:
    void add_header(FixMsgType msg_type);
    void add_field(FixTag tag, const std::string& value);
    void add_field(FixTag tag, int64_t value);
    void add_field(FixTag tag, double value, int precision = 4);
    void finalize_message();
    
    std::string get_timestamp() const;
    uint8_t calculate_checksum() const;
};

} // namespace market_data 