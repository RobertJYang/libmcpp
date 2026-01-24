# DBus Signal API

## 概述

`mc::dbus::signal` 模块提供了发射 DBus 标准信号的便捷函数，包括接口添加/移除信号和属性变更信号。同时提供了通过共享内存发送信号的高性能方法。

## 函数定义

```cpp
namespace mc::dbus {
    void send_signal(connection& conn, message& signal);
    void emit_interfaces_added(connection& conn, const engine::abstract_object& obj);
    void emit_interfaces_removed(connection& conn, const engine::abstract_object& obj);
    void emit_properties_changed(connection& conn, engine::abstract_object& obj,
                                const engine::property_base& prop, const variant& value);
}
```

## 信号发送

### send_signal

```cpp
void send_signal(connection& conn, message& signal);
```

通过共享内存或 DBus 发送信号消息。优先使用共享内存发送，如果共享内存不可用或发送失败，则回退到普通 DBus 发送。

**参数：**
- `conn` [in/out] - DBus 连接对象
- `signal` [in] - 待发送的信号消息对象

**功能说明：**
- 自动查找匹配该信号的所有目标接收者（通过共享内存匹配规则）
- 优先通过共享内存消息队列发送信号（如果目标支持共享内存）
- 如果共享内存发送失败或目标不支持共享内存，则通过普通 DBus 连接发送
- 自动设置消息序列号（如果未设置）
- 记录发送耗时日志（debug 级别）

**工作流程：**
1. 在共享内存中查找匹配该信号的所有目标（通过 `shm._matchs.run()`）
2. 收集所有目标的唯一名称（unique_name）和已知名称（wellknow_name）
3. 为每个目标尝试通过共享内存消息队列发送
4. 如果共享内存队列不可用，回退到普通 DBus 发送
5. 释放序列化缓冲区并记录耗时

**约束：**
- 信号消息必须已正确设置路径、接口和成员名称
- 如果信号序列号为 0，会自动设置为连接的下一个序列号
- 共享内存发送失败时会记录错误日志，但不会抛出异常
- 函数会修改 `signal` 对象（设置序列号和目标地址）

**示例：**

```cpp
#include <mc/dbus/signal.h>
#include <mc/dbus/message.h>

// 创建信号消息
auto signal = mc::dbus::message::new_signal(
    "/com/example/Object",
    "com.example.Interface",
    "StatusChanged"
);

// 添加信号参数
auto writer = signal.writer();
writer << "online";  // 状态值

// 通过共享内存或 DBus 发送
mc::dbus::send_signal(conn, signal);
```

**性能说明：**
- 对于支持共享内存的目标，消息通过共享内存队列传递，性能更高
- 对于不支持共享内存的目标，自动回退到普通 DBus 发送
- 函数会记录发送耗时（微秒级），便于性能分析

**注意事项：**
- 函数会修改传入的 `signal` 对象（设置序列号和目标地址）
- 如果需要在发送后继续使用信号对象，需要先创建副本
- 共享内存发送失败时会静默回退到 DBus 发送，不会抛出异常

## 接口信号

### emit_interfaces_added

```cpp
void emit_interfaces_added(connection& conn, const engine::abstract_object& obj);
```

发射接口添加信号。

**参数：**
- `conn` [in/out] - DBus连接对象
- `obj` [in] - 抽象对象

**约束：**
- 用于通知DBus总线某个对象添加了新接口
- 符合 `org.freedesktop.DBus.ObjectManager` 接口规范

**信号详情：**
- **接口名称**：`org.freedesktop.DBus.ObjectManager`
- **信号名称**：`InterfacesAdded`
- **参数**：对象路径、接口及其属性字典

**示例：**

```cpp
// 创建对象并注册到 DBus
my_object obj;
conn.register_path("/com/example/Object", ...);

// 发射接口添加信号
mc::dbus::emit_interfaces_added(conn, obj);
```

