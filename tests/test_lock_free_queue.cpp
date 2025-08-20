#include <gtest/gtest.h>
#include <thread>
#include <vector>
#include <atomic>
#include <chrono>
#include "core/lock_free_queue.hpp"
#include "core/market_data_types.hpp"

using namespace market_data;

class LockFreeQueueTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Reset any global state if needed
    }
    
    void TearDown() override {
        // Cleanup after each test
    }
};

// Basic functionality tests
TEST_F(LockFreeQueueTest, BasicEnqueueDequeue) {
    SPSCQueue<int> queue;
    
    EXPECT_TRUE(queue.empty());
    EXPECT_EQ(queue.size(), 0);
    
    // Enqueue some items
    EXPECT_TRUE(queue.try_push(42));
    EXPECT_TRUE(queue.try_push(123));
    EXPECT_TRUE(queue.try_push(456));
    
    EXPECT_FALSE(queue.empty());
    EXPECT_EQ(queue.size(), 3);
    
    // Dequeue items
    int value;
    EXPECT_TRUE(queue.try_pop(value));
    EXPECT_EQ(value, 42);
    
    EXPECT_TRUE(queue.try_pop(value));
    EXPECT_EQ(value, 123);
    
    EXPECT_TRUE(queue.try_pop(value));
    EXPECT_EQ(value, 456);
    
    EXPECT_TRUE(queue.empty());
    EXPECT_EQ(queue.size(), 0);
    
    // Try to dequeue from empty queue
    EXPECT_FALSE(queue.try_pop(value));
}

TEST_F(LockFreeQueueTest, QueueCapacity) {
    SPSCQueue<int, 8> small_queue;  // 8 elements, 7 usable
    
    // Fill queue to capacity
    for (int i = 0; i < 7; ++i) {
        EXPECT_TRUE(small_queue.try_push(i));
    }
    
    EXPECT_EQ(small_queue.size(), 7);
    
    // Next push should fail (queue full)
    EXPECT_FALSE(small_queue.try_push(999));
    
    // Dequeue one item
    int value;
    EXPECT_TRUE(small_queue.try_pop(value));
    EXPECT_EQ(value, 0);
    
    // Now we should be able to push again
    EXPECT_TRUE(small_queue.try_push(777));
    EXPECT_EQ(small_queue.size(), 7);
}

TEST_F(LockFreeQueueTest, MoveSemantics) {
    SPSCQueue<std::unique_ptr<int>> queue;
    
    auto ptr1 = std::make_unique<int>(42);
    auto ptr2 = std::make_unique<int>(123);
    
    // Move items into queue
    EXPECT_TRUE(queue.try_push(std::move(ptr1)));
    EXPECT_TRUE(queue.try_push(std::move(ptr2)));
    
    EXPECT_EQ(ptr1, nullptr);  // Should be moved
    EXPECT_EQ(ptr2, nullptr);  // Should be moved
    
    // Move items out of queue
    std::unique_ptr<int> result;
    EXPECT_TRUE(queue.try_pop(result));
    EXPECT_NE(result, nullptr);
    EXPECT_EQ(*result, 42);
    
    EXPECT_TRUE(queue.try_pop(result));
    EXPECT_NE(result, nullptr);
    EXPECT_EQ(*result, 123);
}

// Concurrent access tests
TEST_F(LockFreeQueueTest, SingleProducerSingleConsumer) {
    SPSCQueue<int> queue;
    constexpr size_t NUM_ITEMS = 100000;
    
    std::atomic<bool> producer_done{false};
    std::atomic<size_t> items_consumed{0};
    
    // Producer thread
    std::thread producer([&]() {
        for (size_t i = 0; i < NUM_ITEMS; ++i) {
            while (!queue.try_push(static_cast<int>(i))) {
                std::this_thread::yield();
            }
        }
        producer_done.store(true);
    });
    
    // Consumer thread
    std::thread consumer([&]() {
        int value;
        size_t expected = 0;
        
        while (items_consumed.load() < NUM_ITEMS) {
            if (queue.try_pop(value)) {
                EXPECT_EQ(value, static_cast<int>(expected));
                ++expected;
                items_consumed.fetch_add(1);
            } else if (producer_done.load()) {
                // Producer is done, but we might still have items in queue
                std::this_thread::yield();
            } else {
                std::this_thread::yield();
            }
        }
    });
    
    producer.join();
    consumer.join();
    
    EXPECT_EQ(items_consumed.load(), NUM_ITEMS);
    EXPECT_TRUE(queue.empty());
}

