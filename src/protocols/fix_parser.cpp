#include "protocols/fix_parser.hpp"
#include <charconv>
#include <algorithm>
#include <cstring>
#include <sstream>
#include <iomanip>

namespace market_data {

FixParser::FixParser() {
    // Initialize tag cache
    std::fill(tag_cache_.begin(), tag_cache_.end(), std::string_view{});
}

bool FixParser::parse_message(std::string_view message, Timestamp receive_time) {
    const auto start_time = now();
    
    reset();
    
    // Validate basic FIX structure
    if (message.length() < 20 || !message.starts_with("8=FIX")) {
        parse_errors_.fetch_add(1, std::memory_order_relaxed);
        return false;
    }
    
    // Parse fields
    if (!parse_fields(message)) {
        parse_errors_.fetch_add(1, std::memory_order_relaxed);
        return false;
    }
    
    // Build tag cache for fast lookup
    build_tag_cache();
    
    // Update performance metrics
    const auto parse_time = duration_ns(start_time, now());
    messages_parsed_.fetch_add(1, std::memory_order_relaxed);
    total_parse_time_ns_.fetch_add(parse_time, std::memory_order_relaxed);
    
    return true;
}

bool FixParser::parse_fields(std::string_view message) {
    size_t pos = 0;
    field_count_ = 0;
    
    while (pos < message.length() && field_count_ < MAX_FIELDS) {
        // Find tag separator
        const size_t eq_pos = message.find('=', pos);
        if (eq_pos == std::string_view::npos) break;
        
        // Parse tag number
        const auto tag_str = message.substr(pos, eq_pos - pos);
        const auto tag_opt = fast_parse_int(tag_str);
        if (!tag_opt) break;
        
        // Find value separator (SOH = 0x01)
        const size_t soh_pos = message.find('\x01', eq_pos + 1);
        if (soh_pos == std::string_view::npos) break;
        
        // Extract value
        const auto value = message.substr(eq_pos + 1, soh_pos - eq_pos - 1);
        
        // Store field
        fields_[field_count_++] = FixField(static_cast<FixTag>(tag_opt.value()), value);
        
        pos = soh_pos + 1;
    }
    
    return field_count_ > 0;
}

void FixParser::build_tag_cache() {
    // Clear cache
    std::fill(tag_cache_.begin(), tag_cache_.end(), std::string_view{});
    
    // Build cache for fast O(1) field lookup
    for (size_t i = 0; i < field_count_; ++i) {
        const auto tag_num = static_cast<size_t>(fields_[i].tag);
        if (tag_num < tag_cache_.size()) {
            tag_cache_[tag_num] = fields_[i].value;
        }
    }
}

std::optional<std::string_view> FixParser::get_field(FixTag tag) const {
    const auto tag_num = static_cast<size_t>(tag);
    if (tag_num < tag_cache_.size() && !tag_cache_[tag_num].empty()) {
        return tag_cache_[tag_num];
    }
    return std::nullopt;
}

std::optional<int64_t> FixParser::get_int_field(FixTag tag) const {
    const auto field = get_field(tag);
    return field ? fast_parse_int(*field) : std::nullopt;
}

std::optional<double> FixParser::get_double_field(FixTag tag) const {
    const auto field = get_field(tag);
    return field ? fast_parse_double(*field) : std::nullopt;
}

std::optional<Price> FixParser::get_price_field(FixTag tag) const {
    const auto field = get_field(tag);
    if (!field) return std::nullopt;
    
    const auto price_double = fast_parse_double(*field);
    return price_double ? std::make_optional(from_double(*price_double)) : std::nullopt;
}

std::optional<Quantity> FixParser::get_quantity_field(FixTag tag) const {
    const auto field = get_field(tag);
    if (!field) return std::nullopt;
    
    const auto qty = fast_parse_int(*field);
    return qty && *qty >= 0 ? std::make_optional(static_cast<Quantity>(*qty)) : std::nullopt;
}

std::optional<Timestamp> FixParser::get_timestamp_field(FixTag tag) const {
    const auto field = get_field(tag);
    return field ? parse_fix_timestamp(*field) : std::nullopt;
}

std::optional<FixMsgType> FixParser::get_message_type() const {
    const auto field = get_field(FixTag::MSG_TYPE);
    if (!field || field->empty()) {
        return std::nullopt;
    }
    return static_cast<FixMsgType>(field->front());
}

std::optional<MarketTrade> FixParser::to_trade(Timestamp receive_time) const {
    const auto msg_type = get_message_type();
    if (!msg_type || (*msg_type != FixMsgType::MARKET_DATA_INCREMENTAL_REFRESH && 
                      *msg_type != FixMsgType::MARKET_DATA_SNAPSHOT)) {
        return std::nullopt;
    }
    
    const auto symbol_field = get_field(FixTag::SYMBOL);
    const auto price = get_price_field(FixTag::LAST_PX);
    const auto quantity = get_quantity_field(FixTag::LAST_QTY);
    
    if (!symbol_field || !price || !quantity) {
        return std::nullopt;
    }
    
    MarketTrade trade;
    trade.timestamp = receive_time;
    trade.symbol = make_symbol(std::string(*symbol_field));
    trade.price = *price;
    trade.quantity = *quantity;
    trade.aggressor_side = Side::BUY;  // Default, would need additional logic
    trade.trade_id = static_cast<uint32_t>(messages_parsed_.load());
    
    return trade;
}

std::optional<MarketQuote> FixParser::to_quote(Timestamp receive_time) const {
    const auto msg_type = get_message_type();
    if (!msg_type || *msg_type != FixMsgType::MARKET_DATA_SNAPSHOT) {
        return std::nullopt;
    }
    
    const auto symbol_field = get_field(FixTag::SYMBOL);
    const auto bid_price = get_price_field(FixTag::BID_PX);
    const auto ask_price = get_price_field(FixTag::OFFER_PX);
    const auto bid_size = get_quantity_field(FixTag::BID_SIZE);
    const auto ask_size = get_quantity_field(FixTag::OFFER_SIZE);
    
    if (!symbol_field || !bid_price || !ask_price || !bid_size || !ask_size) {
        return std::nullopt;
    }
    
    MarketQuote quote;
    quote.timestamp = receive_time;
    quote.symbol = make_symbol(std::string(*symbol_field));
    quote.bid_price = *bid_price;
    quote.ask_price = *ask_price;
    quote.bid_size = *bid_size;
    quote.ask_size = *ask_size;
    
    return quote;
}

std::optional<MarketDataMessage> FixParser::to_market_data_message(Timestamp receive_time) const {
    const auto msg_type = get_message_type();
    if (!msg_type) return std::nullopt;
    
    // Try to convert to trade first
    if (const auto trade = to_trade(receive_time)) {
        MarketDataMessage message(MessageType::TRADE);
        message.receive_timestamp = receive_time;
        message.exchange_timestamp = get_timestamp_field(FixTag::SENDING_TIME).value_or(receive_time);
        message.trade_data = *trade;
        return message;
    }
    
    // Try to convert to quote
    if (const auto quote = to_quote(receive_time)) {
        MarketDataMessage message(MessageType::QUOTE);
        message.receive_timestamp = receive_time;
        message.exchange_timestamp = get_timestamp_field(FixTag::SENDING_TIME).value_or(receive_time);
        message.quote_data = *quote;
        return message;
    }
    
    return std::nullopt;
}

bool FixParser::validate_checksum(std::string_view message) const {
    // Simplified checksum validation for demo
    return true;  // In production, implement full checksum validation
}

bool FixParser::validate_message_structure() const {
    // Must have BeginString, BodyLength, MsgType, and Checksum
    return get_field(FixTag::BEGIN_STRING).has_value() &&
           get_field(FixTag::BODY_LENGTH).has_value() &&
           get_field(FixTag::MSG_TYPE).has_value() &&
           field_count_ >= 4;
}

double FixParser::get_average_parse_time_ns() const {
    const uint64_t count = messages_parsed_.load();
    const uint64_t total = total_parse_time_ns_.load();
    return count > 0 ? static_cast<double>(total) / count : 0.0;
}

void FixParser::reset() {
    field_count_ = 0;
    std::fill(tag_cache_.begin(), tag_cache_.end(), std::string_view{});
}

uint8_t FixParser::calculate_checksum(std::string_view message) const {
    uint32_t sum = 0;
    for (const char c : message) {
        sum += static_cast<uint8_t>(c);
    }
    return static_cast<uint8_t>(sum % 256);
}

std::optional<int64_t> FixParser::fast_parse_int(std::string_view str) const {
    if (str.empty()) return std::nullopt;
    
    int64_t result = 0;
    auto [ptr, ec] = std::from_chars(str.data(), str.data() + str.size(), result);
    
    return (ec == std::errc{}) ? std::make_optional(result) : std::nullopt;
}

std::optional<double> FixParser::fast_parse_double(std::string_view str) const {
    if (str.empty()) return std::nullopt;
    
    try {
        // Use string stream for double parsing (from_chars doesn't support double on all platforms)
        std::string temp(str);
        std::istringstream iss(temp);
        double result;
        if (iss >> result) {
            return result;
        }
    } catch (...) {
        // Fall through to nullopt
    }
    
    return std::nullopt;
}

std::optional<Timestamp> FixParser::parse_fix_timestamp(std::string_view str) const {
    // Simplified timestamp parsing for demo
    return now();  // In production, implement proper FIX timestamp parsing
}

// FixMessageBuilder implementation
FixMessageBuilder::FixMessageBuilder(const std::string& sender, const std::string& target)
    : seq_num_(1), sender_comp_id_(sender), target_comp_id_(target) {
    buffer_.reserve(1024);
}

std::string FixMessageBuilder::create_logon_message() {
    buffer_.clear();
    add_header(FixMsgType::LOGON);
    add_field(FixTag::SENDER_COMP_ID, sender_comp_id_);
    add_field(FixTag::TARGET_COMP_ID, target_comp_id_);
    add_field(FixTag::SENDING_TIME, get_timestamp());
    finalize_message();
    return buffer_;
}

std::string FixMessageBuilder::create_market_data_request(const std::vector<std::string>& symbols, int depth) {
    buffer_.clear();
    add_header(FixMsgType::MARKET_DATA_REQUEST);
    add_field(FixTag::SENDER_COMP_ID, sender_comp_id_);
    add_field(FixTag::TARGET_COMP_ID, target_comp_id_);
    add_field(FixTag::MD_REQ_ID, "MDR" + std::to_string(seq_num_));
    add_field(FixTag::SUBSCRIPTION_REQUEST_TYPE, static_cast<int64_t>(1));  // Explicit cast
    add_field(FixTag::MARKET_DEPTH, static_cast<int64_t>(depth));  // Explicit cast
    
    // Add symbols
    for (const auto& symbol : symbols) {
        add_field(FixTag::SYMBOL, symbol);
    }
    
    add_field(FixTag::SENDING_TIME, get_timestamp());
    finalize_message();
    return buffer_;
}

std::string FixMessageBuilder::create_heartbeat() {
    buffer_.clear();
    add_header(FixMsgType::HEARTBEAT);
    add_field(FixTag::SENDER_COMP_ID, sender_comp_id_);
    add_field(FixTag::TARGET_COMP_ID, target_comp_id_);
    add_field(FixTag::SENDING_TIME, get_timestamp());
    finalize_message();
    return buffer_;
}

std::string FixMessageBuilder::create_test_request(const std::string& test_req_id) {
    buffer_.clear();
    add_header(FixMsgType::TEST_REQUEST);
    add_field(FixTag::SENDER_COMP_ID, sender_comp_id_);
    add_field(FixTag::TARGET_COMP_ID, target_comp_id_);
    add_field(static_cast<FixTag>(112), test_req_id);  // TestReqID
    add_field(FixTag::SENDING_TIME, get_timestamp());
    finalize_message();
    return buffer_;
}

void FixMessageBuilder::add_header(FixMsgType msg_type) {
    add_field(FixTag::BEGIN_STRING, "FIX.4.4");
    // Body length will be calculated and inserted later
    add_field(FixTag::MSG_TYPE, std::string(1, static_cast<char>(msg_type)));
    add_field(FixTag::MSG_SEQ_NUM, static_cast<int64_t>(seq_num_++));  // Explicit cast
}

void FixMessageBuilder::add_field(FixTag tag, const std::string& value) {
    buffer_ += std::to_string(static_cast<uint32_t>(tag));
    buffer_ += '=';
    buffer_ += value;
    buffer_ += '\x01';
}

void FixMessageBuilder::add_field(FixTag tag, int64_t value) {
    add_field(tag, std::to_string(value));
}

void FixMessageBuilder::add_field(FixTag tag, double value, int precision) {
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(precision) << value;
    add_field(tag, oss.str());
}

void FixMessageBuilder::finalize_message() {
    // Simplified finalization for demo
    const uint8_t checksum = calculate_checksum();
    buffer_ += "10=";
    
    // Format checksum as 3-digit string
    std::ostringstream oss;
    oss << std::setfill('0') << std::setw(3) << static_cast<int>(checksum);
    buffer_ += oss.str();
    buffer_ += '\x01';
}

std::string FixMessageBuilder::get_timestamp() const {
    const auto now = std::chrono::system_clock::now();
    const auto time_t = std::chrono::system_clock::to_time_t(now);
    const auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()) % 1000;
    
    std::ostringstream oss;
    oss << std::put_time(std::gmtime(&time_t), "%Y%m%d-%H:%M:%S");
    oss << '.' << std::setfill('0') << std::setw(3) << ms.count();
    
    return oss.str();
}

uint8_t FixMessageBuilder::calculate_checksum() const {
    uint32_t sum = 0;
    for (const char c : buffer_) {
        sum += static_cast<uint8_t>(c);
    }
    return static_cast<uint8_t>(sum % 256);
}

} // namespace market_data 