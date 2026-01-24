# LRU 缓存模块 API 文档

## 概述

`mc::algorithm::lru_cache` 是一个高性能的 LRU (Least Recently Used) 缓存模板类，提供 O(1) 时间复杂度的操作。

## 头文件

```cpp
#include <mc/algorithm/lru_cache.h>
```

## 类定义

```cpp
namespace mc::algorithm {

template <typename Key, typename Value>
class lru_cache {
public:
    using eviction_callback = std::function<void(const Key&, Value&&)>;
    
    // 构造函数和析构函数
    explicit lru_cache(size_t max_size = 0, eviction_callback on_evict = nullptr);
    
    // 核心操作
    std::optional<std::reference_wrapper<Value>> get(const Key& key);
    void put(const Key& key, Value value);
    bool erase(const Key& key);
    void clear();
    
    // 容量管理
    size_t size() const;
    size_t max_size() const;
    void set_max_size(size_t max_size);
    
    // 查询操作
    bool contains(const Key& key) const;
    bool empty() const;
    
    // 回调管理
    void set_eviction_callback(eviction_callback on_evict);
    
    // 遍历操作
    template <typename Func>
    void for_each(Func&& func) const;
};

} // namespace mc::algorithm
```

## 特性

### 1. 高性能

- **O(1) 时间复杂度**：所有核心操作（get、put、erase）均为 O(1)
- **哈希表 + 双向链表**：使用 std::unordered_map 和 std::list 实现
- **内存高效**：最小化额外开销

### 2. 灵活性

- **泛型设计**：支持任意键值类型
- **可配置容量**：支持运行时动态调整
- **淘汰回调**：支持自定义淘汰时的清理逻辑

### 3. 易用性

- **简洁 API**：接口设计简单直观
- **标准兼容**：使用 std::optional 等标准库特性
- **非侵入式**：不要求值类型继承特定基类

## API 详解

### 构造函数

#### lru_cache

```cpp
explicit lru_cache(size_t max_size = 0, eviction_callback on_evict = nullptr);
```

**功能**：构造 LRU 缓存对象

**参数**：
- `max_size`：最大缓存容量，0 表示不限制
- `on_evict`：可选的淘汰回调函数

**示例**：

```cpp
// 创建容量为 100 的缓存
mc::algorithm::lru_cache<int, std::string> cache(100);

// 创建带淘汰回调的缓存
auto callback = [](const int& key, std::string&& value) {
    ilog("淘汰: ${key}", ("key", key));
};
mc::algorithm::lru_cache<int, std::string> cache_with_callback(100, callback);

// 创建不限制容量的缓存
mc::algorithm::lru_cache<int, std::string> unlimited_cache(0);
```

---

### 核心操作

#### get

```cpp
std::optional<std::reference_wrapper<Value>> get(const Key& key);
```

**功能**：获取缓存值，并将该条目标记为最近使用

**参数**：
- `key`：要查询的键

**返回值**：
- 如果键存在，返回值的引用（wrapped in std::optional）
- 如果键不存在，返回 std::nullopt

**注意**：获取操作会更新该条目的访问时间

**示例**：

```cpp
auto val = cache.get(42);
if (val.has_value()) {
    std::string& str = val->get();
    std::cout << str << std::endl;
}
```

---

#### put

```cpp
void put(const Key& key, Value value);
```

**功能**：添加或更新缓存值

**参数**：
- `key`：键
- `value`：值

**行为**：
- 如果键已存在，更新值并标记为最近使用
- 如果键不存在且缓存已满，自动淘汰最久未使用的条目
- 新条目被标记为最近使用

**示例**：

```cpp
cache.put(1, "one");
cache.put(2, "two");
cache.put(1, "ONE");  // 更新键 1 的值
```

---

#### erase

```cpp
bool erase(const Key& key);
```

**功能**：删除指定键的缓存

**参数**：
- `key`：要删除的键

**返回值**：
- 如果键存在则返回 true
- 如果键不存在则返回 false

**示例**：

```cpp
if (cache.erase(42)) {
    std::cout << "键 42 已删除" << std::endl;
}
```

---

#### clear