TEST_F(LockFreeQueueTest, HighFrequencyOperations) {
    SPSCQueue<MarketDataMessage> queue;
    constexpr size_t NUM_MESSAGES = 50000;
    constexpr auto TEST_DURATION = std::chrono::seconds(5);
    
    std::atomic<size_t> messages_sent{0};
    std::atomic<size_t> messages_received{0};
    std::atomic<bool> test_running{true};
    
    auto start_time = std::chrono::high_resolution_clock::now();
    
    // High-frequency producer
    std::thread producer([&]() {
        size_t msg_id = 0;
        while (test_running.load() && msg_id < NUM_MESSAGES) {
            MarketDataMessage msg(MessageType::TRADE);
            msg.data.trade.trade_id = static_cast<uint32_t>(msg_id);
            msg.data.trade.price = from_double(100.0 + (msg_id % 100) * 0.01);
            
            if (queue.try_push(std::move(msg))) {
                messages_sent.fetch_add(1);
                ++msg_id;
            }
        }
    });
    
    // High-frequency consumer
    std::thread consumer([&]() {
        MarketDataMessage msg;
        while (test_running.load() || !queue.empty()) {
            if (queue.try_pop(msg)) {
                messages_received.fetch_add(1);
                
                // Validate message integrity
                EXPECT_EQ(msg.type, MessageType::TRADE);
                EXPECT_GT(msg.data.trade.price, 0);
            }
        }
    });
    
    // Let test run for specified duration
    std::this_thread::sleep_for(TEST_DURATION);
    test_running.store(false);
    
    producer.join();
    consumer.join();
    
    auto end_time = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
    
    const size_t sent = messages_sent.load();
    const size_t received = messages_received.load();
    
    std::cout << "High-frequency test results:\n";
    std::cout << "  Duration: " << duration.count() << "ms\n";
    std::cout << "  Messages sent: " << sent << "\n";
    std::cout << "  Messages received: " << received << "\n";
    std::cout << "  Throughput: " << (sent * 1000 / duration.count()) << " msg/sec\n";
    std::cout << "  Loss rate: " << ((sent - received) * 100.0 / sent) << "%\n";
    
    // Validate performance targets
    const size_t throughput = sent * 1000 / duration.count();
    EXPECT_GT(throughput, 10000);  // At least 10K msg/sec
    EXPECT_EQ(sent, received);     // No message loss
}

// Market data specific tests
TEST_F(LockFreeQueueTest, MarketDataMessageQueue) {
    MarketDataQueue queue;
    
    // Create test messages
    MarketDataMessage trade_msg(MessageType::TRADE);
    trade_msg.data.trade = MarketTrade(
        now(),
        make_symbol("TEST"),
        from_double(100.50),
        1000,
        Side::BUY,
        1
    );
    
    MarketDataMessage quote_msg(MessageType::QUOTE);
    quote_msg.data.quote = MarketQuote(
        now(),
        make_symbol("TEST"),
        from_double(100.49),
        from_double(100.51),
        500,
        750
    );
    
    // Test enqueue
    EXPECT_TRUE(queue.enqueue(std::move(trade_msg)));
    EXPECT_TRUE(queue.enqueue(std::move(quote_msg)));
    
    EXPECT_EQ(queue.size(), 2);
    EXPECT_FALSE(queue.empty());
    
    // Test dequeue
    MarketDataMessage received_msg;
    EXPECT_TRUE(queue.dequeue(received_msg));
    EXPECT_EQ(received_msg.type, MessageType::TRADE);
    EXPECT_EQ(received_msg.data.trade.trade_id, 1);
    
    EXPECT_TRUE(queue.dequeue(received_msg));
    EXPECT_EQ(received_msg.type, MessageType::QUOTE);
    EXPECT_EQ(received_msg.data.quote.bid_size, 500);
    
    EXPECT_TRUE(queue.empty());
}

TEST_F(LockFreeQueueTest, BatchOperations) {
    MarketDataQueue queue;
    constexpr size_t BATCH_SIZE = 64;
    
    // Enqueue batch of messages
    for (size_t i = 0; i < BATCH_SIZE; ++i) {
        MarketDataMessage msg(MessageType::TRADE);
        msg.data.trade.trade_id = static_cast<uint32_t>(i);
        msg.data.trade.price = from_double(100.0 + i * 0.01);
        EXPECT_TRUE(queue.enqueue(std::move(msg)));
    }
    
    EXPECT_EQ(queue.size(), BATCH_SIZE);
    
    // Dequeue in batch
    std::array<MarketDataMessage, BATCH_SIZE> batch;
    const size_t dequeued = queue.dequeue_batch(batch.data(), BATCH_SIZE);
    
    EXPECT_EQ(dequeued, BATCH_SIZE);
    EXPECT_TRUE(queue.empty());
    
    // Validate batch contents
    for (size_t i = 0; i < dequeued; ++i) {
        EXPECT_EQ(batch[i].type, MessageType::TRADE);
        EXPECT_EQ(batch[i].data.trade.trade_id, static_cast<uint32_t>(i));
        EXPECT_EQ(batch[i].data.trade.price, from_double(100.0 + i * 0.01));
    }
}