### emit_interfaces_removed

```cpp
void emit_interfaces_removed(connection& conn, const engine::abstract_object& obj);
```

发射接口移除信号。

**参数：**
- `conn` [in/out] - DBus连接对象
- `obj` [in] - 抽象对象

**约束：**
- 用于通知DBus总线某个对象移除了接口
- 符合 `org.freedesktop.DBus.ObjectManager` 接口规范

**信号详情：**
- **接口名称**：`org.freedesktop.DBus.ObjectManager`
- **信号名称**：`InterfacesRemoved`
- **参数**：对象路径、接口名称列表

**示例：**

```cpp
// 在对象销毁前发射移除信号
mc::dbus::emit_interfaces_removed(conn, obj);

// 注销对象路径
conn.unregister_path("/com/example/Object");
```

## 属性信号

### emit_properties_changed

```cpp
void emit_properties_changed(connection& conn, 
                            engine::abstract_object& obj,
                            const engine::property_base& prop, 
                            const variant& value);
```

发射属性变更信号。

**参数：**
- `conn` [in/out] - DBus连接对象
- `obj` [in/out] - 抽象对象
- `prop` [in] - 属性基类引用
- `value` [in] - 新属性值

**约束：**
- 用于通知DBus总线某个对象的属性发生了变化
- 符合 `org.freedesktop.DBus.Properties` 接口规范

**信号详情：**
- **接口名称**：`org.freedesktop.DBus.Properties`
- **信号名称**：`PropertiesChanged`
- **参数**：接口名称、变更属性字典、失效属性列表

**示例：**

```cpp
// 属性变更时发射信号
void set_status(const std::string& new_status) {
    m_status = new_status;
    
    // 发射属性变更信号
    mc::dbus::emit_properties_changed(
        m_conn, 
        *this, 
        m_status_property, 
        mc::variant(new_status)
    );
}
```

## 使用场景

### ObjectManager 模式

`InterfacesAdded` 和 `InterfacesRemoved` 信号用于实现 ObjectManager 模式，适用于管理动态对象集合。

```cpp
class device_manager {
public:
    device_manager(mc::dbus::connection& conn) : m_conn(conn) {
        // 注册 ObjectManager 路径
        m_conn.register_path("/com/example/Devices", 
            [this](mc::dbus::message& msg) {
                return handle_message(msg);
            });
    }
    
    void add_device(const std::string& id) {
        // 创建设备对象
        auto device = std::make_shared<device_object>(id);
        m_devices[id] = device;
        
        // 注册设备路径
        std::string path = "/com/example/Devices/" + id;
        m_conn.register_path(path, [device](mc::dbus::message& msg) {
            return device->handle_message(msg);
        });
        
        // 发射接口添加信号
        mc::dbus::emit_interfaces_added(m_conn, *device);
    }
    
    void remove_device(const std::string& id) {
        auto it = m_devices.find(id);
        if (it == m_devices.end()) {
            return;
        }
        
        // 发射接口移除信号
        mc::dbus::emit_interfaces_removed(m_conn, *it->second);
        
        // 注销设备路径
        std::string path = "/com/example/Devices/" + id;
        m_conn.unregister_path(path);
        
        // 移除设备
        m_devices.erase(it);
    }
    
private:
    mc::dbus::connection& m_conn;
    std::map<std::string, std::shared_ptr<device_object>> m_devices;
};
```

### Properties 接口模式

`PropertiesChanged` 信号用于通知属性变更，客户端可以监听此信号而不需要轮询。

