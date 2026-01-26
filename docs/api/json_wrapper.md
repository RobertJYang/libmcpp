# mc::json_wrapper 模块

## 定义

```cpp
namespace mc {
namespace json_wrapper {
    class JsonValue;
    class JsonArray;
    class JsonObject;
}
}
```

## 概述

`mc::json_wrapper` 模块是基于白泽 C JSON 库的 C++ 封装，提供了类型安全的 JSON 操作接口。它使用 RAII 机制管理底层 JSON 对象的生命周期，通过引用计数确保内存安全，并提供了与 `mc::variant` 类型的无缝互转功能。

## 主要特性

- **RAII 内存管理**：自动管理底层 JSON 对象的引用计数，无需手动释放资源
- **类型安全**：提供了强类型的 API，在类型不匹配时会抛出异常
- **与 variant 互转**：支持与 `mc::variant` 类型的双向转换
- **STL 风格接口**：提供迭代器支持，便于使用 C++ 标准算法
- **值语义比较**：支持基于内容的相等比较操作

## 核心类

### JsonValue

`JsonValue` 是 JSON 值的包装类，可以表示 JSON 支持的所有类型：null、bool、number（int/double）、string、array、object。

#### 构造和创建

```cpp
// 默认构造（空值）
JsonValue val1;

// 工厂方法创建不同类型的 JSON 值
JsonValue null_val = JsonValue::make_null();
JsonValue bool_val = JsonValue::make_bool(true);
JsonValue int_val = JsonValue::make_int(42);
JsonValue double_val = JsonValue::make_double(3.14);
JsonValue str_val = JsonValue::make_string("hello");
JsonValue arr_val = JsonValue::make_array();
JsonValue obj_val = JsonValue::make_object();
```

#### 类型判断

```cpp
JsonValue val = JsonValue::make_int(42);

if (val.is_null()) { /* ... */ }
if (val.is_bool()) { /* ... */ }
if (val.is_int()) { /* ... */ }
if (val.is_double()) { /* ... */ }
if (val.is_number()) { /* 包含 is_int() 和 is_double() */ }
if (val.is_string()) { /* ... */ }
if (val.is_array()) { /* ... */ }
if (val.is_object()) { /* ... */ }
```

#### 值读取

```cpp
JsonValue val = JsonValue::make_string("hello");

// 类型不匹配时会抛出 mc::bad_cast_exception
bool b = val.as_bool();        // 如果类型不是 bool，抛出异常
int64_t i = val.as_int();      // 如果类型不是 number，抛出异常
double d = val.as_double();    // 如果类型不是 number，抛出异常
std::string s = val.as_string(); // 如果类型不是 string，抛出异常
JsonArray arr = val.as_array();  // 如果类型不是 array，抛出异常
JsonObject obj = val.as_object(); // 如果类型不是 object，抛出异常
```

#### 与 variant 互转

```cpp
// JsonValue -> variant
JsonValue json_val = JsonValue::make_object();
json_val.as_object().set("name", JsonValue::make_string("test"));
mc::variant var = json_val.to_variant();

// variant -> JsonValue
mc::dict obj{{"name", "test"}, {"age", 30}};
mc::variant var_obj(obj);
JsonValue json_val2 = JsonValue::from_variant(var_obj);
```

#### 相等比较

```cpp
JsonValue val1 = JsonValue::make_int(42);
JsonValue val2 = JsonValue::make_int(42);
JsonValue val3 = JsonValue::make_int(43);

assert(val1 == val2);  // true，按内容比较
assert(val1 != val3);  // true
```

### JsonArray

`JsonArray` 是 JSON 数组的视图类，提供了数组操作接口。

#### 基本操作

```cpp
JsonValue arr_val = JsonValue::make_array();
JsonArray arr = arr_val.as_array();

// 添加元素
arr.push_back(JsonValue::make_int(1));
arr.push_back(JsonValue::make_string("test"));
arr.push_back(JsonValue::make_bool(true));

// 获取大小
uint32_t size = arr.size();
bool is_empty = arr.empty();

// 访问元素
JsonValue first = arr.at(0);      // 越界会抛出异常
JsonValue second = arr[1];        // 等价于 arr.at(1)

// 修改元素
arr.set(0, JsonValue::make_int(100));
```