// Performance and stress tests
TEST_F(LockFreeQueueTest, MemoryOrdering) {
    SPSCQueue<std::atomic<int>> queue;
    constexpr size_t NUM_OPERATIONS = 10000;
    
    std::atomic<bool> producer_ready{false};
    std::atomic<bool> consumer_ready{false};
    std::atomic<size_t> operations_completed{0};
    
    // Test memory ordering with atomic operations
    std::thread producer([&]() {
        producer_ready.store(true);
        
        for (size_t i = 0; i < NUM_OPERATIONS; ++i) {
            std::atomic<int> value{static_cast<int>(i)};
            while (!queue.try_push(std::move(value))) {
                std::this_thread::yield();
            }
        }
    });
    
    std::thread consumer([&]() {
        consumer_ready.store(true);
        
        std::atomic<int> value;
        for (size_t i = 0; i < NUM_OPERATIONS; ++i) {
            while (!queue.try_pop(value)) {
                std::this_thread::yield();
            }
            
            EXPECT_EQ(value.load(), static_cast<int>(i));
            operations_completed.fetch_add(1);
        }
    });
    
    producer.join();
    consumer.join();
    
    EXPECT_EQ(operations_completed.load(), NUM_OPERATIONS);
    EXPECT_TRUE(producer_ready.load());
    EXPECT_TRUE(consumer_ready.load());
}

TEST_F(LockFreeQueueTest, StressTest) {
    SPSCQueue<uint64_t> queue;
    constexpr size_t STRESS_ITERATIONS = 1000000;
    constexpr auto STRESS_DURATION = std::chrono::seconds(10);
    
    std::atomic<bool> stress_running{true};
    std::atomic<uint64_t> total_enqueued{0};
    std::atomic<uint64_t> total_dequeued{0};
    std::atomic<uint64_t> queue_full_count{0};
    std::atomic<uint64_t> queue_empty_count{0};
    
    auto start_time = std::chrono::high_resolution_clock::now();
    
    // Stress producer
    std::thread producer([&]() {
        uint64_t value = 0;
        while (stress_running.load()) {
            if (queue.try_push(value++)) {
                total_enqueued.fetch_add(1);
            } else {
                queue_full_count.fetch_add(1);
                std::this_thread::yield();
            }
        }
    });
    
    // Stress consumer
    std::thread consumer([&]() {
        uint64_t value;
        while (stress_running.load() || !queue.empty()) {
            if (queue.try_pop(value)) {
                total_dequeued.fetch_add(1);
            } else {
                queue_empty_count.fetch_add(1);
                std::this_thread::yield();
            }
        }
    });
    
    // Run stress test
    std::this_thread::sleep_for(STRESS_DURATION);
    stress_running.store(false);
    
    producer.join();
    consumer.join();
    
    auto end_time = std::chrono::high_resolution_clock::now();
    auto duration_ms = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time).count();
    
    const uint64_t enqueued = total_enqueued.load();
    const uint64_t dequeued = total_dequeued.load();
    
    std::cout << "Stress test results (" << duration_ms << "ms):\n";
    std::cout << "  Enqueued: " << enqueued << " (" << (enqueued * 1000 / duration_ms) << " ops/sec)\n";
    std::cout << "  Dequeued: " << dequeued << " (" << (dequeued * 1000 / duration_ms) << " ops/sec)\n";
    std::cout << "  Queue full events: " << queue_full_count.load() << "\n";
    std::cout << "  Queue empty events: " << queue_empty_count.load() << "\n";
    std::cout << "  Final queue size: " << queue.size() << "\n";
    
    // Validate stress test results
    EXPECT_GT(enqueued, 100000);  // At least 100K operations
    EXPECT_EQ(enqueued - dequeued, queue.size());  // Conservation of messages
}

