# Dict API文档

## 概述

`mc::dict` 是一个重要的数据结构类，用于表示键值对集合。它是整个库的基础组件之一，为其他组件提供了灵活的数据组织和表示能力。

- `dict`：可变的键值对集合，保持插入顺序
- `mutable_dict`：`dict` 的别名，为了向后兼容而保留

> **注意**：`mutable_dict` 现在只是 `dict` 的类型别名（`using mutable_dict = dict;`），两者完全相同。历史上 `dict` 是不可变的而 `mutable_dict` 是可变的，但现在 `dict` 本身就支持所有修改操作。

## 主要特性

- **可变性**：支持添加、修改、删除键值对
- **保持插入顺序**：按照键值对的插入顺序进行迭代
- **高效的键查找**：基于哈希表实现，O(1) 平均查找时间
- **灵活的值类型**：值可以是任意 `variant` 类型
- **多种访问方式**：支持迭代器、索引和键值访问
- **智能的共享数据模型**：使用写时复制（COW），减少不必要的内存复制
- **const 正确性**：非 const `operator[]` 可自动创建键，const 版本会抛出异常

## 基本用法

### 创建 dict

```cpp
// 创建空字典
mc::dict d1;

// 使用初始化列表创建
mc::dict d2 = {
    {"name", "张三"},
    {"age", 30},
    {"is_student", false}
};

// 使用单键值对构造
mc::dict d3("name", "李四");

// 使用链式调用创建
mc::dict d4 = mc::dict("name", "王五")
                     ("age", 35)
                     ("is_student", false);

// mutable_dict 是 dict 的别名，用法完全相同
mc::mutable_dict md = {{"name", "赵六"}, {"age", 28}};  // 等同于 mc::dict
```

### 访问数据

```cpp
mc::dict d = {{"name", "张三"}, {"age", 30}};

// 使用非 const operator[] 访问（不存在的键会自动创建）
d["age"] = 31;         // 修改现有键
d["email"] = "zhangsan@example.com";  // 自动创建新键

// 使用 const operator[] 访问（不存在的键会抛出异常）
const mc::dict& const_d = d;
try {
    mc::variant email = const_d["email"];  // OK
    mc::variant phone = const_d["phone"];  // 抛出 std::out_of_range
} catch (const std::out_of_range& e) {
    // 处理键不存在的情况
}

// 使用 at() 安全访问（总是会对不存在的键抛出异常）
try {
    mc::variant age = d.at("age");      // OK
    mc::variant phone = d.at("phone");  // 抛出 std::out_of_range
} catch (const std::out_of_range& e) {
    // 处理键不存在的情况
}

// 使用 get 安全访问（带默认值，永不抛出异常）
mc::variant email = d.get("email", "无");  // "zhangsan@example.com"
mc::variant phone = d.get("phone", "无");  // "无"

// 检查键是否存在
bool has_age = d.contains("age");    // true
bool has_phone = d.contains("phone"); // false

// 获取所有键和值
std::vector<mc::variant> keys = d.keys();    // ["name", "age", "email"]
std::vector<mc::variant> values = d.values(); // ["张三", 31, "zhangsan@example.com"]

// 获取字典大小
size_t size = d.size();  // 3
```

### 修改 dict

```cpp
mc::dict d = {{"name", "张三"}, {"age", 30}};

// 使用 operator[] 修改或添加键值对（不存在的键会自动创建）
d["age"] = 31;         // 修改现有键
d["email"] = "zhangsan@example.com";  // 添加新键

// 使用 operator() 方法（支持链式调用）
d("phone", "12345678");  // 添加新键值对
d("age", 32);            // 修改现有键

// 链式调用
d("city", "北京")("country", "中国");

// 删除键值对
bool removed = d.erase("phone");  // 返回 true
bool not_found = d.erase("notexist");  // 返回 false

// 清空字典
d.clear();
```

### 迭代

```cpp
mc::dict d = {{"name", "张三"}, {"age", 30}, {"city", "北京"}};

// 使用迭代器遍历（推荐，性能最佳）
for (const auto& entry : d) {
    std::cout << entry.key << ": " << entry.value.to_string() << std::endl;
}

// 使用迭代器修改值
for (auto& entry : d) {
    if (entry.key == "age") {
        entry.value = 31;  // 修改值
    }
}

// 使用索引遍历（不推荐，O(n) 复杂度）
for (size_t i = 0; i < d.size(); ++i) {
    const auto& entry = d.at_index(i);
    std::cout << entry.key << ": " << entry.value.to_string() << std::endl;
}

// 使用 find 查找
auto it = d.find("age");
if (it != d.end()) {
    std::cout << "找到: " << it->key << " = " << it->value.to_string() << std::endl;
}
```

## API 详细说明

### dict 类

> **注意**：`mutable_dict` 是 `dict` 的类型别名，所有 API 完全相同。

#### 构造函数

| 函数 | 描述 |
|------|------|
| `dict()` | 创建空字典 |
| `dict(variant key, variant value)` | 从单个键值对构造（支持链式调用） |
| `dict(const dict& other)` | 拷贝构造函数（共享数据） |
| `dict(dict&& other)` | 移动构造函数 |
| `dict(const std::vector<entry>& entries)` | 从键值对集合构造 |
| `dict(std::initializer_list<std::pair<variant, variant>> init)` | 从初始化列表构造 |
| `dict(InputIt first, InputIt last)` | 从迭代器范围构造 |