#### 迭代器支持

```cpp
JsonArray arr = JsonValue::make_array().as_array();
arr.push_back(JsonValue::make_int(1));
arr.push_back(JsonValue::make_int(2));
arr.push_back(JsonValue::make_int(3));

// 使用范围 for 循环
for (const auto& item : arr) {
    int64_t value = item.as_int();
    // ...
}

// 使用迭代器
for (auto it = arr.begin(); it != arr.end(); ++it) {
    JsonValue item = *it;
    // ...
}
```

#### 拷贝语义

`JsonArray` 使用引用计数，拷贝是浅拷贝，多个 `JsonArray` 实例共享同一个底层 JSON 数组：

```cpp
JsonValue arr_val = JsonValue::make_array();
JsonArray arr1 = arr_val.as_array();
arr1.push_back(JsonValue::make_int(42));

JsonArray arr2 = arr1;  // 浅拷贝，共享底层数据
assert(arr2.size() == 1);
assert(arr2[0].as_int() == 42);

arr2.push_back(JsonValue::make_int(43));  // 修改会影响 arr1
assert(arr1.size() == 2);
```

### JsonObject

`JsonObject` 是 JSON 对象的视图类，提供了键值对操作接口。

#### 基本操作

```cpp
JsonValue obj_val = JsonValue::make_object();
JsonObject obj = obj_val.as_object();

// 设置键值对
obj.set("name", JsonValue::make_string("test"));
obj.set("age", JsonValue::make_int(30));
obj.set("active", JsonValue::make_bool(true));

// 检查键是否存在
if (obj.has("name")) {
    // ...
}

// 获取值
JsonValue name_val = obj.get("name");     // 键不存在会抛出异常
JsonValue age_val = obj["age"];           // 等价于 obj.get("age")

// 删除键
obj.erase("age");

// 获取大小
uint32_t size = obj.size();
bool is_empty = obj.empty();
```

#### 迭代器支持

```cpp
JsonObject obj = JsonValue::make_object().as_object();
obj.set("name", JsonValue::make_string("test"));
obj.set("age", JsonValue::make_int(30));

// 使用范围 for 循环
for (const auto& [key, value] : obj) {
    std::cout << key << ": ";
    if (value.is_string()) {
        std::cout << value.as_string();
    } else if (value.is_int()) {
        std::cout << value.as_int();
    }
    std::cout << std::endl;
}

// 使用迭代器
for (auto it = obj.begin(); it != obj.end(); ++it) {
    auto [key, value] = *it;
    // ...
}
```

#### 拷贝语义

与 `JsonArray` 类似，`JsonObject` 也使用引用计数，拷贝是浅拷贝：

```cpp
JsonValue obj_val = JsonValue::make_object();
JsonObject obj1 = obj_val.as_object();
obj1.set("name", JsonValue::make_string("test"));

JsonObject obj2 = obj1;  // 浅拷贝，共享底层数据
assert(obj2.has("name"));
assert(obj2["name"].as_string() == "test");

obj2.set("age", JsonValue::make_int(30));  // 修改会影响 obj1
assert(obj1.has("age"));
```

## 序列化和反序列化函数

`json_wrapper` 模块提供了全局的序列化和反序列化函数，支持 `JsonValue`、`variant`、`dict` 和 `variant` 数组与 JSON 字符串之间的双向转换。

### json_encode

将 C++ 值编码为 JSON 字符串。提供多个重载版本，支持不同的输入类型。

#### 函数签名

```cpp
namespace mc {
namespace json_wrapper {
    // 将 variant 编码为 JSON 字符串
    std::string json_encode(const mc::variant& value, bool pretty_print = false);

    // 将 dict 编码为 JSON 对象字符串
    std::string json_encode(const mc::dict& obj, bool pretty_print = false);

    // 将 variant 数组编码为 JSON 数组字符串
    std::string json_encode(const std::vector<mc::variant>& arr, bool pretty_print = false);

    // 将 JsonValue 编码为 JSON 字符串
    std::string json_encode(const JsonValue& json_val, bool pretty_print = false);
}
}
```

#### 参数

