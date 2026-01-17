# DBus Match API

## 概述

`mc::dbus::match` 和 `mc::dbus::match_rule` 用于创建和管理 DBus 消息匹配规则，实现消息过滤和信号监听功能。

## 类定义

```cpp
namespace mc::dbus {
    class match_rule;
    class match;
}
```

## Match Rule

`match_rule` 定义消息匹配规则。

### 创建匹配规则

#### 构造函数

```cpp
match_rule(DBus::Match::MessageType type, 
           const std::string_view& member,
           const std::string_view& interface);
```

创建匹配规则。

**参数：**
- `type` [in] - 消息类型
- `member` [in] - 成员名称
- `interface` [in] - 接口名称

#### new_signal

```cpp
static match_rule new_signal(const std::string_view& member, 
                              const std::string_view& interface);
```

创建信号匹配规则（便捷方法）。

**参数：**
- `member` [in] - 信号名称
- `interface` [in] - 接口名称

**返回值：**
- 返回匹配规则对象

**示例：**

```cpp
// 监听 NameOwnerChanged 信号
auto rule = mc::dbus::match_rule::new_signal(
    "NameOwnerChanged",
    "org.freedesktop.DBus"
);
```

### 配置匹配条件

匹配规则支持链式调用配置多个匹配条件。

#### with_interface

```cpp
void with_interface(const std::string_view& interface);
```

设置匹配接口。

**参数：**
- `interface` [in] - 接口名称

#### with_member

```cpp
void with_member(const std::string_view& member);
```

设置匹配成员（方法名或信号名）。

**参数：**
- `member` [in] - 成员名称

#### with_path

```cpp
void with_path(const std::string_view& path);
```

设置匹配对象路径。

**参数：**
- `path` [in] - 对象路径

#### with_path_namespace

```cpp
void with_path_namespace(const std::string_view& path_namespace);
```

设置匹配路径命名空间。

**参数：**
- `path_namespace` [in] - 路径命名空间

**约束：**
- 匹配该命名空间下的所有路径

**示例：**

```cpp
// 匹配 /com/example/ 下的所有对象
rule.with_path_namespace("/com/example/");
```

#### with_sender

```cpp
void with_sender(const std::string_view& sender);
```

设置匹配发送者。

**参数：**
- `sender` [in] - 发送者名称

#### with_type

```cpp
void with_type(DBus::Match::MessageType type);
```

设置匹配消息类型。

**参数：**
- `type` [in] - 消息类型

#### with_destination

```cpp
void with_destination(const std::string_view& destination);
```

设置匹配目标。

**参数：**
- `destination` [in] - 目标服务名称

### 其他方法

#### clone

```cpp
match_rule clone() const;
```

克隆匹配规则。

**返回值：**
- 返回克隆的匹配规则对象

#### as_string

```cpp
std::string as_string() const;
```

将匹配规则转换为字符串。

**返回值：**
- 返回匹配规则的字符串表示

**示例：**

```cpp
auto rule = mc::dbus::match_rule::new_signal("NameOwnerChanged", 
                                             "org.freedesktop.DBus");
std::cout << "匹配规则: " << rule.as_string() << std::endl;
// 输出: type='signal',interface='org.freedesktop.DBus',member='NameOwnerChanged'
```

## Match

`match` 用于管理多个匹配规则并分发匹配的消息。

### 构造函数

```cpp
match();
```

创建匹配对象。

### 添加和移除规则

#### add_rule

```cpp
void add_rule(match_rule& rule, match_cb_t&& cb, uint64_t id);
```

添加匹配规则。

**参数：**
- `rule` [in] - 匹配规则
- `cb` [in] - 匹配成功时的回调函数
- `id` [in] - 规则的唯一标识符

**回调函数类型：**

```cpp
using match_cb_t = std::function<void(mc::dbus::message&)>;
```

**示例：**

```cpp
mc::dbus::match matcher;
auto rule = mc::dbus::match_rule::new_signal("StatusChanged", 
                                             "com.example.Interface");

matcher.add_rule(rule, [](mc::dbus::message& msg) {
    std::cout << "收到状态变更信号" << std::endl;
    std::string status;
    msg.reader() >> status;
    std::cout << "新状态: " << status << std::endl;
}, 1);
```

#### remove_rule

```cpp
void remove_rule(uint64_t id);
```

移除匹配规则。

**参数：**
- `id` [in] - 规则的唯一标识符

### 消息匹配

#### run_msg

```cpp
bool run_msg(DBusMessage* msg);
```

运行消息匹配，执行匹配的回调。

**参数：**
- `msg` [in] - DBus消息

**返回值：**
- 匹配成功并执行回调返回 true，否则返回 false

#### test_match

```cpp
bool test_match(DBusMessage* msg);
```

测试消息是否匹配（不执行回调）。

**参数：**
- `msg` [in] - DBus消息

**返回值：**
- 匹配返回 true，否则返回 false

## 使用示例

