# Dict API文档

## 概述

`mc::dict` 和 `mc::mutable_dict` 是两个重要的数据结构类，用于表示键值对集合。它们是整个库的基础组件之一，为其他组件提供了灵活的数据组织和表示能力。

- `dict`：不可变的键值对集合，保持插入顺序
- `mutable_dict`：可变的键值对集合，继承自`dict`，提供修改操作

## 主要特性

- 保持键值对的插入顺序
- 高效的键查找操作，基于哈希表实现 
- 值可以是任意`variant`类型
- 支持迭代器、索引和键值访问
- 智能的共享数据模型，减少不必要的内存复制

## 基本用法

### 创建 dict 和 mutable_dict

```cpp
// 创建空字典
mc::dict d1;
mc::mutable_dict md1;

// 使用初始化列表创建
mc::dict d2 = {
    {"name", "张三"},
    {"age", 30},
    {"is_student", false}
};

mc::mutable_dict md2 = {
    {"name", "李四"},
    {"age", 25},
    {"is_student", true}
};

// 使用链式调用创建 mutable_dict
mc::mutable_dict md3 = mc::mutable_dict("name", "王五")
                                      ("age", 35)
                                      ("is_student", false);
```

### 访问数据

```cpp
// 使用 [] 访问
mc::dict d = {{"name", "张三"}, {"age", 30}};
mc::variant name = d["name"];    // "张三"

// 使用 get 安全访问（带默认值）
mc::variant email = d.get("email", "无");  // "无"

// 检查键是否存在
bool has_age = d.contains("age");  // true

// 获取所有键和值
std::vector<std::string> keys = d.keys();    // ["name", "age"]
std::vector<mc::variant> values = d.values(); // ["张三", 30]

// 获取字典大小
size_t size = d.size();  // 2
```

### 修改 mutable_dict

```cpp
// 使用 [] 修改或添加键值对
mc::mutable_dict md = {{"name", "张三"}, {"age", 30}};
md["age"] = 31;         // 修改现有键
md["email"] = "zhangsan@example.com";  // 添加新键

// 使用 operator() 方法
md("phone", "12345678");  // 添加新键值对
md("age", 32);            // 修改现有键

// 链式调用
md("city", "北京")("country", "中国");

// 删除键值对
md.erase("phone");  // 返回 true

// 清空字典
md.clear();
```

### 迭代

```cpp
// 使用迭代器遍历（推荐）
mc::dict d = {{"name", "张三"}, {"age", 30}, {"city", "北京"}};
for (const auto& entry : d) {
    std::cout << entry.key << ": " << entry.value.to_string() << std::endl;
}

// 使用索引遍历（性能较差）
for (size_t i = 0; i < d.size(); ++i) {
    const auto& entry = d.at_index(i);
    std::cout << entry.key << ": " << entry.value.to_string() << std::endl;
}
```

## API 详细说明

### dict 类

#### 构造函数

| 函数 | 描述 |
|------|------|
| `dict()` | 创建空字典 |
| `dict(const dict& other)` | 拷贝构造函数 |
| `dict(dict&& other)` | 移动构造函数 |
| `dict(std::vector<entry> entries)` | 从键值对集合构造 |
| `dict(std::initializer_list<std::pair<std::string, variant>> init)` | 从初始化列表构造 |

#### 访问方法

| 函数 | 描述 |
|------|------|
| `const variant& operator[](const std::string& key) const` | 获取指定键的值（如果键不存在则抛出异常） |
| `const variant& get(const std::string& key, const variant& default_value = variant()) const` | 获取指定键的值，如果键不存在则返回默认值 |
| `bool contains(const std::string& key) const` | 检查字典是否包含指定键 |
| `const entry& at_index(size_t index) const` | 获取指定索引位置的键值对 |
| `size_t size() const` | 获取字典的大小（键值对数量） |
| `bool empty() const` | 检查字典是否为空 |
| `std::vector<std::string> keys() const` | 获取所有键的列表 |
| `std::vector<variant> values() const` | 获取所有值的列表 |

#### 迭代器

| 函数 | 描述 |
|------|------|
| `const_iterator begin() const` | 获取指向首个元素的迭代器 |
| `const_iterator end() const` | 获取指向末尾的迭代器 |
| `const_iterator find(const std::string& key) const` | 查找指定键的元素，返回迭代器 |

#### 比较操作

| 函数 | 描述 |
|------|------|
| `bool operator==(const dict& other) const` | 判断两个字典是否相等 |
| `bool operator!=(const dict& other) const` | 判断两个字典是否不相等 |

### mutable_dict 类（继承自 dict）

#### 构造函数

| 函数 | 描述 |
|------|------|
| `mutable_dict()` | 创建空可变字典 |
| `mutable_dict(const dict& other)` | 从 dict 构造 |
| `mutable_dict(const mutable_dict& other)` | 拷贝构造函数 |
| `mutable_dict(mutable_dict&& other)` | 移动构造函数 |
| `mutable_dict(std::string key, variant value)` | 从单个键值对构造 |
| `mutable_dict(std::vector<entry> entries)` | 从键值对集合构造 |
| `mutable_dict(std::initializer_list<std::pair<std::string, variant>> init)` | 从初始化列表构造 |

#### 修改方法

| 函数 | 描述 |
|------|------|
| `variant& operator[](const std::string& key)` | 获取或设置指定键的值（如果键不存在则创建） |
| `mutable_dict& operator()(const std::string& key, variant value)` | 添加或更新键值对，返回自身引用（支持链式调用） |
| `bool erase(const std::string& key)` | 移除指定键的键值对，如果成功删除返回 true |
| `void clear()` | 清空所有键值对 |

#### 访问和迭代

| 函数 | 描述 |
|------|------|
| `iterator begin()` | 获取指向首个元素的可变迭代器 |
| `iterator end()` | 获取指向末尾的可变迭代器 |
| `iterator find(const std::string& key)` | 查找指定键的元素，返回可变迭代器 |

## 最佳实践

1. **优先使用迭代器**：遍历 dict 和 mutable_dict 时，优先使用迭代器而不是索引访问，可以获得更好的性能

2. **使用初始化列表**：优先使用初始化列表构造 dict 和 mutable_dict，代码更简洁

3. **链式操作**：使用 mutable_dict 的 operator() 方法进行链式调用，可以简化代码

4. **数据共享意识**：注意 mutable_dict 使用共享数据模型，从 dict 创建的 mutable_dict 修改会影响原始 dict

5. **安全访问**：优先使用 get 方法而不是 operator[] 访问数据，可以避免键不存在时的异常

6. **键的类型**：虽然接口支持多种类型的键（std::string、std::string_view、const char*），但内部存储都是 std::string 