#### 访问方法

| 函数 | 描述 |
|------|------|
| `variant& operator[](const std::string& key)` | 获取或设置指定键的值（不存在则自动创建，类似 std::map） |
| `const variant& operator[](const std::string& key) const` | 获取指定键的值（const 版本，键不存在则抛出 std::out_of_range） |
| `variant& at(const std::string& key)` | 获取指定键的值（键不存在则抛出 std::out_of_range） |
| `const variant& at(const std::string& key) const` | 获取指定键的值（const 版本，键不存在则抛出异常） |
| `const variant& get(const std::string& key, const variant& default_value) const` | 获取指定键的值，如果键不存在则返回默认值 |
| `bool contains(const std::string& key) const` | 检查字典是否包含指定键 |
| `entry& at_index(size_t index)` | 获取指定索引位置的键值对 |
| `const entry& at_index(size_t index) const` | 获取指定索引位置的键值对（const 版本） |
| `size_t size() const` | 获取字典的大小（键值对数量） |
| `bool empty() const` | 检查字典是否为空 |
| `std::vector<variant> keys() const` | 获取所有键的列表 |
| `std::vector<variant> values() const` | 获取所有值的列表 |
| `int find_index(const std::string& key) const` | 查找键的索引位置，不存在返回 -1 |

#### 修改方法

| 函数 | 描述 |
|------|------|
| `dict& operator()(variant key, variant value)` | 添加或更新键值对，返回自身引用（支持链式调用） |
| `bool erase(const std::string& key)` | 移除指定键的键值对，如果成功删除返回 true |
| `void clear()` | 清空所有键值对 |
| `std::pair<iterator, bool> insert(variant key, variant value)` | 插入键值对，返回迭代器和是否成功 |
| `iterator insert(const_iterator hint, variant key, variant value)` | 带提示位置的插入 |
| `std::pair<iterator, bool> emplace(variant key, Arg&& arg)` | 原地构造并插入元素 |
| `std::pair<iterator, bool> try_emplace(const K& key, Arg&& arg)` | 尝试原地构造（键不存在时才插入） |

#### 迭代器

| 函数 | 描述 |
|------|------|
| `iterator begin()` | 获取指向首个元素的迭代器 |
| `const_iterator begin() const` | 获取指向首个元素的常量迭代器 |
| `iterator end()` | 获取指向末尾的迭代器 |
| `const_iterator end() const` | 获取指向末尾的常量迭代器 |
| `iterator find(const std::string& key)` | 查找指定键的元素，返回迭代器 |
| `const_iterator find(const std::string& key) const` | 查找指定键的元素，返回常量迭代器 |

#### 比较和其他操作

| 函数 | 描述 |
|------|------|
| `bool operator==(const dict& other) const` | 判断两个字典是否相等 |
| `bool operator!=(const dict& other) const` | 判断两个字典是否不相等 |
| `dict operator+(const dict& other) const` | 合并两个字典 |
| `dict& operator+=(const dict& other)` | 合并另一个字典到当前字典 |
| `size_t hash() const` | 计算字典的哈希值 |
| `std::string to_string() const` | 转换为字符串表示 |
| `dict deep_copy() const` | 深度拷贝字典 |

#### 兼容性方法

| 函数 | 描述 |
|------|------|
| `dict& as_mut()` | 获取可变引用（为向后兼容保留，直接返回自身） |
| `dict as_mut() const` | 获取可变副本（为向后兼容保留） |

## 最佳实践

1. **优先使用迭代器**：遍历 dict 时，优先使用迭代器（range-based for）而不是索引访问，可以获得 O(n) 而非 O(n²) 的性能

2. **使用初始化列表**：优先使用初始化列表构造 dict，代码更简洁易读
   ```cpp
   mc::dict d = {{"name", "张三"}, {"age", 30}};  // 推荐
   ```

3. **链式操作**：使用 `operator()` 方法进行链式调用，可以简化代码
   ```cpp
   mc::dict d = mc::dict("a", 1)("b", 2)("c", 3);
   ```

4. **选择合适的访问方法**：
   - 修改或可能创建键：使用非 const `operator[]`
   - 只读访问已知存在的键：使用 const `operator[]` 或 `at()`
   - 访问可能不存在的键：使用 `get()` 方法提供默认值
   - 检查后访问：先用 `contains()` 检查，再访问

5. **理解 const 正确性**：
   ```cpp
   dict d = {{"key", "value"}};
   d["new_key"] = 123;           // OK，自动创建
   
   const dict& cd = d;
   auto v1 = cd["key"];          // OK，键存在
   auto v2 = cd["missing"];      // 抛出异常！
   ```

6. **数据共享意识**：dict 使用共享数据模型（写时复制），拷贝很轻量，但修改会影响所有共享该数据的对象

7. **类型灵活性**：键和值都支持多种类型，接口支持 `std::string`、`std::string_view`、`const char*` 和 `variant` 作为键

8. **避免使用 `mutable_dict`**：新代码应直接使用 `dict`，`mutable_dict` 仅为向后兼容保留

9. **性能考虑**：
   - 键查找：O(1) 平均时间复杂度
   - 索引访问 `at_index()`：O(n) 时间复杂度，避免在循环中使用
   - 使用迭代器遍历：O(n) 时间复杂度，是遍历的最佳方式 