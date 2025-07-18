#include "../include/EventBus.h"
#include <iostream>
#include <thread>
#include <chrono>
#include <memory>

// 自定义消息类型
struct VoidMessage {};
struct TradeEvent {
    std::string symbol;
    double price;
};

struct RiskResult {
    bool allowed;
    std::string reason;
};

struct Notification {
    std::string message;
};

// 业务处理器类
class RiskEngine {
public:
    RiskEngine() {
        // 低优先级处理器
        riskSubId_ = EventBus::instance().subscribe<TradeEvent>(
            "risk.check", 
            [this](std::shared_ptr<TradeEvent> trade) { return checkRisk(trade); },
            200  // 低优先级
        );
    }

    ~RiskEngine() {
        EventBus::instance().unsubscribe(riskSubId_);
    }

private:
    std::shared_ptr<EventBus::Response> checkRisk(std::shared_ptr<TradeEvent> trade) {
        std::cout << "[RiskEngine] Processing " << trade->symbol << ", price:" << trade->price << std::endl;
        
        if (trade->price > 1000) {
            return std::make_shared<EventBus::TypedResponse<RiskResult>>(
                RiskResult{false, "Price too high"}
            );
        }
        return std::make_shared<EventBus::TypedResponse<void>>(); // 无返回值的通过
    }

    EventBus::HandlerId riskSubId_;
};

class Notifier {
public:
    Notifier() {
        notifSubId_ = EventBus::instance().subscribe<TradeEvent>(
            "trade.*",  // 通配符订阅
            [this](std::shared_ptr<TradeEvent> trade) { return sendNotification(trade); }
        );
    }

    ~Notifier() {
        EventBus::instance().unsubscribe(notifSubId_);
    }

private:
    std::shared_ptr<EventBus::Response> sendNotification(std::shared_ptr<TradeEvent> trade) {
        std::cout << "[Notifier] Sending alert for " << trade->symbol << std::endl;
        
        EventBus::instance().postAsync<Notification>(
            "notification",
            Notification{"Trade executed: " + trade->symbol}
        );
        
        return std::make_shared<EventBus::TypedResponse<void>>();
    }

    EventBus::HandlerId notifSubId_;
};

int main() {
    EventBus& bus = EventBus::instance();
    bus.start();

    // 1. 基本订阅/发布场景
    auto basicSubId = bus.subscribe<TradeEvent>(
        "basic.trade",
        [](std::shared_ptr<TradeEvent> trade) {
            std::cout << "Basic handler: " << trade->symbol 
                      << " @ " << trade->price << std::endl;
            return std::make_shared<EventBus::TypedResponse<void>>();
        }
    );
    bus.subscribe<VoidMessage>(
        "basic.trade",[](std::shared_ptr<VoidMessage>){
            std::cout << "void message handler" << std::endl;
            return std::make_shared<EventBus::TypedResponse<void>>();
        }
    );

    TradeEvent trade1{"GOOG", 142.56};
    bus.post<TradeEvent>("basic.trade", trade1);
    bus.post<VoidMessage>("basic.trade", VoidMessage{});

    // 2. 带响应的同步调用
    auto respSubId = bus.subscribe<TradeEvent>(
        "trade.validate",
        [](std::shared_ptr<TradeEvent> trade) {
            if (trade->price <= 0) {
                return std::make_shared<EventBus::TypedResponse<RiskResult>>(
                    RiskResult{false, "Invalid price"}
                );
            }
            return std::make_shared<EventBus::TypedResponse<RiskResult>>(
                RiskResult{true, "Valid"}
            );
        }
    );

    TradeEvent trade2{"AAPL", -1.23};
    auto results = bus.post<TradeEvent>("trade.validate", trade2);
    for (auto& r : results) {
        if (auto resp = std::dynamic_pointer_cast<EventBus::TypedResponse<RiskResult>>(r)) {
            std::cout << "Validation: " 
                      << (resp->get().allowed ? "Approved" : "Rejected")
                      << " - " << resp->get().reason << std::endl;
        }
    }

    // 3. 异步处理场景
    bus.subscribe<Notification>(
        "notification",
        [](std::shared_ptr<Notification> notif) {
            std::cout << "Notification received: " 
                      << notif->message << std::endl;
            return std::make_shared<EventBus::TypedResponse<void>>();
        }
    );

    RiskEngine riskEngine;
    TradeEvent trade3{"MSFT", 247.86};
    bus.postAsync<TradeEvent>("risk.check", trade3);

    // 4. 优先级测试
    bus.subscribe<TradeEvent>(
        "risk.check",
        [](std::shared_ptr<TradeEvent> trade) {
            std::cout << "High priority handler for " << trade->symbol << ", price:" << trade->price << std::endl;
            return std::make_shared<EventBus::TypedResponse<void>>();
        },
        50  // 高优先级
    );

    // 5. 通配符测试
    Notifier notifier;
    TradeEvent trade4{"TSLA", 699.20};
    bus.post<TradeEvent>("trade.special", trade4);

    // 6. 多线程测试
    std::vector<std::thread> threads;
    for (int i = 0; i < 5; ++i) {
        threads.emplace_back([i, &bus]() {
            TradeEvent trade{"THREAD", 100.0 + i};
            bus.postAsync<TradeEvent>("risk.check", trade);
        });
    }

    // 7. 取消订阅测试
    bus.unsubscribe(basicSubId);
    TradeEvent trade5{"UNSUB", 123.45};
    bus.post<TradeEvent>("basic.trade", trade5);  // 不应被处理

    // 等待异步处理完成
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    // 清理
    for (auto& t : threads) t.join();
    bus.unsubscribe(respSubId);
    bus.stop();
    
    return 0;
}