// Market data specific queue tests
TEST_F(LockFreeQueueTest, MarketDataMessageIntegrity) {
    MarketDataQueue queue;
    constexpr size_t NUM_MESSAGES = 1000;
    
    // Generate test market data
    std::vector<MarketDataMessage> test_messages;
    test_messages.reserve(NUM_MESSAGES);
    
    for (size_t i = 0; i < NUM_MESSAGES; ++i) {
        MarketDataMessage msg(MessageType::TRADE);
        msg.sequence_number.store(i);
        msg.receive_timestamp = now();
        msg.data.trade = MarketTrade(
            msg.receive_timestamp,
            make_symbol("TEST" + std::to_string(i % 10)),
            from_double(100.0 + i * 0.01),
            1000 + i,
            static_cast<Side>(i % 2),
            static_cast<uint32_t>(i)
        );
        test_messages.push_back(std::move(msg));
    }
    
    // Enqueue all messages
    for (auto& msg : test_messages) {
        EXPECT_TRUE(queue.enqueue(std::move(msg)));
    }
    
    EXPECT_EQ(queue.size(), NUM_MESSAGES);
    
    // Dequeue and validate
    for (size_t i = 0; i < NUM_MESSAGES; ++i) {
        MarketDataMessage received;
        EXPECT_TRUE(queue.dequeue(received));
        
        EXPECT_EQ(received.type, MessageType::TRADE);
        EXPECT_EQ(received.sequence_number.load(), i);
        EXPECT_EQ(received.data.trade.trade_id, static_cast<uint32_t>(i));
        EXPECT_EQ(received.data.trade.price, from_double(100.0 + i * 0.01));
        EXPECT_EQ(received.data.trade.quantity, 1000 + i);
        EXPECT_EQ(received.data.trade.aggressor_side, static_cast<Side>(i % 2));
    }
    
    EXPECT_TRUE(queue.empty());
}

TEST_F(LockFreeQueueTest, PerformanceCharacteristics) {
    SPSCQueue<uint64_t> queue;
    constexpr size_t PERF_ITERATIONS = 100000;
    
    std::vector<uint64_t> enqueue_latencies;
    std::vector<uint64_t> dequeue_latencies;
    enqueue_latencies.reserve(PERF_ITERATIONS);
    dequeue_latencies.reserve(PERF_ITERATIONS);
    
    // Measure enqueue latencies
    for (size_t i = 0; i < PERF_ITERATIONS; ++i) {
        const auto start = std::chrono::high_resolution_clock::now();
        queue.try_push(i);
        const auto end = std::chrono::high_resolution_clock::now();
        
        enqueue_latencies.push_back(
            std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count());
    }
    
    // Measure dequeue latencies
    uint64_t value;
    for (size_t i = 0; i < PERF_ITERATIONS; ++i) {
        const auto start = std::chrono::high_resolution_clock::now();
        queue.try_pop(value);
        const auto end = std::chrono::high_resolution_clock::now();
        
        dequeue_latencies.push_back(
            std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count());
    }
    
    // Calculate statistics
    std::sort(enqueue_latencies.begin(), enqueue_latencies.end());
    std::sort(dequeue_latencies.begin(), dequeue_latencies.end());
    
    const auto enqueue_p99 = enqueue_latencies[enqueue_latencies.size() * 99 / 100];
    const auto dequeue_p99 = dequeue_latencies[dequeue_latencies.size() * 99 / 100];
    
    std::cout << "Performance characteristics:\n";
    std::cout << "  Enqueue P99: " << enqueue_p99 << "ns\n";
    std::cout << "  Dequeue P99: " << dequeue_p99 << "ns\n";
    
    // Validate HFT-grade performance (sub-100ns for P99)
    EXPECT_LT(enqueue_p99, 100);  // Sub-100ns enqueue
    EXPECT_LT(dequeue_p99, 100);  // Sub-100ns dequeue
}

TEST_F(LockFreeQueueTest, UtilizationMetrics) {
    SPSCQueue<int, 16> queue;  // Small queue for testing
    
    EXPECT_DOUBLE_EQ(queue.utilization(), 0.0);
    
    // Fill queue partially
    for (int i = 0; i < 7; ++i) {
        queue.try_push(i);
    }
    
    const double expected_utilization = 7.0 / 15.0;  // 7 items in 15-capacity queue
    EXPECT_NEAR(queue.utilization(), expected_utilization, 0.01);
    
    // Fill queue completely
    for (int i = 7; i < 15; ++i) {
        queue.try_push(i);
    }
    
    EXPECT_NEAR(queue.utilization(), 1.0, 0.01);
    
    // Empty queue
    int value;
    while (queue.try_pop(value)) {
        // Drain queue
    }
    
    EXPECT_DOUBLE_EQ(queue.utilization(), 0.0);
} 