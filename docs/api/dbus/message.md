# DBus Message API

## 概述

`mc::dbus::message` 是 DBus 消息对象，用于表示和操作 DBus 消息。支持方法调用、方法返回、错误和信号等消息类型。

## 类定义

```cpp
namespace mc::dbus {
    class message;
    struct message_reader;
    struct message_writer;
}
```

## 创建消息

### 静态工厂方法

#### new_method_call

```cpp
static message new_method_call(std::string_view destination, 
                               std::string_view path,
                               std::string_view interface, 
                               std::string_view member);
```

创建方法调用消息。

**参数：**
- `destination` [in] - 目标服务名称
- `path` [in] - 对象路径
- `interface` [in] - 接口名称
- `member` [in] - 方法名称

**返回值：**
- 返回方法调用消息对象

**示例：**

```cpp
auto msg = mc::dbus::message::new_method_call(
    "org.freedesktop.DBus",
    "/org/freedesktop/DBus",
    "org.freedesktop.DBus",
    "ListNames"
);
```

#### new_method_return

```cpp
static message new_method_return(const message& msg);
```

创建方法返回消息。

**参数：**
- `msg` [in] - 原始方法调用消息

**返回值：**
- 返回方法返回消息对象

**示例：**

```cpp
auto reply = mc::dbus::message::new_method_return(original_msg);
reply.writer() << "result";
```

#### new_error

```cpp
static message new_error(const message& msg, 
                         std::string_view error_name,
                         std::string_view error_message = {});
```

创建错误消息。

**参数：**
- `msg` [in] - 原始消息
- `error_name` [in] - 错误名称
- `error_message` [in] - 错误消息内容（可选）

**返回值：**
- 返回错误消息对象

**示例：**

```cpp
auto error_msg = mc::dbus::message::new_error(
    original_msg,
    "com.example.Error.Failed",
    "操作失败"
);
```

#### new_signal

```cpp
static message new_signal(std::string_view path, 
                          std::string_view interface,
                          std::string_view member);
```

创建信号消息。

**参数：**
- `path` [in] - 对象路径
- `interface` [in] - 接口名称
- `member` [in] - 信号名称

**返回值：**
- 返回信号消息对象

**示例：**

```cpp
auto sig = mc::dbus::message::new_signal(
    "/com/example/Object",
    "com.example.Interface",
    "StatusChanged"
);
```

## 消息属性

### 获取属性

#### get_type

```cpp
message_type get_type() const;
```

获取消息类型。

**返回值：**
- 返回消息类型枚举值（method_call, method_return, error, signal）

#### get_path

```cpp
std::string_view get_path() const;
```

获取对象路径。

#### get_interface

```cpp
std::string_view get_interface() const;
```

获取接口名称。

#### get_member

```cpp
std::string_view get_member() const;
```

获取成员名称（方法名或信号名）。

#### get_sender

```cpp
std::string_view get_sender() const;
```

获取发送者名称。

#### get_destination

```cpp
std::string_view get_destination() const;
```

获取目标服务名称。

#### get_signature

```cpp
std::string_view get_signature() const;
```

获取消息签名（参数类型字符串）。

#### get_serial

```cpp
uint32_t get_serial() const;
```

获取消息序列号。

#### get_reply_serial

```cpp
uint32_t get_reply_serial() const;
```

获取回复消息的序列号。

#### get_error_name

```cpp
std::string_view get_error_name() const;
```

获取错误名称（仅错误消息有效）。

#### get_error_message

```cpp
std::string get_error_message() const;
```

获取错误消息内容（仅错误消息有效）。

### 设置属性

#### set_path

```cpp
void set_path(std::string_view path);
```

设置对象路径。

#### set_interface

```cpp
void set_interface(std::string_view interface);
```

设置接口名称。

#### set_member

```cpp
void set_member(std::string_view member);
```

设置成员名称。

#### set_destination

```cpp
void set_destination(std::string_view destination);
```

设置目标服务名称。

#### set_sender

```cpp
void set_sender(std::string_view sender);
```

设置发送者名称。

#### set_serial

```cpp
void set_serial(uint32_t serial);
```

设置消息序列号。

## 消息状态检查

### is_valid