- `value`: `mc::variant` 类型的值，支持所有 variant 支持的类型（nil、bool、int、double、string、dict、variants）
- `obj`: `mc::dict` 类型的对象，会被编码为 JSON 对象
- `arr`: `std::vector<mc::variant>` 类型的数组，会被编码为 JSON 数组
- `json_val`: `JsonValue` 类型的 JSON 值
- `pretty_print`: 是否格式化输出（带缩进和换行），默认为 `false`（紧凑格式）

#### 返回值

返回 JSON 字符串（`std::string`）。

#### 说明

- 这些函数将输入的 C++ 值转换为 JSON 字符串表示
- 当 `pretty_print` 为 `true` 时，输出的 JSON 字符串会带缩进和换行，便于阅读
- 当 `pretty_print` 为 `false` 时，输出的 JSON 字符串是紧凑格式（无缩进和换行）
- JSON 对象的键会按照字符串顺序排序，以保证稳定的顺序
- 如果序列化失败，会抛出 `mc::parse_error_exception` 异常

#### 示例

```cpp
#include <mc/json_wrapper.h>
#include <mc/variant.h>
#include <mc/dict.h>

using namespace mc;
using namespace mc::json_wrapper;

// 示例 1：编码 variant
mc::variant var = dict{{"name", "Alice"}, {"age", 30}, {"active", true}};
std::string json_str1 = json_encode(var);
// 紧凑格式：{"active":true,"age":30,"name":"Alice"}

std::string json_str1_pretty = json_encode(var, true);
// 格式化输出：
// {
//   "active": true,
//   "age": 30,
//   "name": "Alice"
// }

// 示例 2：编码 dict
mc::dict user_obj{{"name", "Bob"}, {"email", "bob@example.com"}};
std::string json_str2 = json_encode(user_obj);
// {"email":"bob@example.com","name":"Bob"}

// 示例 3：编码 variant 数组
std::vector<mc::variant> scores = {85, 92, 78, 88};
std::string json_str3 = json_encode(scores);
// [85,92,78,88]

// 示例 4：编码 JsonValue
JsonValue json_val = JsonValue::make_object();
JsonObject obj = json_val.as_object();
obj.set("x", JsonValue::make_int(10));
obj.set("y", JsonValue::make_int(20));
std::string json_str4 = json_encode(json_val);
// {"x":10,"y":20}

// 示例 5：编码嵌套结构
mc::dict nested{{"user", dict{{"name", "Charlie"}, {"age", 35}}},
                {"tags", variants{"admin", "user"}}};
std::string json_str5 = json_encode(nested, true);
// 格式化输出：
// {
//   "tags": [
//     "admin",
//     "user"
//   ],
//   "user": {
//     "age": 35,
//     "name": "Charlie"
//   }
// }
```

### json_decode

将 JSON 字符串解码为 C++ 值。提供两个版本，分别返回 `variant` 和 `JsonValue`。

#### 函数签名

```cpp
namespace mc {
namespace json_wrapper {
    // 将 JSON 字符串解码为 variant
    mc::variant json_decode(std::string_view json);

    // 将 JSON 字符串解码为 JsonValue
    JsonValue json_decode_raw(const char* json);
}
}
```

#### 参数

- `json`: JSON 字符串（`std::string_view` 或 `const char*`）

#### 返回值

- `json_decode`: 返回 `mc::variant`，表示 JSON 数据
- `json_decode_raw`: 返回 `JsonValue`，保持 JSON 结构和键的顺序

#### 说明

- 这些函数将 JSON 字符串解析为 C++ 值
- JSON null 转换为 variant 的 nil
- JSON boolean 转换为 variant 的 bool
- JSON number 转换为 variant 的 int 或 double（整数转为 int64_t，浮点数转为 double）
- JSON string 转换为 variant 的 string
- JSON array 转换为 variant 的 variants（`std::vector<mc::variant>`）
- JSON object 转换为 variant 的 dict（`mc::dict`）
- 如果解析失败，会抛出 `mc::parse_error_exception` 异常，异常信息中包含错误位置
- `json_decode_raw` 返回 `JsonValue`，适合需要保持 JSON 结构和键顺序的场景

#### 示例