### 监听信号

```cpp
#include <mc/dbus/connection.h>
#include <mc/dbus/match.h>

// 创建连接
mc::io_context executor(1);
auto conn = mc::dbus::connection::open_system_bus(executor);
conn.start();

// 创建信号匹配规则
auto rule = mc::dbus::match_rule::new_signal(
    "NameOwnerChanged",
    "org.freedesktop.DBus"
);

// 添加匹配规则
conn.add_match(rule, [](mc::dbus::message& msg) {
    std::string name, old_owner, new_owner;
    msg.reader() >> name >> old_owner >> new_owner;
    
    std::cout << "服务 " << name << " 所有者变更" << std::endl;
    std::cout << "旧所有者: " << old_owner << std::endl;
    std::cout << "新所有者: " << new_owner << std::endl;
}, 1);

// 运行事件循环
executor.run();
```

### 多条件匹配

```cpp
// 创建复杂匹配规则
auto rule = mc::dbus::match_rule::new_signal("PropertyChanged", 
                                             "com.example.Interface");

// 添加多个条件
rule.with_path("/com/example/Object");
rule.with_sender("com.example.Service");

conn.add_match(rule, [](mc::dbus::message& msg) {
    // 处理属性变更信号
    std::string property;
    mc::variant value;
    msg.reader() >> property >> value;
    
    std::cout << "属性 " << property << " 变更为: " 
              << value.as<std::string>() << std::endl;
}, 2);
```

### 路径命名空间匹配

```cpp
// 匹配特定命名空间下的所有信号
auto rule = mc::dbus::match_rule::new_signal("StateChanged", 
                                             "com.example.Device");
rule.with_path_namespace("/com/example/devices/");

conn.add_match(rule, [](mc::dbus::message& msg) {
    auto path = msg.get_path();
    std::cout << "设备 " << path << " 状态变更" << std::endl;
}, 3);
```

### 管理多个匹配规则

```cpp
class signal_manager {
public:
    signal_manager(mc::dbus::connection& conn) : m_conn(conn) {}
    
    void add_status_monitor(uint64_t id) {
        auto rule = mc::dbus::match_rule::new_signal(
            "StatusChanged", 
            "com.example.Service"
        );
        
        m_conn.add_match(rule, [this](mc::dbus::message& msg) {
            handle_status_change(msg);
        }, id);
        
        m_rule_ids.push_back(id);
    }
    
    void add_error_monitor(uint64_t id) {
        auto rule = mc::dbus::match_rule::new_signal(
            "ErrorOccurred", 
            "com.example.Service"
        );
        
        m_conn.add_match(rule, [this](mc::dbus::message& msg) {
            handle_error(msg);
        }, id);
        
        m_rule_ids.push_back(id);
    }
    
    void remove_all() {
        for (auto id : m_rule_ids) {
            m_conn.remove_match(id);
        }
        m_rule_ids.clear();
    }
    
private:
    void handle_status_change(mc::dbus::message& msg) {
        // 处理状态变更
    }
    
    void handle_error(mc::dbus::message& msg) {
        // 处理错误
    }
    
    mc::dbus::connection& m_conn;
    std::vector<uint64_t> m_rule_ids;
};
```

### 动态添加和移除规则

```cpp
class dynamic_listener {
public:
    dynamic_listener(mc::dbus::connection& conn) 
        : m_conn(conn), m_next_id(1) {}
    
    uint64_t subscribe(const std::string& signal_name,
                       const std::string& interface,
                       std::function<void(mc::dbus::message&)> callback) {
        auto rule = mc::dbus::match_rule::new_signal(signal_name, interface);
        uint64_t id = m_next_id++;
        
        m_conn.add_match(rule, std::move(callback), id);
        return id;
    }
    
    void unsubscribe(uint64_t id) {
        m_conn.remove_match(id);
    }
    
private:
    mc::dbus::connection& m_conn;
    uint64_t m_next_id;
};

// 使用
dynamic_listener listener(conn);

// 订阅
auto id1 = listener.subscribe("StatusChanged", "com.example.Service",
    [](mc::dbus::message& msg) {
        std::cout << "状态变更" << std::endl;
    });

auto id2 = listener.subscribe("ValueChanged", "com.example.Sensor",
    [](mc::dbus::message& msg) {
        std::cout << "值变更" << std::endl;
    });

// 取消订阅
listener.unsubscribe(id1);
```

## 注意事项

1. **规则ID唯一性**：确保每个规则的ID唯一，避免冲突
2. **及时移除**：不再需要时及时移除匹配规则，避免内存泄漏
3. **回调线程**：回调函数在 IO 线程中执行，避免长时间阻塞
4. **规则字符串**：DBus 匹配规则有特定语法，使用 API 构建避免错误
5. **权限检查**：某些信号可能需要权限才能监听

## 相关接口

- [connection](connection.md) - 连接对象
- [message](message.md) - 消息对象
- [signal](signal.md) - 信号发射