```cpp
void clear();
```

**功能**：清空所有缓存

**示例**：

```cpp
cache.clear();
assert(cache.empty());
```

---

### 容量管理

#### size

```cpp
size_t size() const;
```

**功能**：获取当前缓存的条目数量

**返回值**：当前缓存的条目数量

**示例**：

```cpp
size_t current_size = cache.size();
ilog("当前缓存大小: ${size}", ("size", current_size));
```

---

#### max_size

```cpp
size_t max_size() const;
```

**功能**：获取最大缓存容量

**返回值**：最大缓存容量，0 表示不限制

**示例**：

```cpp
size_t max = cache.max_size();
ilog("最大容量: ${max}", ("max", max));
```

---

#### set_max_size

```cpp
void set_max_size(size_t max_size);
```

**功能**：设置最大缓存容量

**参数**：
- `max_size`：新的最大容量，0 表示不限制

**注意**：如果新容量小于当前大小，会立即淘汰多余的条目

**示例**：

```cpp
cache.set_max_size(50);  // 设置最大容量为 50

// 如果当前有 100 个条目，会立即淘汰 50 个最久未使用的条目
```

---

### 查询操作

#### contains

```cpp
bool contains(const Key& key) const;
```

**功能**：检查键是否存在

**参数**：
- `key`：要检查的键

**返回值**：如果键存在返回 true，否则返回 false

**注意**：此操作不会更新访问时间

**示例**：

```cpp
if (cache.contains(42)) {
    std::cout << "键 42 存在" << std::endl;
}
```

---

#### empty

```cpp
bool empty() const;
```

**功能**：判断缓存是否为空

**返回值**：如果缓存为空返回 true

**示例**：

```cpp
if (cache.empty()) {
    std::cout << "缓存为空" << std::endl;
}
```

---

### 回调管理

#### set_eviction_callback

```cpp
void set_eviction_callback(eviction_callback on_evict);
```

**功能**：设置淘汰回调函数

**参数**：
- `on_evict`：淘汰回调函数，签名为 `void(const Key&, Value&&)`

**示例**：

```cpp
cache.set_eviction_callback([](const int& key, std::string&& value) {
    ilog("淘汰: ${key} = ${value}", ("key", key)("value", value));
});
```

---

### 遍历操作

#### for_each

```cpp
template <typename Func>
void for_each(Func&& func) const;
```

**功能**：遍历所有缓存条目（按访问时间从新到旧）

**参数**：
- `func`：遍历函数，签名为 `void(const Key&, const Value&)`

**注意**：遍历顺序为从最近使用到最久未使用

**示例**：

```cpp
cache.for_each([](const int& key, const std::string& value) {
    std::cout << key << ": " << value << std::endl;
});
```

---

## 使用示例

### 基本使用

```cpp
#include <mc/algorithm/lru_cache.h>

// 创建一个容量为 3 的缓存
mc::algorithm::lru_cache<int, std::string> cache(3);

// 添加条目
cache.put(1, "one");
cache.put(2, "two");
cache.put(3, "three");

// 获取条目
auto val = cache.get(1);
if (val.has_value()) {
    std::cout << val->get() << std::endl;  // 输出: one
}

// 添加第 4 个条目，触发 LRU 淘汰
cache.put(4, "four");

// 键 2 被淘汰了（最久未使用）
assert(!cache.contains(2));
assert(cache.contains(1));
assert(cache.contains(3));
assert(cache.contains(4));
```

### 使用淘汰回调

```cpp
#include <mc/algorithm/lru_cache.h>
#include <mc/log.h>

// 定义淘汰回调
auto on_evict = [](const int& key, std::string&& value) {
    ilog("淘汰缓存: key=${key}, value=${value}", 
         ("key", key)("value", value));
};

// 创建带回调的缓存
mc::algorithm::lru_cache<int, std::string> cache(3, on_evict);

cache.put(1, "one");
cache.put(2, "two");
cache.put(3, "three");

// 这会触发淘汰回调
cache.put(4, "four");  // 日志: 淘汰缓存: key=1, value=one
```

### 动态调整容量

