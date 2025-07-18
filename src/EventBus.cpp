#include "../include/EventBus.h"

EventBus::EventBus() : running_(false), handlerIdCounter_(0) {}
EventBus::~EventBus() { stop(); }

EventBus& EventBus::instance() {
    static EventBus instance;
    return instance;
}

void EventBus::start() {
    if (!running_.exchange(true)) {
        dispatchThread_ = std::thread(&EventBus::dispatchLoop, this);
    }
}

void EventBus::stop() {
    if (running_.exchange(false)) {
        cv_.notify_all();
        if (dispatchThread_.joinable()) {
            dispatchThread_.join();
        }
    }
}

void EventBus::unsubscribe(HandlerId id) {
    std::lock_guard<std::mutex> lock(subscriptionMutex_);
    subscriptions_.erase(
        std::remove_if(subscriptions_.begin(), subscriptions_.end(),
            [id](const Subscription& sub) { return sub.id == id; }),
        subscriptions_.end());
}

bool EventBus::isTopicMatch(const Topic& pattern, const Topic& topic) {
    if (pattern == "*") return true;
    if (pattern.back() == '*' && 
        topic.compare(0, pattern.length()-1, pattern, 0, pattern.length()-1) == 0) {
        return true;
    }
    return pattern == topic;
}

void EventBus::dispatchLoop() {
    while (running_) {
        MessageEvent event;
        {
            std::unique_lock<std::mutex> lock(queueMutex_);
            cv_.wait(lock, [this] { 
                return !running_ || !messageQueue_.empty(); 
            });
            
            if (!running_) return;
            
            event = messageQueue_.front();
            messageQueue_.pop();
        }
        
        std::vector<Subscription> handlers;
        {
            std::lock_guard<std::mutex> lock(subscriptionMutex_);
            for (const auto& sub : subscriptions_) {
                if (isTopicMatch(sub.topicPattern, event.topic)) {
                    handlers.push_back(sub);
                }
            }
        }
        
        for (const auto& sub : handlers) {
            try {
                sub.handler(event.message);
            } catch (std::exception e) {
                std::cerr << e.what() << std::endl;
            }
        }
    }
}