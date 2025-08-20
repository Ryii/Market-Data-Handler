#pragma once

#include <atomic>
#include <memory>
#include <array>
#include "market_data_types.hpp"

namespace market_data {

// High-performance SPSC (Single Producer Single Consumer) lock-free queue
// Optimized for market data with cache-line alignment and memory ordering
template<typename T, size_t Size = 65536>
class SPSCQueue {
    static_assert((Size & (Size - 1)) == 0, "Size must be power of 2");
    
private:
    // Cache line size for alignment
    static constexpr size_t CACHE_LINE_SIZE = 64;
    
    // Separate cache lines for head and tail to avoid false sharing
    alignas(CACHE_LINE_SIZE) std::atomic<size_t> head_{0};
    alignas(CACHE_LINE_SIZE) std::atomic<size_t> tail_{0};
    
    // Ring buffer with cache-aligned elements
    alignas(CACHE_LINE_SIZE) std::array<T, Size> buffer_;
    
    static constexpr size_t mask_ = Size - 1;
    
public:
    SPSCQueue() = default;
    
    // Non-copyable, non-movable for safety
    SPSCQueue(const SPSCQueue&) = delete;
    SPSCQueue& operator=(const SPSCQueue&) = delete;
    SPSCQueue(SPSCQueue&&) = delete;
    SPSCQueue& operator=(SPSCQueue&&) = delete;
    
    // Producer side (single thread only)
    bool try_push(const T& item) noexcept {
        const size_t current_tail = tail_.load(std::memory_order_relaxed);
        const size_t next_tail = (current_tail + 1) & mask_;
        
        // Check if queue is full (relaxed load is sufficient)
        if (next_tail == head_.load(std::memory_order_acquire)) {
            return false;  // Queue full
        }
        
        // Store the item
        buffer_[current_tail] = item;
        
        // Make the item visible to consumer (release semantics)
        tail_.store(next_tail, std::memory_order_release);
        return true;
    }
    
    bool try_push(T&& item) noexcept {
        const size_t current_tail = tail_.load(std::memory_order_relaxed);
        const size_t next_tail = (current_tail + 1) & mask_;
        
        if (next_tail == head_.load(std::memory_order_acquire)) {
            return false;
        }
        
        buffer_[current_tail] = std::move(item);
        tail_.store(next_tail, std::memory_order_release);
        return true;
    }
    
    // Consumer side (single thread only)
    bool try_pop(T& item) noexcept {
        const size_t current_head = head_.load(std::memory_order_relaxed);
        
        // Check if queue is empty (acquire semantics to see producer's writes)
        if (current_head == tail_.load(std::memory_order_acquire)) {
            return false;  // Queue empty
        }
        
        // Load the item
        item = std::move(buffer_[current_head]);
        
        // Update head (release semantics for producer)
        head_.store((current_head + 1) & mask_, std::memory_order_release);
        return true;
    }
    
    // Queue status (can be called from any thread)
    bool empty() const noexcept {
        return head_.load(std::memory_order_acquire) == tail_.load(std::memory_order_acquire);
    }
    
    size_t size() const noexcept {
        const size_t h = head_.load(std::memory_order_acquire);
        const size_t t = tail_.load(std::memory_order_acquire);
        return (t - h) & mask_;
    }
    
    size_t capacity() const noexcept {
        return Size - 1;  // One slot reserved for full/empty distinction
    }
    
    // Performance monitoring
    double utilization() const noexcept {
        return static_cast<double>(size()) / capacity();
    }
};

// Multi-Producer Single-Consumer queue for scenarios with multiple data sources
template<typename T, size_t Size = 65536>
class MPSCQueue {
    static_assert((Size & (Size - 1)) == 0, "Size must be power of 2");
    
private:
    static constexpr size_t CACHE_LINE_SIZE = 64;
    
    alignas(CACHE_LINE_SIZE) std::atomic<size_t> head_{0};
    alignas(CACHE_LINE_SIZE) std::atomic<size_t> tail_{0};
    alignas(CACHE_LINE_SIZE) std::array<std::atomic<T*>, Size> buffer_;
    
    static constexpr size_t mask_ = Size - 1;
    
public:
    MPSCQueue() {
        // Initialize buffer pointers to nullptr
        for (auto& ptr : buffer_) {
            ptr.store(nullptr, std::memory_order_relaxed);
        }
    }
    
    ~MPSCQueue() {
        // Clean up any remaining items
        T* item;
        while (try_pop(item)) {
            delete item;
        }
    }
    
    // Producer side (multiple threads allowed)
    bool try_push(T* item) noexcept {
        const size_t pos = tail_.fetch_add(1, std::memory_order_acq_rel) & mask_;
        
        // Wait for slot to be available
        T* expected = nullptr;
        while (!buffer_[pos].compare_exchange_weak(expected, item, std::memory_order_release, std::memory_order_relaxed)) {
            if (expected != nullptr) {
                return false;  // Slot occupied, queue might be full
            }
            expected = nullptr;
        }
        
        return true;
    }
    
    // Consumer side (single thread only)
    bool try_pop(T*& item) noexcept {
        const size_t pos = head_.load(std::memory_order_relaxed);
        
        item = buffer_[pos].exchange(nullptr, std::memory_order_acquire);
        if (item == nullptr) {
            return false;  // No item available
        }
        
        head_.store((pos + 1) & mask_, std::memory_order_release);
        return true;
    }
    
    size_t approximate_size() const noexcept {
        const size_t h = head_.load(std::memory_order_acquire);
        const size_t t = tail_.load(std::memory_order_acquire);
        return (t - h) & mask_;
    }
};

// Specialized queue for market data messages with batching support
class MarketDataQueue {
private:
    SPSCQueue<MarketDataMessage, 131072> queue_;  // 128K messages
    std::atomic<uint64_t> dropped_messages_{0};
    
public:
    bool enqueue(MarketDataMessage&& msg) noexcept {
        if (!queue_.try_push(std::move(msg))) {
            dropped_messages_.fetch_add(1, std::memory_order_relaxed);
            return false;
        }
        return true;
    }
    
    bool dequeue(MarketDataMessage& msg) noexcept {
        return queue_.try_pop(msg);
    }
    
    // Batch dequeue for improved cache efficiency
    size_t dequeue_batch(MarketDataMessage* msgs, size_t max_count) noexcept {
        size_t count = 0;
        while (count < max_count && queue_.try_pop(msgs[count])) {
            ++count;
        }
        return count;
    }
    
    bool empty() const noexcept { return queue_.empty(); }
    size_t size() const noexcept { return queue_.size(); }
    double utilization() const noexcept { return queue_.utilization(); }
    uint64_t dropped_count() const noexcept { return dropped_messages_.load(); }
};

} // namespace market_data 