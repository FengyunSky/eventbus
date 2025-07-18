#ifndef EVENTBUS_H
#define EVENTBUS_H

#include <functional>
#include <vector>
#include <queue>
#include <memory>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <string>
#include <typeinfo>
#include <stdexcept>
#include <iostream>

class EventBus {
public:
    using HandlerId = size_t;
    using Topic = std::string;
    
    class Message {
    public:
        virtual ~Message() {}
        virtual const std::type_info& type() const = 0;
    };

    class Response {
    public:
        virtual ~Response() {}
        virtual bool isValid() const = 0;
    };

    template <typename T>
    class TypedMessage : public Message {
    public:
        TypedMessage(const T& data) : data_(data) {}
        const T& get() const { return data_; }
        const std::type_info& type() const override { return typeid(T); }
    private:
        T data_;
    };

    template <typename T>
    class TypedResponse : public Response {
    public:
        TypedResponse(const T& data) : data_(data), valid_(true) {}
        TypedResponse() : valid_(false) {}
        const T& get() const { 
            if (!valid_) throw std::runtime_error("Invalid response");
            return data_; 
        }
        bool isValid() const override { return valid_; }
    private:
        T data_;
        bool valid_;
    };

    template <>
    class TypedResponse<void> : public Response {
    public:
        TypedResponse(bool valid = true) : valid_(valid) {}
        
        void get() const { 
            if (!valid_) throw std::runtime_error("Invalid response");
        }
        
        bool isValid() const override { return valid_; }

    private:
        bool valid_;
    };

    typedef std::function<std::shared_ptr<Response>(std::shared_ptr<Message>)> Handler;

    static EventBus& instance();

    void start();
    void stop();

    //A lower priority value indicates higher priority
    template<typename T>
    HandlerId subscribe(const Topic& topicPattern, 
                       std::function<std::shared_ptr<Response>(std::shared_ptr<T>)> handler,
                       int priority = 0);

    void unsubscribe(HandlerId id);

    template<typename T>
    std::vector<std::shared_ptr<Response>> post(const Topic& topic, const T& message);

    template<typename T>
    void postAsync(const Topic& topic, const T& message);

private:
    EventBus();
    ~EventBus();

    struct Subscription {
        HandlerId id;
        Topic topicPattern;
        Handler handler;
        int priority;
        
        bool operator<(const Subscription& other) const {
            return priority < other.priority;
        }
    };

    struct MessageEvent {
        Topic topic;
        std::shared_ptr<Message> message;
    };

    std::vector<Subscription> subscriptions_;
    std::queue<MessageEvent> messageQueue_;
    std::thread dispatchThread_;
    std::mutex subscriptionMutex_;
    std::mutex queueMutex_;
    std::condition_variable cv_;
    std::atomic<bool> running_;
    std::atomic<HandlerId> handlerIdCounter_;

    bool isTopicMatch(const Topic& pattern, const Topic& topic);
    void dispatchLoop();
};

template<typename T>
EventBus::HandlerId EventBus::subscribe(
    const Topic& topicPattern, 
    std::function<std::shared_ptr<Response>(std::shared_ptr<T>)> handler,
    int priority) 
{
    std::lock_guard<std::mutex> lock(subscriptionMutex_);
    
    auto wrappedHandler = [handler](std::shared_ptr<Message> msg) -> std::shared_ptr<Response> {
        auto typedMsg = std::dynamic_pointer_cast<TypedMessage<T>>(msg);
        if (!typedMsg) {
            return std::make_shared<TypedResponse<void>>();
        }
        return handler(std::make_shared<T>(std::move(typedMsg->get())));
    };
    
    HandlerId id = ++handlerIdCounter_;
    subscriptions_.push_back({
        id, 
        topicPattern, 
        wrappedHandler,
        priority
    });
    
    std::stable_sort(subscriptions_.begin(), subscriptions_.end());
    return id;
}

template<typename T>
std::vector<std::shared_ptr<EventBus::Response>> EventBus::post(
    const Topic& topic, const T& message) 
{
    auto msg = std::make_shared<TypedMessage<T>>(message);
    std::vector<Subscription> handlers;
    
    {
        std::lock_guard<std::mutex> lock(subscriptionMutex_);
        for (const auto& sub : subscriptions_) {
            if (isTopicMatch(sub.topicPattern, topic)) {
                handlers.push_back(sub);
            }
        }
    }
    
    std::vector<std::shared_ptr<Response>> results;
    for (const auto& sub : handlers) {
        try {
            auto result = sub.handler(msg);
            if (result->isValid()) {
                results.push_back(result);
            }
        } catch (std::exception e) {
            std::cerr << e.what() << std::endl;
        }
    }
    
    return results;
}

template<typename T>
void EventBus::postAsync(const Topic& topic, const T& message) {
    auto msg = std::make_shared<TypedMessage<T>>(message);
    
    {
        std::lock_guard<std::mutex> lock(queueMutex_);
        messageQueue_.push({topic, msg});
    }
    cv_.notify_one();
}

#endif // EVENTBUS_H