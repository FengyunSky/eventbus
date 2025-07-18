# EventBus 模块

一个轻量级、线程安全的C++事件总线实现，支持带优先级处理的同步和异步事件分发。

## 功能特性

- **类型安全的事件处理** 通过模板订阅机制
- **基于优先级的执行** (高优先级处理器优先执行)
- **同步 & 异步** 事件发布
- **通配符主题匹配** (如 `trade.*`)
- **响应处理** 支持请求/响应模式
- **线程安全设计** 适用于并发场景
- **零依赖** (纯C++11标准库实现)

## 安装指南

1. 复制以下文件到您的项目：
- `include/EventBus.h`
- `src/EventBus.cpp`

2. 在CMakeLists.txt中添加：
```cmake
add_library(EventBus STATIC src/EventBus.cpp)
target_include_directories(EventBus PUBLIC include)
```

## 使用示例

### 基础用法

```cpp
#include "EventBus.h"

struct MyEvent {
std::string message;
};

// 订阅事件
auto id = EventBus::instance().subscribe<MyEvent>(
"my.topic",
[](std::shared_ptr<MyEvent> event) {
std::cout << event->message << std::endl;
return EventBus::Response::voidResponse();
}
);

// 发布事件
EventBus::instance().post<MyEvent>(
"my.topic",
MyEvent{"Hello World"}
);

// 取消订阅
EventBus::instance().unsubscribe(id);
```

### 核心组件

| 组件 | 说明 |
|------|------|
| `EventBus::instance()` | 单例访问入口 |
| `subscribe<T>()` | 注册事件处理器 |
| `post<T>()` | 同步事件发布 |
| `postAsync<T>()` | 异步事件发布 |
| `unsubscribe()` | 移除处理器 |
| `TypedMessage<T>` | 事件包装类 |
| `TypedResponse<T>` | 响应包装类 |

## 高级功能

### 优先级控制

```cpp
// 高优先级处理器(优先执行)
bus.subscribe<TradeEvent>(
"trade.alert",
[](auto event) { /* 关键处理逻辑 */ },
100// 优先级数值(越大越优先)
);

// 低优先级处理器
bus.subscribe<TradeEvent>(
"trade.alert",
[](auto event) { /* 后台处理 */ },
10// 较低优先级
);
```

### 通配符订阅

```cpp
// 匹配 "trade.stocks", "trade.forex" 等
bus.subscribe<TradeEvent>(
"trade.*",
[](auto event) {
std::cout << "交易事件: " << event->symbol;
}
);
```

### 请求/响应模式

```cpp
// 带响应的处理器
bus.subscribe<AuthRequest>(
"auth.check",
[](std::shared_ptr<AuthRequest> req) {
return std::make_shared<EventBus::TypedResponse<AuthResult>>(
AuthResult{req->valid(), "已处理"}
);
}
);

// 调用方
auto results = bus.post<AuthRequest>(
"auth.check",
AuthRequest{...}
);

auto response = dynamic_cast<TypedResponse<AuthResult>*>(results[0].get());
if(response->isValid()) {
use(response->get());
}
```

## 线程模型

- **订阅操作** 线程安全
- **post()** 阻塞直到所有处理器完成
- **postAsync()** 将事件加入后台队列处理
- **处理器执行**:
- 同步模式: 在发布者线程执行
- 异步模式: 在专用分发线程执行

## 最佳实践

1. **RAII管理订阅**:
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

2. **异常处理**:
```cpp
bus.subscribe<RiskEvent>(
"risk.check",
[](auto event) {
try {
// 高风险操作
return makeResponse(success);
} catch(...) {
return EventBus::Response::invalidResponse();
}
}
);
```

3. **性能优化**:
- 非关键路径使用 `postAsync()`
- 保持处理器简洁
- 尽量在栈上分配事件对象

## 测试构建

```bash
mkdir build && cd build
cmake .. -DEVENTBUS_BUILD_TESTS=ON
make
ctest --output-on-failure
```