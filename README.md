# EventBus Module

A lightweight, thread-safe event bus implementation for C++ applications, supporting synchronous and asynchronous event handling with priority-based execution order.

## Features

- **Type-safe event handling** with templated subscriptions
- **Priority-based execution** (higher priority handlers execute first)
- **Synchronous & asynchronous** event posting
- **Wildcard topic matching** (e.g., `trade.*`)
- **Response handling** for request/response patterns
- **Thread-safe** design for concurrent usage
- **Zero dependencies** (pure C++11 STL)

## Installation

1. Copy the following files to your project:
- `include/EventBus.h`
- `src/EventBus.cpp`

2. Add to your CMakeLists.txt:
```cmake
add_library(EventBus STATIC src/EventBus.cpp)
target_include_directories(EventBus PUBLIC include)
```

## Usage

### Basic Example

```cpp
#include "EventBus.h"

struct MyEvent {
std::string message;
};

// Subscribe
auto id = EventBus::instance().subscribe<MyEvent>(
"my.topic",
[](std::shared_ptr<MyEvent> event) {
std::cout << event->message << std::endl;
return EventBus::Response::voidResponse();
}
);

// Publish
EventBus::instance().post<MyEvent>(
"my.topic",
MyEvent{"Hello World"}
);

// Unsubscribe
EventBus::instance().unsubscribe(id);
```

### Key Components

| Component | Description |
|-----------|-------------|
| `EventBus::instance()` | Singleton access point |
| `subscribe<T>()` | Register an event handler |
| `post<T>()` | Synchronous event publishing |
| `postAsync<T>()` | Asynchronous event publishing |
| `unsubscribe()` | Remove a handler |
| `TypedMessage<T>` | Event wrapper class |
| `TypedResponse<T>` | Response wrapper class |

## Advanced Features

### Priority Handling

```cpp
// High priority handler (executes first)
bus.subscribe<TradeEvent>(
"trade.alert",
[](auto event) { /* Critical processing */ },
100// Priority value (higher = earlier execution)
);

// Low priority handler
bus.subscribe<TradeEvent>(
"trade.alert",
[](auto event) { /* Background processing */ },
10// Lower priority
);
```

### Wildcard Subscriptions

```cpp
// Matches "trade.stocks", "trade.forex", etc.
bus.subscribe<TradeEvent>(
"trade.*",
[](auto event) {
std::cout << "Trade occurred: " << event->symbol;
}
);
```

### Request/Response Pattern

```cpp
// Handler with response
bus.subscribe<AuthRequest>(
"auth.check",
[](std::shared_ptr<AuthRequest> req) {
return std::make_shared<EventBus::TypedResponse<AuthResult>>(
AuthResult{req->valid(), "Processed"}
);
}
);

// Caller
auto results = bus.post<AuthRequest>(
"auth.check",
AuthRequest{...}
);

auto response = dynamic_cast<TypedResponse<AuthResult>*>(results[0].get());
if(response->isValid()) {
use(response->get());
}
```

## Threading Model

- **Subscriptions** are thread-safe
- **post()** blocks until all handlers complete
- **postAsync()** queues events for background processing
- **Handler execution**:
- Synchronous: Runs in publisher's thread
- Asynchronous: Runs in dedicated dispatch thread

## Best Practices

1. **RAII for subscriptions**:
```cpp
class MyService {
EventBus::HandlerId id_;
public:
MyService() {
id_ = bus.subscribe(...);
}
~MyService() {
bus.unsubscribe(id_);
}
};
```

2. **Error handling**:
```cpp
bus.subscribe<RiskEvent>(
"risk.check",
[](auto event) {
try {
// Risky operation
return makeResponse(success);
} catch(...) {
return EventBus::Response::invalidResponse();
}
}
);
```

3. **Performance tips**:
- Use `postAsync()` for non-critical paths
- Keep handlers short
- Prefer stack allocation for events when possible

## Building Tests

```bash
mkdir build && cd build
cmake .. -DEVENTBUS_BUILD_TESTS=ON
make
ctest --output-on-failure
```