```cpp
#include <mc/json_wrapper.h>
#include <mc/variant.h>
#include <mc/dict.h>

using namespace mc;
using namespace mc::json_wrapper;

// 示例 1：解码 JSON 对象为 variant
std::string json_str1 = R"({"name":"Alice","age":30,"active":true})";
mc::variant var1 = json_decode(json_str1);
mc::dict user = var1.as<dict>();
std::string name = user["name"].as<std::string>();
int64_t age = user["age"].as<int64_t>();
bool active = user["active"].as<bool>();

// 示例 2：解码 JSON 数组为 variant
std::string json_str2 = "[85, 92, 78, 88]";
mc::variant var2 = json_decode(json_str2);
std::vector<mc::variant> scores = var2.as<std::vector<mc::variant>>();
for (const auto& score : scores) {
    int64_t value = score.as<int64_t>();
    // 处理分数
}

// 示例 3：解码嵌套结构
std::string json_str3 = R"({
    "user": {
        "name": "Bob",
        "profile": {
            "email": "bob@example.com",
            "age": 25
        }
    },
    "tags": ["admin", "user"]
})";
mc::variant var3 = json_decode(json_str3);
mc::dict nested = var3.as<dict>();
mc::dict user_obj = nested["user"].as<dict>();
mc::dict profile = user_obj["profile"].as<dict>();

// 示例 4：解码为 JsonValue（保持结构）
std::string json_str4 = R"({"x":10,"y":20})";
JsonValue json_val = json_decode_raw(json_str4.c_str());
JsonObject obj = json_val.as_object();
int64_t x = obj["x"].as_int();
int64_t y = obj["y"].as_int();

// 示例 5：错误处理
try {
    std::string invalid_json = "{invalid json}";
    mc::variant var = json_decode(invalid_json);
} catch (const mc::parse_error_exception& e) {
    // 解析失败，处理异常
    std::cerr << "JSON parsing failed: " << e.what() << std::endl;
}

// 示例 6：在 variant 和 JsonValue 之间转换
std::string json_str5 = R"({"items":[1,2,3]})";

// 解码为 variant
mc::variant var5 = json_decode(json_str5);

// 转换为 JsonValue
JsonValue json_val5 = JsonValue::from_variant(var5);

// 或直接解码为 JsonValue
JsonValue json_val6 = json_decode_raw(json_str5.c_str());
```

### dump

将 C++ 值编码为 JSON 字符串并写入文件。提供多个重载版本，支持不同的输入类型。

#### 函数签名

```cpp
namespace mc {
namespace json_wrapper {
    // 将 variant 编码为 JSON 字符串并写入文件
    bool dump(const mc::variant& value,
              const mc::filesystem::path& file_path,
              bool pretty_print = false);

    // 将 dict 编码为 JSON 对象字符串并写入文件
    bool dump(const mc::dict& obj,
              const mc::filesystem::path& file_path,
              bool pretty_print = false);

    // 将 variant 数组编码为 JSON 数组字符串并写入文件
    bool dump(const std::vector<mc::variant>& arr,
              const mc::filesystem::path& file_path,
              bool pretty_print = false);

    // 将 JsonValue 编码为 JSON 字符串并写入文件
    bool dump(const JsonValue& json_val,
              const mc::filesystem::path& file_path,
              bool pretty_print = false);
}
}
```

#### 参数

- `value`: `mc::variant` 类型的值
- `obj`: `mc::dict` 类型的对象
- `arr`: `std::vector<mc::variant>` 类型的数组
- `json_val`: `JsonValue` 类型的 JSON 值
- `file_path`: 目标文件路径（`mc::filesystem::path`）
- `pretty_print`: 是否格式化输出（带缩进和换行），默认为 `false`（紧凑格式）

#### 返回值

返回 `bool`，成功返回 `true`，失败返回 `false`。

#### 说明

- 这些函数将输入的 C++ 值编码为 JSON 字符串并写入指定文件
- 如果文件已存在，会覆盖原文件
- 如果文件所在的目录不存在，会自动创建目录
- 当 `pretty_print` 为 `true` 时，输出的 JSON 字符串会带缩进和换行，便于阅读
- 当 `pretty_print` 为 `false` 时，输出的 JSON 字符串是紧凑格式（无缩进和换行）
- JSON 对象的键会按照字符串顺序排序，以保证稳定的顺序
- 如果编码或写入文件失败，函数会捕获异常并返回 `false`，不会抛出异常