```cpp
mc::algorithm::lru_cache<int, std::string> cache(100);

// 添加数据...
for (int i = 0; i < 50; i++) {
    cache.put(i, std::to_string(i));
}

// 根据内存压力动态调整
if (memory_pressure_high()) {
    cache.set_max_size(25);  // 立即淘汰 25 个最久未使用的条目
}
```

### 复杂类型缓存

```cpp
struct user_info {
    std::string name;
    int age;
    std::vector<std::string> roles;
};

mc::algorithm::lru_cache<std::string, user_info> user_cache(1000);

// 添加用户信息
user_cache.put("user123", {"Alice", 30, {"admin", "user"}});

// 获取并修改
auto user = user_cache.get("user123");
if (user.has_value()) {
    user->get().age++;  // 直接修改缓存中的值
}
```

## 线程安全

**注意**：`lru_cache` 类本身**不是线程安全的**。在多线程环境中使用时，需要外部同步。

### 推荐做法

```cpp
#include <mutex>

mc::algorithm::lru_cache<int, std::string> cache(100);
std::mutex cache_mutex;

// 线程安全的访问
void safe_get(int key) {
    std::lock_guard<std::mutex> lock(cache_mutex);
    auto val = cache.get(key);
    // ... 使用 val
}

void safe_put(int key, std::string value) {
    std::lock_guard<std::mutex> lock(cache_mutex);
    cache.put(key, std::move(value));
}
```

## 性能特征

### 时间复杂度

| 操作 | 时间复杂度 | 说明 |
|------|-----------|------|
| get() | O(1) | 哈希查找 + 链表操作 |
| put() | O(1) | 哈希插入 + 链表操作 |
| erase() | O(1) | 哈希删除 + 链表操作 |
| contains() | O(1) | 哈希查找 |
| clear() | O(n) | 遍历并删除所有条目 |
| for_each() | O(n) | 遍历所有条目 |

### 空间复杂度

- **每个条目**：约 48-64 字节（取决于键值类型）
  - 哈希表条目：~24 字节
  - 链表节点：~24 字节
  - Key 和 Value 大小

- **额外开销**：O(n)，其中 n 为缓存大小

## 最佳实践

### 1. 选择合适的容量

```cpp
// ✅ 根据实际需求设置容量
mc::algorithm::lru_cache<int, std::string> cache(1000);

// ❌ 避免设置过大的容量
mc::algorithm::lru_cache<int, std::string> cache(1000000);  // 可能占用过多内存
```

### 2. 使用 std::move 传递值

```cpp
std::string large_str = generate_large_string();

// ✅ 使用 move 避免拷贝
cache.put(42, std::move(large_str));

// ❌ 会产生不必要的拷贝
// cache.put(42, large_str);
```

### 3. 检查 get 返回值

```cpp
// ✅ 始终检查返回值
auto val = cache.get(42);
if (val.has_value()) {
    // 使用 val->get()
}

// ❌ 不检查直接使用会导致未定义行为
// auto& value = cache.get(42).value().get();  // 可能崩溃
```

### 4. 使用淘汰回调进行清理

```cpp
// ✅ 在淘汰时执行必要的清理
cache.set_eviction_callback([](const int& key, Resource&& res) {
    res.cleanup();  // 释放资源
});
```

## 常见问题

**Q: 为什么 get 返回的是引用而不是值？**

A: 返回引用避免了不必要的拷贝，特别是对于大型对象。同时允许直接修改缓存中的值。

**Q: 淘汰回调什么时候调用？**

A: 当缓存满时添加新条目，或者调用 `set_max_size` 导致容量减小时，会自动调用淘汰回调。

**Q: 可以在淘汰回调中访问缓存吗？**

A: 不推荐。淘汰回调在内部持有锁的情况下调用，在回调中访问缓存可能导致死锁。

**Q: contains() 会更新访问时间吗？**

A: 不会。`contains()` 是只读操作，不会影响 LRU 顺序。

## 相关文档

- [MDB Service API 文档](../mdb/mdb_service.md) - 使用 LRU 缓存的实际案例
- [MDB 缓存配置指南](../../mdb_cache_config_guide.md) - 缓存配置最佳实践