```cpp
class sensor_object : public mc::engine::abstract_object {
public:
    sensor_object(mc::dbus::connection& conn) 
        : m_conn(conn), m_temperature(0.0) {}
    
    // 更新温度
    void update_temperature(double temp) {
        if (std::abs(m_temperature - temp) > 0.1) {  // 变化足够大
            m_temperature = temp;
            
            // 发射属性变更信号
            mc::dbus::emit_properties_changed(
                m_conn,
                *this,
                temperature_property(),
                mc::variant(temp)
            );
        }
    }
    
    // 获取温度属性
    double get_temperature() const {
        return m_temperature;
    }
    
    // 属性元数据
    const mc::engine::property_base& temperature_property() const {
        static mc::engine::property_type_info info{
            "Temperature",
            "d",  // double 类型
            mc::engine::property_access::read
        };
        return info;
    }
    
private:
    mc::dbus::connection& m_conn;
    double m_temperature;
};

// 客户端监听属性变更
void monitor_temperature(mc::dbus::connection& conn) {
    auto rule = mc::dbus::match_rule::new_signal(
        "PropertiesChanged",
        "org.freedesktop.DBus.Properties"
    );
    rule.with_path("/com/example/Sensor");
    
    conn.add_match(rule, [](mc::dbus::message& msg) {
        std::string interface;
        std::map<std::string, mc::variant> changed_props;
        std::vector<std::string> invalidated_props;
        
        msg.reader() >> interface >> changed_props >> invalidated_props;
        
        if (changed_props.count("Temperature")) {
            double temp = changed_props["Temperature"].as<double>();
            std::cout << "温度变更: " << temp << "°C" << std::endl;
        }
    }, 1);
}
```

### 完整示例：可观察对象

```cpp
class observable_object : public mc::engine::abstract_object {
public:
    observable_object(mc::dbus::connection& conn, 
                     const std::string& path)
        : m_conn(conn), m_path(path) {
        
        // 注册对象路径
        m_conn.register_path(m_path, [this](mc::dbus::message& msg) {
            return handle_message(msg);
        });
        
        // 发射接口添加信号
        mc::dbus::emit_interfaces_added(m_conn, *this);
    }
    
    ~observable_object() {
        // 发射接口移除信号
        mc::dbus::emit_interfaces_removed(m_conn, *this);
        
        // 注销对象路径
        m_conn.unregister_path(m_path);
    }
    
    // 设置属性并通知
    template<typename T>
    void set_property(const mc::engine::property_base& prop,
                     const T& value) {
        // 更新内部状态...
        
        // 发射属性变更信号
        mc::dbus::emit_properties_changed(
            m_conn, 
            *this, 
            prop, 
            mc::variant(value)
        );
    }
    
private:
    DBusHandlerResult handle_message(mc::dbus::message& msg) {
        // 处理方法调用...
        return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
    }
    
    mc::dbus::connection& m_conn;
    std::string m_path;
};
```

## 标准接口规范

### ObjectManager

ObjectManager 接口用于管理对象集合：

- **接口名称**：`org.freedesktop.DBus.ObjectManager`
- **方法**：
  - `GetManagedObjects()` - 获取所有管理的对象
- **信号**：
  - `InterfacesAdded(object_path, interfaces)` - 接口添加
  - `InterfacesRemoved(object_path, interface_names)` - 接口移除

### Properties

Properties 接口用于属性访问和变更通知：

- **接口名称**：`org.freedesktop.DBus.Properties`
- **方法**：
  - `Get(interface, property)` - 获取属性
  - `Set(interface, property, value)` - 设置属性
  - `GetAll(interface)` - 获取所有属性
- **信号**：
  - `PropertiesChanged(interface, changed_properties, invalidated_properties)` - 属性变更

## 注意事项

1. **信号时机**：在对象状态实际变更后才发射信号
2. **性能考虑**：避免频繁发射信号，可以批量合并变更
3. **接口一致性**：确保信号参数与标准接口规范一致
4. **生命周期**：在对象销毁前发射 InterfacesRemoved 信号
5. **线程安全**：如果从多个线程访问对象，需要适当同步

## 相关接口

- [connection](connection.md) - 连接对象
- [message](message.md) - 消息对象
- [match](match.md) - 消息匹配