#### 示例

```cpp
#include <mc/json_wrapper.h>
#include <mc/variant.h>
#include <mc/dict.h>
#include <mc/filesystem.h>

using namespace mc;
using namespace mc::json_wrapper;

// 示例 1：将 variant 保存到文件（紧凑格式）
mc::variant var1 = dict{{"name", "Alice"}, {"age", 30}, {"active", true}};
bool success1 = dump(var1, "/tmp/user.json");
if (success1) {
    // 文件写入成功
}
// 文件内容：{"active":true,"age":30,"name":"Alice"}

// 示例 2：将 dict 保存到文件（格式化输出）
mc::dict user_obj{{"name", "Bob"}, {"email", "bob@example.com"}};
bool success2 = dump(user_obj, "/tmp/user_pretty.json", true);
// 文件内容：
// {
//   "email": "bob@example.com",
//   "name": "Bob"
// }

// 示例 3：将 variant 数组保存到文件
std::vector<mc::variant> scores = {85, 92, 78, 88};
dump(scores, "/tmp/scores.json");
// 文件内容：[85,92,78,88]

// 示例 4：将 JsonValue 保存到文件
JsonValue json_val = JsonValue::make_object();
JsonObject obj = json_val.as_object();
obj.set("x", JsonValue::make_int(10));
obj.set("y", JsonValue::make_int(20));
dump(json_val, "/tmp/coordinates.json");
// 文件内容：{"x":10,"y":20}

// 示例 5：保存嵌套结构（格式化输出）
mc::dict nested{{"user", dict{{"name", "Charlie"}, {"age", 35}}},
                {"tags", variants{"admin", "user"}}};
dump(nested, "/tmp/nested.json", true);
// 文件内容：
// {
//   "tags": [
//     "admin",
//     "user"
//   ],
//   "user": {
//     "age": 35,
//     "name": "Charlie"
//   }
// }
```

## 使用示例

### 示例 1：构建复杂的 JSON 结构

```cpp
#include <mc/json_wrapper.h>

using namespace mc::json_wrapper;

// 构建用户列表 JSON
JsonValue root = JsonValue::make_object();
JsonObject root_obj = root.as_object();

// 创建用户数组
JsonValue users_val = JsonValue::make_array();
JsonArray users = users_val.as_array();

// 创建第一个用户
JsonValue user1_val = JsonValue::make_object();
JsonObject user1 = user1_val.as_object();
user1.set("name", JsonValue::make_string("Alice"));
user1.set("age", JsonValue::make_int(30));
users.push_back(user1_val);

// 创建第二个用户
JsonValue user2_val = JsonValue::make_object();
JsonObject user2 = user2_val.as_object();
user2.set("name", JsonValue::make_string("Bob"));
user2.set("age", JsonValue::make_int(25));
users.push_back(user2_val);

// 将数组添加到根对象
root_obj.set("users", users_val);
root_obj.set("count", JsonValue::make_int(users.size()));
```

### 示例 2：从 variant 转换并处理

```cpp
#include <mc/json_wrapper.h>
#include <mc/variant.h>
#include <mc/dict.h>

using namespace mc;
using namespace mc::json_wrapper;

// 从 variant 创建 JSON
dict user_obj{{"name", "test"}, {"age", 30}, {"scores", variants{85, 92, 78}}};
variant user_var(user_obj);

JsonValue json_val = JsonValue::from_variant(user_var);

// 访问和修改
JsonObject obj = json_val.as_object();
if (obj.has("name")) {
    std::string name = obj["name"].as_string();
}

JsonArray scores = obj["scores"].as_array();
scores.push_back(JsonValue::make_int(88));

// 转换回 variant
variant result = json_val.to_variant();
```

### 示例 3：遍历和修改 JSON 结构

```cpp
#include <mc/json_wrapper.h>

using namespace mc::json_wrapper;

void process_json_object(JsonObject& obj) {
    // 遍历所有键值对
    for (const auto& [key, value] : obj) {
        if (value.is_array()) {
            JsonArray arr = value.as_array();
            // 处理数组
            for (const auto& item : arr) {
                if (item.is_int()) {
                    int64_t val = item.as_int();
                    // 处理整数值
                }
            }
        } else if (value.is_object()) {
            JsonObject nested_obj = value.as_object();
            // 递归处理嵌套对象
            process_json_object(nested_obj);
        }
    }
}
```

