#include "gtest/gtest.h"
#include "../include/EventBus.h"
#include <atomic>
#include <chrono>

// 测试消息类型
struct TradeEvent {
    std::string symbol;
    double price;
};

struct RiskResult {
    bool allowed;
    std::string reason;
};

class EventBusTest : public ::testing::Test {
protected:
    void SetUp() override {
        bus.start();
    }

    void TearDown() override {
        bus.stop();
    }

    EventBus& bus = EventBus::instance();
};

// 基本功能测试
TEST_F(EventBusTest, BasicSubscription) {
    std::atomic<bool> handlerCalled{false};
    
    auto id = bus.subscribe<TradeEvent>("trade.test", 
        [&](std::shared_ptr<TradeEvent> trade) -> std::shared_ptr<EventBus::Response> {
            handlerCalled = true;
            EXPECT_EQ(trade->symbol, "AAPL");
            EXPECT_DOUBLE_EQ(trade->price, 150.25);
            return std::make_shared<EventBus::TypedResponse<void>>();
        });

    TradeEvent event{"AAPL", 150.25};
    bus.post<TradeEvent>("trade.test", event);
    
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    EXPECT_TRUE(handlerCalled.load());
    
    bus.unsubscribe(id);
}

// 异步消息测试
TEST_F(EventBusTest, AsyncMessage) {
    std::atomic<int> callCount{0};
    
    auto id = bus.subscribe<TradeEvent>("trade.async", 
        [&](std::shared_ptr<TradeEvent>) {
            callCount++;
            return std::make_shared<EventBus::TypedResponse<void>>();
        });

    TradeEvent event{"MSFT", 200.50};
    bus.postAsync<TradeEvent>("trade.async", event);
    
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    EXPECT_EQ(callCount.load(), 1);
    
    bus.unsubscribe(id);
}

// 通配符主题测试
TEST_F(EventBusTest, WildcardTopic) {
    std::atomic<int> matchedCount{0};
    
    auto id1 = bus.subscribe<TradeEvent>("trade.*", 
        [&](std::shared_ptr<TradeEvent>) {
            matchedCount++;
            return std::make_shared<EventBus::TypedResponse<void>>();
        });
    
    auto id2 = bus.subscribe<TradeEvent>("trade.specific", 
        [&](std::shared_ptr<TradeEvent>) {
            matchedCount += 10; // 不应被调用
            return std::make_shared<EventBus::TypedResponse<void>>();
        });

    TradeEvent event{"GOOG", 175.75};
    bus.post<TradeEvent>("trade.wildcard", event);
    
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    EXPECT_EQ(matchedCount.load(), 1);
    
    bus.unsubscribe(id1);
    bus.unsubscribe(id2);
}

// 响应数据测试
TEST_F(EventBusTest, ResponseData) {
    auto id = bus.subscribe<TradeEvent>("trade.response", 
        [](std::shared_ptr<TradeEvent> trade) {
            if (trade->price > 100) {
                return std::make_shared<EventBus::TypedResponse<RiskResult>>(
                    RiskResult{true, "OK"}
                );
            }
            return std::make_shared<EventBus::TypedResponse<RiskResult>>(
                RiskResult{false, "Price too low"}
            );
        });

    TradeEvent event1{"AAPL", 150.25};
    auto results1 = bus.post<TradeEvent>("trade.response", event1);
    ASSERT_EQ(results1.size(), 1);
    
    auto resp1 = std::dynamic_pointer_cast<EventBus::TypedResponse<RiskResult>>(results1[0]);
    ASSERT_NE(resp1, nullptr);
    EXPECT_TRUE(resp1->get().allowed);

    TradeEvent event2{"BIDU", 80.50};
    auto results2 = bus.post<TradeEvent>("trade.response", event2);
    auto resp2 = std::dynamic_pointer_cast<EventBus::TypedResponse<RiskResult>>(results2[0]);
    EXPECT_FALSE(resp2->get().allowed);
    
    bus.unsubscribe(id);
}

// 优先级测试
TEST_F(EventBusTest, HandlerPriority) {
    std::vector<int> executionOrder;
    
    auto id1 = bus.subscribe<TradeEvent>("trade.priority", 
        [&](std::shared_ptr<TradeEvent>) {
            executionOrder.push_back(1);
            return std::make_shared<EventBus::TypedResponse<void>>();
        }, 10); // 高优先级
        
    auto id2 = bus.subscribe<TradeEvent>("trade.priority", 
        [&](std::shared_ptr<TradeEvent>) {
            executionOrder.push_back(2);
            return std::make_shared<EventBus::TypedResponse<void>>();
        }, 100); // 低优先级

    TradeEvent event{"TSLA", 250.75};
    bus.post<TradeEvent>("trade.priority", event);
    
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    ASSERT_EQ(executionOrder.size(), 2);
    EXPECT_EQ(executionOrder[0], 1); // 高优先级先执行
    EXPECT_EQ(executionOrder[1], 2);
    
    bus.unsubscribe(id1);
    bus.unsubscribe(id2);
}

// 取消订阅测试
TEST_F(EventBusTest, Unsubscription) {
    std::atomic<int> callCount{0};
    
    auto id = bus.subscribe<TradeEvent>("trade.unsub", 
        [&](std::shared_ptr<TradeEvent>) {
            callCount++;
            return std::make_shared<EventBus::TypedResponse<void>>();
        });

    TradeEvent event1{"FB", 195.50};
    bus.post<TradeEvent>("trade.unsub", event1);
    
    bus.unsubscribe(id);
    
    TradeEvent event2{"FB", 196.00};
    bus.post<TradeEvent>("trade.unsub", event2);
    
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    EXPECT_EQ(callCount.load(), 1); // 只应调用一次
}

// 多线程压力测试
TEST_F(EventBusTest, MultithreadStress) {
    const int THREAD_COUNT = 10;
    const int MESSAGE_COUNT = 100;
    std::atomic<int> totalReceived{0};
    
    auto id = bus.subscribe<TradeEvent>("trade.stress", 
        [&](std::shared_ptr<TradeEvent>) {
            totalReceived++;
            return std::make_shared<EventBus::TypedResponse<void>>();
        });

    std::vector<std::thread> threads;
    for (int i = 0; i < THREAD_COUNT; ++i) {
        threads.emplace_back([&, i]() {
            for (int j = 0; j < MESSAGE_COUNT; ++j) {
                TradeEvent event{"STRESS", static_cast<double>(i * 1000 + j)};
                bus.postAsync<TradeEvent>("trade.stress", event);
            }
        });
    }
    
    for (auto& t : threads) {
        t.join();
    }
    
    std::this_thread::sleep_for(std::chrono::seconds(1)); // 等待处理完成
    EXPECT_EQ(totalReceived.load(), THREAD_COUNT * MESSAGE_COUNT);
    
    bus.unsubscribe(id);
}