```cpp
bool is_valid() const;
```

检查消息是否有效。

### is_error

```cpp
bool is_error() const;
```

检查是否为错误消息。

### is_method_call

```cpp
bool is_method_call() const;
```

检查是否为方法调用消息。

### is_method_return

```cpp
bool is_method_return() const;
```

检查是否为方法返回消息。

### is_signal

```cpp
bool is_signal() const;
```

检查是否为信号消息。

### has_signature

```cpp
bool has_signature(std::string_view signature);
```

检查消息签名是否匹配。

**参数：**
- `signature` [in] - 待检查的签名

**返回值：**
- 匹配返回 true，否则返回 false

## 读写消息参数

### reader

```cpp
message_reader reader() const;
```

获取消息读取器。

**返回值：**
- 返回 message_reader 对象

### writer

```cpp
message_writer writer();
```

获取消息写入器。

**返回值：**
- 返回 message_writer 对象

### as

```cpp
template <typename T>
T as() const;
```

将消息内容转换为指定类型。

**模板参数：**
- `T` - 目标类型

**返回值：**
- 返回转换后的对象

**异常：**
- 类型转换失败时抛出异常

**示例：**

```cpp
// 读取字符串数组
auto names = reply.as<std::vector<std::string>>();

// 读取单个值
auto value = reply.as<int>();

// 读取结构体
auto person = reply.as<person_struct>();
```

### from

```cpp
template <typename T>
void from(const T& v);
```

从指定对象写入消息内容。

**模板参数：**
- `T` - 源对象类型

**参数：**
- `v` [in] - 源对象

**示例：**

```cpp
person_struct p{"张三", 30};
msg.from(p);
```

### read_args

```cpp
mc::variants read_args() const;
```

读取消息参数为 variant 数组。

**返回值：**
- 返回消息参数的 variant 数组

## Message Reader

### 创建 Reader

```cpp
message_reader reader = msg.reader();
```

### 读取基本类型

使用 `operator>>` 读取：

```cpp
message_reader reader = msg.reader();

std::string name;
int age;
bool is_student;

reader >> name >> age >> is_student;
```

### 读取容器类型

```cpp
// 读取数组
std::vector<std::string> names;
reader >> names;

// 读取字典
std::map<std::string, int> scores;
reader >> scores;

// 读取元组
std::tuple<std::string, int, bool> data;
reader >> data;

// 读取可选值
std::optional<std::string> email;
reader >> email;
```

### 读取反射类型

```cpp
struct person {
    std::string name;
    int age;
    bool is_student;
};
MC_REFLECT(person, name, age, is_student);

person p;
reader >> p;
```

### Reader 方法

#### at_end

```cpp
bool at_end() const;
```

检查是否到达末尾。

#### next

```cpp
const message_reader& next() const;
```

移动到下一个元素。

#### recurse

```cpp
void recurse(const message_reader& parent) const;
```

递归进入容器。

#### current_type

```cpp
type_code current_type() const;
```

获取当前类型码。

## Message Writer

### 创建 Writer

```cpp
message_writer writer = msg.writer();
```

### 写入基本类型

使用 `operator<<` 写入：

```cpp
message_writer writer = msg.writer();

writer << "张三" << 30 << true;
```

### 写入容器类型

```cpp
// 写入数组
std::vector<std::string> names = {"Alice", "Bob", "Charlie"};
writer << names;

// 写入字典
std::map<std::string, int> scores = {{"Alice", 95}, {"Bob", 87}};
writer << scores;

// 写入元组
std::tuple<std::string, int, bool> data{"张三", 30, false};
writer << data;

// 写入可选值
std::optional<std::string> email = "user@example.com";
writer << email;
```

### 写入反射类型

```cpp
struct person {
    std::string name;
    int age;
    bool is_student;
};
MC_REFLECT(person, name, age, is_student);

person p{"张三", 30, false};
writer << p;
```

### Writer 方法

#### append

```cpp
template <typename T>
const message_writer& append(const T& v) const;
```

追加值（支持链式调用）。

#### write_path

```cpp
void write_path(const path& p) const;
void write_path(std::string_view p, bool need_add_tail_zero = true) const;
```

写入对象路径。

#### write_signature