## 异常处理

`json_wrapper` 模块在以下情况下会抛出异常：

- **类型转换失败**：当调用 `as_*()` 方法但类型不匹配时，抛出 `mc::bad_cast_exception`
- **访问越界**：当数组索引超出范围时，抛出 `mc::out_of_range_exception`
- **底层 API 错误**：当底层 JSON 库操作失败时，抛出 `mc::parse_error_exception`

建议使用 try-catch 块处理这些异常：

```cpp
try {
    JsonValue val = JsonValue::make_string("hello");
    int64_t i = val.as_int();  // 抛出 bad_cast_exception
} catch (const mc::bad_cast_exception& e) {
    // 处理类型转换错误
} catch (const mc::exception& e) {
    // 处理其他异常
}
```

## 最佳实践

### 1. 使用工厂方法创建 JSON 值

优先使用 `JsonValue::make_*()` 静态工厂方法创建 JSON 值，而不是直接构造：

```cpp
// ✅ 推荐
JsonValue val = JsonValue::make_int(42);

// ❌ 不推荐（需要手动管理底层指针）
// Json* raw_json = ...;
// JsonValue val(raw_json);
```

### 2. 检查类型后再转换

在使用 `as_*()` 方法之前，先使用 `is_*()` 方法检查类型：

```cpp
JsonValue val = get_value_from_somewhere();

if (val.is_string()) {
    std::string str = val.as_string();
    // ...
} else if (val.is_int()) {
    int64_t i = val.as_int();
    // ...
}
```

### 3. 利用与 variant 的互转

当需要与其他模块（如配置系统、序列化系统）交互时，可以先将 `JsonValue` 转换为 `variant`：

```cpp
JsonValue json_val = build_complex_json();
mc::variant var = json_val.to_variant();

// 可以传递给其他接受 variant 的接口
config_manager.set("key", var);

// 需要时再转换回来
JsonValue json_val2 = JsonValue::from_variant(var);
```

### 4. 注意引用计数的共享语义

`JsonArray` 和 `JsonObject` 使用引用计数，拷贝是浅拷贝。如果需要一个独立的副本，应该从 variant 重新构建：

```cpp
JsonValue original = JsonValue::make_object();
original.as_object().set("key", JsonValue::make_string("value"));

// 通过 variant 创建独立副本
mc::variant var = original.to_variant();
JsonValue copy = JsonValue::from_variant(var);

// 现在修改 copy 不会影响 original
copy.as_object().set("key", JsonValue::make_string("new_value"));
```

### 5. 使用范围 for 循环遍历

优先使用 C++11 的范围 for 循环遍历数组和对象：

```cpp
// ✅ 推荐
for (const auto& item : array) {
    // ...
}

for (const auto& [key, value] : object) {
    // ...
}

// ❌ 不推荐（虽然也可以）
for (auto it = array.begin(); it != array.end(); ++it) {
    // ...
}
```

## 注意事项

1. **线程安全**：`json_wrapper` 类不是线程安全的。在多线程环境中使用时，需要采取适当的同步措施。

2. **引用计数**：底层 JSON 对象使用引用计数管理生命周期。当最后一个 `JsonValue`、`JsonArray` 或 `JsonObject` 实例被销毁时，底层 JSON 对象才会被释放。

3. **空值处理**：默认构造的 `JsonValue` 是空值（null），在调用 `as_*()` 方法时需要先检查类型。

4. **字符串视图**：`make_string()` 方法接受 `std::string_view` 参数，可以高效处理字符串字面量和 `std::string`。

5. **相等比较**：`operator==` 进行的是内容比较，而不是指针比较。即使两个 `JsonValue` 对象指向不同的底层 JSON 对象，只要内容相同，`operator==` 就会返回 `true`。

## 相关文档

- [mc::variant 文档](variant.md) - 通用变量类型系统
- [mc::dict 文档](dict.md) - 键值对集合
- [JSON 包装器测试说明](../../tests/test_json_wrapper.cpp)