```cpp
void write_signature(const signature& sig) const;
void write_signature(std::string_view sig, bool need_add_tail_zero = true) const;
```

写入签名。

## 序列化与反序列化

### marshal

```cpp
std::pair<dbus_ptr<char>, std::size_t> marshal();
```

序列化消息。

**返回值：**
- 返回序列化后的数据和长度

### demarshal

```cpp
bool demarshal(const std::vector<uint8_t>& in, error& err);
bool demarshal(const char* in, std::size_t len, error& err);
```

反序列化消息。

**参数：**
- `in` [in] - 输入数据
- `len` [in] - 数据长度（第二个重载）
- `err` [out] - 错误信息

**返回值：**
- 成功返回 true，失败返回 false

## 使用示例

### 发送方法调用

```cpp
// 创建消息
auto msg = mc::dbus::message::new_method_call(
    "com.example.Service",
    "/com/example/Object",
    "com.example.Interface",
    "Calculate"
);

// 写入参数
auto writer = msg.writer();
writer << 10 << 20;

// 发送并接收回复
auto reply = conn.send_with_reply_and_block(std::move(msg));

// 读取结果
auto reader = reply.reader();
int result;
reader >> result;
std::cout << "结果: " << result << std::endl;
```

### 发送信号

```cpp
// 创建信号
auto sig = mc::dbus::message::new_signal(
    "/com/example/Object",
    "com.example.Interface",
    "StatusChanged"
);

// 写入信号参数
sig.writer() << "running" << 100;

// 发送信号
conn.send(std::move(sig));
```

### 处理错误消息

```cpp
if (reply.is_error()) {
    std::cerr << "错误: " << reply.get_error_name() 
              << " - " << reply.get_error_message() << std::endl;
    return;
}
```

### 使用复杂类型

```cpp
// 定义结构体
struct config {
    std::string name;
    std::map<std::string, std::string> settings;
    std::vector<int> ports;
};
MC_REFLECT(config, name, settings, ports);

// 发送结构体
config cfg{"MyService", {{"host", "localhost"}}, {8080, 8081}};
auto msg = mc::dbus::message::new_method_call(...);
msg.writer() << cfg;

// 接收结构体
auto reply = conn.send_with_reply_and_block(std::move(msg));
config result;
reply.reader() >> result;
```

## 类型签名

### 常用类型签名

| C++类型 | DBus签名 | 说明 |
|---------|----------|------|
| bool | b | 布尔值 |
| int8_t, uint8_t | y | 字节 |
| int16_t | n | 16位整数 |
| uint16_t | q | 16位无符号整数 |
| int32_t | i | 32位整数 |
| uint32_t | u | 32位无符号整数 |
| int64_t | x | 64位整数 |
| uint64_t | t | 64位无符号整数 |
| double | d | 双精度浮点数 |
| std::string | s | 字符串 |
| mc::engine::path | o | 对象路径 |
| mc::reflect::signature | g | 签名 |
| std::vector<T> | a + T的签名 | 数组 |
| std::tuple<T...> | (T的签名...) | 结构体 |
| std::pair<K,V> | {K的签名 + V的签名} | 字典项 |
| std::map<K,V> | a{K的签名 + V的签名} | 字典 |
| mc::variant | v | Variant |

### 示例

```cpp
// "si" - string, int32
msg.writer() << "hello" << 42;

// "a{ss}" - array of dict entries (string->string)
std::map<std::string, std::string> dict = {{"key", "value"}};
msg.writer() << dict;

// "(sib)" - struct with string, int32, boolean
std::tuple<std::string, int, bool> data{"test", 100, true};
msg.writer() << data;
```

## 注意事项

1. **消息生命周期**：发送消息使用移动语义，发送后原消息对象不可用
2. **类型匹配**：读取时必须与写入时的类型匹配，否则会抛出异常
3. **签名验证**：使用 `has_signature()` 验证消息签名避免类型错误
4. **错误处理**：始终检查 `is_error()` 和 `is_valid()`
5. **内存管理**：message 对象会自动管理内存，无需手动释放

## 相关接口

- [connection](connection.md) - 连接对象
- [error](error.md) - 错误处理
- [validator](validator.md) - 消息验证
