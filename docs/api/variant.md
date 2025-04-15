# mc::variant 类

## 定义

```cpp
namespace mc {
    class variant {
        // ...
    };
}
```

## 概述

`mc::variant` 类是一个通用的变量类型系统，可以存储多种类型的数据，包括基本数据类型（整数、浮点数、布尔值）、字符串、对象（dict）、数组（variants）和二进制数据（blob）。它是LibMCPP库的核心组件之一，为其他组件提供了灵活的数据表示和处理能力。

## 模块架构

`variant` 模块是LibMCPP库的基础数据表示层，它的核心架构如下：

```
        variant
           |
           |----> variant_base<>
                     |
                     |----> 基本数据类型 (int, double, bool等)
                     |
                     |----> 字符串 (std::string)
                     |
                     |----> 对象 (dict)
                     |
                     |----> 数组 (variants)
                     |
                     |----> 二进制 (blob)
```

实际上，`mc::variant` 是 `mc::variant_base<>` 的类型别名，后者是一个模板类，支持自定义内存分配器和类型检查策略。

## 详细描述

`variant` 类设计基于以下核心原则：

### 1. 类型安全

每个 `variant` 实例都存储其当前值的类型信息，并在运行时执行类型检查。对于类型转换，提供了几种机制：
- 直接转换：通过 `as_int()`, `as_double()` 等方法
- 模板转换：通过 `as<T>()` 方法
- 带默认值的转换：通过 `as<T>(default_value)` 方法
- 可选转换：通过 `try_as<T>()` 方法，返回 `std::optional<T>`

### 2. 内存管理

`variant` 对不同类型的值采用不同的内存管理策略：
- 对于小型值类型（整数、浮点数、布尔值），直接存储在 `variant` 实例中
- 对于大型值类型（字符串、dict、数组、blob），使用智能指针间接存储

所有资源都通过RAII（资源获取即初始化）机制管理，确保在 `variant` 析构时正确释放资源。

### 3. 多态访问

`variant` 提供了两种访问内部数据的方式：
- 虚函数方式：通过 `visit(visitor&)` 方法和自定义 `visitor` 类
- 模板方式：通过 `visit_with(lambda)` 方法和自定义函数对象

这使得可以以类型安全的方式处理不同类型的数据，无需大量的类型检查和转换。

### 4. 通用容器支持

`variant` 可以与标准库容器（如 `std::vector`、`std::map` 等）无缝集成，通过适当的转换函数支持将这些容器转换为 `variant` 类型，反之亦然。

## 线程安全性

`variant` 类本身不是线程安全的。在多线程环境中使用时，需要采取适当的同步措施，确保不会有多个线程同时修改同一个 `variant` 实例。

## 典型使用场景

`variant` 在以下场景中特别有用：

1. **配置管理**：表示各种类型的配置值
2. **数据交换**：作为不同组件间交换数据的通用格式
3. **动态类型环境**：在编译时无法确定确切类型的场景
4. **脚本引擎集成**：连接静态类型的C++代码和动态类型的脚本语言
5. **JSON/XML处理**：表示和处理层次化的数据

## 公共构造函数

### 构造函数列表

- `variant()`
- `variant(nullptr_t)`
- `variant(bool val)`
- `variant(int8_t val)`
- `variant(uint8_t val)`
- `variant(int16_t val)`
- `variant(uint16_t val)`
- `variant(int32_t val)`
- `variant(uint32_t val)`
- `variant(int64_t val)`
- `variant(uint64_t val)`
- `variant(float val)`
- `variant(double val)`
- `variant(const char* str)`
- `variant(const std::string& str)`
- `variant(std::string_view str)`
- `variant(const dict& obj)`
- `variant(mutable_dict& obj)`
- `variant(const array_type& arr)`
- `variant(const blob& b)`
- `variant(const std::optional<T>& v)`
- `variant(const variant& other)`
- `variant(variant&& other)`

*详细说明请见上文*

## 公共成员函数

### 类型检查函数

- `type_id get_type() const`
- `bool is_null() const`
- `bool is_bool() const`
- `bool is_int8() const`
- `bool is_uint8() const`
- `bool is_int16() const`
- `bool is_uint16() const`
- `bool is_int32() const`
- `bool is_uint32() const`
- `bool is_int64() const`
- `bool is_uint64() const`
- `bool is_double() const`
- `bool is_string() const`
- `bool is_object() const`
- `bool is_dict() const`
- `bool is_array() const`
- `bool is_blob() const`
- `bool is_numeric() const`
- `bool is_integer() const`
- `bool is_signed_integer() const`
- `bool is_unsigned_integer() const`

### 类型转换函数

- `int8_t as_int8() const`
- `uint8_t as_uint8() const`
- `int16_t as_int16() const`
- `uint16_t as_uint16() const`
- `int32_t as_int32() const`
- `uint32_t as_uint32() const`
- `int64_t as_int64() const`
- `uint64_t as_uint64() const`
- `double as_double() const`
- `bool as_bool() const`
- `std::string as_string() const`
- `const std::string& get_string() const`
- `dict as_dict() const`
- `const dict& get_object() const`
- `variants as_array() const`
- `const array_type& get_array() const`
- `blob as_blob() const`
- `const blob_type& get_blob() const`
- `template <typename T> T as() const`
- `template <typename T> std::optional<T> try_as() const`
- `template <typename T> T as(const T& default_value) const`

### 索引和键访问

- `const variant& operator[](std::size_t pos) const`
- `const variant& operator[](std::string_view key) const`
- `const variant& get(std::string_view key, const variant& default_value) const`
- `bool contains(std::string_view key) const`
- `size_t size() const`

### 访问者模式

#### class visitor

`variant::visitor` 是一个用于访问 variant 中的数据的接口类。通过实现这个接口，可以以多态方式处理不同类型的 variant 数据。

```cpp
class visitor {
public:
    virtual ~visitor() {}
    
    virtual void handle() const = 0;                         // 处理 null
    virtual void handle(const int64_t& v) const = 0;         // 处理有符号整数
    virtual void handle(const uint64_t& v) const = 0;        // 处理无符号整数
    virtual void handle(const double& v) const = 0;          // 处理浮点数
    virtual void handle(const bool& v) const = 0;            // 处理布尔值
    virtual void handle(const std::string& v) const = 0;     // 处理字符串
    virtual void handle(const object_type& v) const = 0;     // 处理对象(dict)
    virtual void handle(const array_type& v) const = 0;      // 处理数组
    virtual void handle(const blob_type& v) const = 0;       // 处理二进制数据
};
```

**使用示例:**

```cpp
class MyVisitor : public variant::visitor {
public:
    mutable std::string result;

    void handle() const override {
        result = "null";
    }

    void handle(const int64_t& value) const override {
        result = "整数: " + std::to_string(value);
    }

    void handle(const uint64_t& value) const override {
        result = "无符号整数: " + std::to_string(value);
    }

    void handle(const double& value) const override {
        result = "浮点数: " + std::to_string(value);
    }

    void handle(const bool& value) const override {
        result = value ? "真" : "假";
    }

    void handle(const std::string& value) const override {
        result = "字符串: " + value;
    }

    void handle(const dict& value) const override {
        result = "字典: " + std::to_string(value.size()) + "项";
    }

    void handle(const variants& value) const override {
        result = "数组: " + std::to_string(value.size()) + "项";
    }

    void handle(const blob& value) const override {
        result = "二进制: " + std::to_string(value.data.size()) + "字节";
    }
};

variant v(42);
MyVisitor visitor;
v.visit(visitor);
std::cout << visitor.result << std::endl;  // 输出: "整数: 42"
```

#### void visit(const visitor& v) const

使用提供的访问者对象访问variant中的数据。根据variant的当前类型，会调用访问者对象中对应的handle方法。

```cpp
variant v(42);
MyVisitor visitor;
v.visit(visitor);
```

**复杂度:** O(1)

#### template <typename Visitor> auto visit_with(Visitor&& visitor) const

使用提供的函数对象访问variant中的数据。这是一个更现代、更灵活的访问方式，支持使用lambda表达式和模板方式处理不同类型。

```cpp
variant v(42);
auto result = v.visit_with([](auto&& value) -> std::string {
    using T = std::decay_t<decltype(value)>;
    if constexpr (std::is_same_v<T, int64_t>) {
        return "整数: " + std::to_string(value);
    } else if constexpr (std::is_same_v<T, std::string>) {
        return "字符串: " + value;
    } else {
        return "其他类型";
    }
});
std::cout << result << std::endl;  // 输出: "整数: 42"
```

**复杂度:** O(1)

### 修改操作

#### void clear()

清除variant的当前值，将其重置为null。

```cpp
variant v(42);
v.clear();  // v现在是null
```

#### void swap(variant& other) noexcept

交换两个variant的内容。

```cpp
variant v1(42);
variant v2("hello");
v1.swap(v2);  // v1现在包含"hello"，v2现在包含42
```

**复杂度:** O(1)

### 杂项函数

#### explicit operator bool() const

检查variant是否非空且对于布尔上下文有"真"值。

- 如果variant是null，返回false
- 如果variant是布尔类型，返回其值
- 如果variant是数值类型，0为false，非0为true
- 如果variant是字符串类型，空字符串为false，非空为true
- 如果variant是容器类型（dict或array），空容器为false，非空为true

```cpp
variant v1;           // null
if (v1) {             // 条件为false
    // 不会执行
}

variant v2(42);       // 整数42
if (v2) {             // 条件为true
    // 会执行
}

variant v3("");       // 空字符串
if (v3) {             // 条件为false
    // 不会执行
}
```

### 空字符串布尔逻辑运算与C语言的区别

在mc::variant中，空字符串在布尔逻辑运算中被视为假(false)，这与C语言的行为有明显区别：

1. **mc::variant的处理方式**：
   - 空字符串(`variant v("")`)在条件判断中被视为`false`
   - 非空字符串在条件判断中被视为`true`
   - 这是通过`variant`类的`as_bool()`方法或`operator bool()`实现的

2. **C语言的处理方式**：
   - 在C语言中，字符串是字符数组，其布尔值取决于指针是否为NULL
   - C语言中即使是空字符串`""`，只要其指针不为NULL，在条件判断中也会被视为`true`
   - C语言判断的是指针的存在性，而非字符串的内容

3. **设计理念差异**：
   - mc::variant检查字符串的长度，空字符串返回`false`，符合语义直觉
   - C语言检查指针是否为NULL，不关心字符串内容
   - variant对所有容器类型(字符串、数组、字典)采用一致的处理方式：
     - 空容器 → `false`
     - 非空容器 → `true`

这种设计更接近Python等现代语言的处理方式，使代码更易读、更符合直觉，避免了C语言中常见的空字符串判断错误。

### C++中各类型的布尔逻辑与variant对比

mc::variant的布尔逻辑设计遵循了一致性和直观性原则，下面是它与C++标准库中各种类型布尔逻辑的详细对比：

#### 1. 数值类型

| 类型 | C++ 原生行为 | variant 行为 | 示例 |
|------|------------|-------------|------|
| 整数 | 0为false，非0为true | 相同 | `variant v(0); // if(v)为false` |
| 浮点数 | 0.0为false，非0为true | 相同 | `variant v(0.0); // if(v)为false` |
| bool | false为false，true为true | 相同 | `variant v(false); // if(v)为false` |

```cpp
// C++原生整数
int i1 = 0, i2 = 42;
if (i1) { /* 不执行 */ }
if (i2) { /* 执行 */ }

// variant整数
variant v_i1(0), v_i2(42);
if (v_i1) { /* 不执行 */ }
if (v_i2) { /* 执行 */ }
```

#### 2. 字符串类型

| 类型 | C++ 原生行为 | variant 行为 | 示例 |
|------|------------|-------------|------|
| C字符串 | 非NULL指针为true，NULL为false | 空字符串为false，非空为true | `variant v(""); // if(v)为false` |
| std::string | 非空对象始终为true | 空字符串为false，非空为true | `variant v(std::string("")); // if(v)为false` |
| std::string_view | 非空对象始终为true | 空字符串为false，非空为true | `variant v(std::string_view("")); // if(v)为false` |

```cpp
// C++原生字符串
const char* cs1 = "";
const char* cs2 = "hello";
std::string s1 = "";
std::string s2 = "hello";

if (cs1) { /* 执行，因为指针非NULL */ }
if (cs2) { /* 执行，因为指针非NULL */ }
if (s1) { /* 在C++中，这是编译错误，std::string没有布尔转换 */ }
// 以下是正确的判断方式
if (!s1.empty()) { /* 不执行 */ }
if (!s2.empty()) { /* 执行 */ }

// variant字符串
variant v_cs1("");
variant v_cs2("hello");
variant v_s1(std::string(""));
variant v_s2(std::string("hello"));

if (v_cs1) { /* 不执行，空字符串为false */ }
if (v_cs2) { /* 执行，非空字符串为true */ }
if (v_s1) { /* 不执行，空字符串为false */ }
if (v_s2) { /* 执行，非空字符串为true */ }
```

#### 3. 容器类型

| 类型 | C++ 原生行为 | variant 行为 | 示例 |
|------|------------|-------------|------|
| std::vector等 | 没有直接布尔转换 | 空容器为false，非空为true | `variant v(variants{}); // if(v)为false` |
| std::map等 | 没有直接布尔转换 | 空容器为false，非空为true | `variant v(dict{}); // if(v)为false` |

```cpp
// C++原生容器
std::vector<int> vec1 = {};
std::vector<int> vec2 = {1, 2, 3};
std::map<std::string, int> map1 = {};
std::map<std::string, int> map2 = {{"key", 1}};

// 在C++中，容器没有布尔转换运算符
// if (vec1) { /* 编译错误 */ }
// 以下是正确的判断方式
if (!vec1.empty()) { /* 不执行 */ }
if (!vec2.empty()) { /* 执行 */ }
if (!map1.empty()) { /* 不执行 */ }
if (!map2.empty()) { /* 执行 */ }

// variant容器
variant v_vec1(variants{});
variant v_vec2(variants{1, 2, 3});
variant v_map1(dict{});
variant v_map2(dict{{"key", 1}});

if (v_vec1) { /* 不执行，空容器为false */ }
if (v_vec2) { /* 执行，非空容器为true */ }
if (v_map1) { /* 不执行，空容器为false */ }
if (v_map2) { /* 执行，非空容器为true */ }
```

#### 4. 空值和指针类型

| 类型 | C++ 原生行为 | variant 行为 | 示例 |
|------|------------|-------------|------|
| nullptr | false | false | `variant v; // if(v)为false` |
| 指针 | NULL为false，非NULL为true | 不直接支持原始指针 | |
| std::unique_ptr | NULL为false，非NULL为true | 通过to_variant转换后，NULL为false，非NULL为true | |
| std::optional | 无值为false，有值为true | 无值为false，有值时取决于值类型 | `variant v(std::optional<int>{}); // if(v)为false` |

```cpp
// C++原生空值和指针
int* ptr1 = nullptr;
int* ptr2 = new int(42);
std::unique_ptr<int> uptr1;
std::unique_ptr<int> uptr2 = std::make_unique<int>(42);
std::optional<int> opt1;
std::optional<int> opt2 = 42;

if (ptr1) { /* 不执行 */ }
if (ptr2) { /* 执行 */ }
if (uptr1) { /* 不执行 */ }
if (uptr2) { /* 执行 */ }
if (opt1) { /* 不执行 */ }
if (opt2) { /* 执行 */ }

// 记得释放手动分配的内存
delete ptr2;

// variant空值
variant v_null;
variant v_opt1(std::optional<int>{});
variant v_opt2(std::optional<int>(42));

if (v_null) { /* 不执行，null为false */ }
if (v_opt1) { /* 不执行，无值optional为false */ }
if (v_opt2) { /* 执行，值为42的optional为true */ }
```

#### 5. 布尔运算组合

variant支持在布尔表达式中与其他类型组合使用：

```cpp
variant v_true(true);
variant v_false(false);
variant v_zero(0);
variant v_str("hello");
variant v_empty_str("");

// 逻辑AND
bool res1 = v_true && v_str;      // true，两个操作数都为true
bool res2 = v_false && v_str;     // false，v_false为false
bool res3 = v_str && v_empty_str; // false，v_empty_str为false

// 逻辑OR
bool res4 = v_false || v_str;     // true，v_str为true
bool res5 = v_zero || v_empty_str; // false，两个操作数都为false
bool res6 = v_str || v_false;     // true，v_str为true

// 逻辑NOT
bool res7 = !v_str;               // false，v_str为true
bool res8 = !v_empty_str;         // true，v_empty_str为false
bool res9 = !v_zero;              // true，v_zero为false
```

#### 设计优势

mc::variant的布尔逻辑设计具有以下优势：

1. **一致性**：对所有容器类型（字符串、数组、字典等）采用一致的空/非空判断
2. **直观性**：符合人类直觉，空值或空容器被视为"无"或"假"
3. **安全性**：避免了C/C++中因指针存在但内容为空而导致的逻辑错误
4. **互操作性**：与Python、JavaScript等动态语言的布尔逻辑行为一致，便于跨语言开发

相比原始C++类型各自不同的布尔行为，variant提供了统一一致的布尔语义，使代码更加简洁明了。

#### size_t hash() const

计算variant值的哈希值，可用于哈希表等数据结构。

```cpp
variant v("hello");
size_t h = v.hash();  // 字符串"hello"的哈希值
```

**复杂度:** 对于小类型为O(1)，对于字符串和容器与其大小成正比

## 全局函数

### template <typename T> void to_variant(const T& src, variant& vo)

将任意类型T的值转换为variant。这是variant的扩展点，允许为自定义类型定义到variant的转换。

```cpp
struct Point { int x, y; };

// 为Point类型定义到variant的转换
namespace mc {
    void to_variant(const Point& p, variant& vo) {
        mutable_dict dict;
        dict("x", p.x)("y", p.y);
        vo = dict;
    }
}

// 使用
Point p{10, 20};
variant v;
to_variant(p, v);  // v现在包含{"x": 10, "y": 20}
```

### template <typename T> void from_variant(const variant& var, T& vo)

将variant转换为任意类型T的值。这是variant的另一个扩展点，允许为自定义类型定义从variant的转换。

```cpp
struct Point { int x, y; };

// 为Point类型定义从variant的转换
namespace mc {
    void from_variant(const variant& var, Point& vo) {
        vo.x = var["x"].as<int>();
        vo.y = var["y"].as<int>();
    }
}

// 使用
variant v(dict{{"x", 10}, {"y", 20}});
Point p;
from_variant(v, p);  // p现在是{10, 20}
```

## 示例

### 基本使用

```cpp
// 创建不同类型的variant
variant v1;                       // null
variant v2 = 42;                  // 整数
variant v3 = 3.14;                // 浮点数
variant v4 = "hello";             // 字符串
variant v5 = true;                // 布尔值

// 类型检查
bool is_null = v1.is_null();      // true
bool is_int = v2.is_int32();      // true
bool is_double = v3.is_double();  // true
bool is_string = v4.is_string();  // true
bool is_bool = v5.is_bool();      // true

// 类型转换
int i = v2.as<int>();             // 42
double d = v3.as<double>();       // 3.14
std::string s = v4.as<std::string>(); // "hello"
bool b = v5.as<bool>();           // true

// 试图转换不兼容的类型
try {
    int i = v4.as<int>();         // 会抛出异常
} catch (const mc::bad_cast_exception& e) {
    // 处理转换错误
}

// 使用try_as避免异常
auto opt = v4.try_as<int>();      // 返回空的std::optional<int>
if (!opt) {
    // 处理转换失败的情况
}

// 使用带默认值的转换
int i2 = v4.as<int>(0);           // 0（转换失败返回默认值）
```

### 容器操作

```cpp
// 创建字典
variant v_dict = dict{
    {"name", "张三"},
    {"age", 30},
    {"skills", variants{"C++", "Python", "JavaScript"}}
};

// 访问字典的值
std::string name = v_dict["name"].as<std::string>();  // "张三"
int age = v_dict["age"].as<int>();                    // 30

// 检查键是否存在
bool has_email = v_dict.contains("email");            // false

// 使用get方法带默认值
variant email = v_dict.get("email", "无");            // "无"

// 访问嵌套数组
const variants& skills = v_dict["skills"].get_array();
std::string first_skill = skills[0].as<std::string>(); // "C++"

// 创建数组
variant v_array = variants{1, "two", 3.0, true};

// 访问数组元素
int first = v_array[0].as<int>();                     // 1
std::string second = v_array[1].as<std::string>();    // "two"
double third = v_array[2].as<double>();               // 3.0
bool fourth = v_array[3].as<bool>();                  // true

// 获取数组大小
size_t size = v_array.size();                         // 4
```

### 访问者模式

```cpp
// 使用visitor类
class TypePrinter : public variant::visitor {
public:
    mutable std::string result;

    void handle() const override {
        result = "类型: null";
    }

    void handle(const int64_t& value) const override {
        result = "类型: 整数, 值: " + std::to_string(value);
    }

    void handle(const uint64_t& value) const override {
        result = "类型: 无符号整数, 值: " + std::to_string(value);
    }

    void handle(const double& value) const override {
        result = "类型: 浮点数, 值: " + std::to_string(value);
    }

    void handle(const bool& value) const override {
        result = "类型: 布尔值, 值: " + std::string(value ? "true" : "false");
    }

    void handle(const std::string& value) const override {
        result = "类型: 字符串, 值: " + value;
    }

    void handle(const dict& value) const override {
        result = "类型: 字典, 键值对数量: " + std::to_string(value.size());
    }

    void handle(const variants& value) const override {
        result = "类型: 数组, 元素数量: " + std::to_string(value.size());
    }

    void handle(const blob& value) const override {
        result = "类型: 二进制, 大小: " + std::to_string(value.data.size()) + "字节";
    }
};

variant v(42);
TypePrinter printer;
v.visit(printer);
std::cout << printer.result << std::endl;  // 输出: "类型: 整数, 值: 42"

// 使用visit_with和lambda
variant v2("hello");
auto result = v2.visit_with([](auto&& value) -> std::string {
    using T = std::decay_t<decltype(value)>;
    std::stringstream ss;
    ss << "类型: ";
    
    if constexpr (std::is_same_v<T, std::nullptr_t>) {
        ss << "null";
    } else if constexpr (std::is_same_v<T, int64_t>) {
        ss << "整数, 值: " << value;
    } else if constexpr (std::is_same_v<T, uint64_t>) {
        ss << "无符号整数, 值: " << value;
    } else if constexpr (std::is_same_v<T, double>) {
        ss << "浮点数, 值: " << value;
    } else if constexpr (std::is_same_v<T, bool>) {
        ss << "布尔值, 值: " << (value ? "true" : "false");
    } else if constexpr (std::is_same_v<T, std::string>) {
        ss << "字符串, 值: " << value;
    } else if constexpr (std::is_same_v<T, dict>) {
        ss << "字典, 键值对数量: " << value.size();
    } else if constexpr (std::is_same_v<T, variants>) {
        ss << "数组, 元素数量: " << value.size();
    } else if constexpr (std::is_same_v<T, blob>) {
        ss << "二进制, 大小: " << value.data.size() << "字节";
    }
    
    return ss.str();
});
std::cout << result << std::endl;  // 输出: "类型: 字符串, 值: hello"
```

### 标准容器集成

variant可以与多种标准C++容器无缝集成，以下是完整的示例：

#### 与std::vector集成

```cpp
// 将std::vector转换为variant（数组类型）
std::vector<int> vec = {1, 2, 3, 4, 5};
variant v_vec;
to_variant(vec, v_vec);

// 验证类型和内容
assert(v_vec.is_array());
assert(v_vec.size() == 5);
assert(v_vec[0].as<int>() == 1);
assert(v_vec[4].as<int>() == 5);

// 将variant转换回std::vector
std::vector<int> vec_back = v_vec.as<std::vector<int>>();
assert(vec_back.size() == 5);
assert(vec_back[0] == 1);
assert(vec_back[4] == 5);

// 处理不同类型的vector元素
std::vector<variant> mixed_vec = {1, "two", 3.0, true};
variant v_mixed(mixed_vec);

// 遍历混合类型的数组
for (size_t i = 0; i < v_mixed.size(); ++i) {
    v_mixed[i].visit_with([](auto&& value) {
        using T = std::decay_t<decltype(value)>;
        if constexpr (std::is_same_v<T, int64_t>) {
            std::cout << "整数: " << value << std::endl;
        } else if constexpr (std::is_same_v<T, std::string>) {
            std::cout << "字符串: " << value << std::endl;
        } else if constexpr (std::is_same_v<T, double>) {
            std::cout << "浮点数: " << value << std::endl;
        } else if constexpr (std::is_same_v<T, bool>) {
            std::cout << "布尔值: " << (value ? "true" : "false") << std::endl;
        }
    });
}
```

#### 与std::map和std::unordered_map集成

```cpp
// 将std::map转换为variant（dict类型）
std::map<std::string, int> map_data = {
    {"one", 1},
    {"two", 2},
    {"three", 3}
};
variant v_map;
to_variant(map_data, v_map);

// 验证类型和内容
assert(v_map.is_dict());
assert(v_map.size() == 3);
assert(v_map["one"].as<int>() == 1);
assert(v_map["three"].as<int>() == 3);

// 将variant转换回std::map
std::map<std::string, int> map_back = v_map.as<std::map<std::string, int>>();
assert(map_back.size() == 3);
assert(map_back["one"] == 1);
assert(map_back["three"] == 3);

// 也支持unordered_map
std::unordered_map<std::string, variant> umap = {
    {"name", "张三"},
    {"age", 30},
    {"is_student", false}
};
variant v_umap;
to_variant(umap, v_umap);

// 转换回unordered_map
std::unordered_map<std::string, variant> umap_back = v_umap.as<std::unordered_map<std::string, variant>>();
assert(umap_back.size() == 3);
assert(umap_back["name"].as<std::string>() == "张三");
```

#### 与std::tuple集成

```cpp
// 将std::tuple转换为variant（数组类型）
std::tuple<int, std::string, bool> tuple_data = {42, "hello", true};
variant v_tuple;
to_variant(tuple_data, v_tuple);

// 验证类型和内容
assert(v_tuple.is_array());
assert(v_tuple.size() == 3);
assert(v_tuple[0].as<int>() == 42);
assert(v_tuple[1].as<std::string>() == "hello");
assert(v_tuple[2].as<bool>() == true);

// 将variant转换回std::tuple
std::tuple<int, std::string, bool> tuple_back = v_tuple.as<std::tuple<int, std::string, bool>>();
assert(std::get<0>(tuple_back) == 42);
assert(std::get<1>(tuple_back) == "hello");
assert(std::get<2>(tuple_back) == true);
```

#### 与std::optional集成

```cpp
// 将std::optional转换为variant
std::optional<int> opt_value = 42;
variant v_opt(opt_value);

// 验证内容
assert(v_opt.is_int32());
assert(v_opt.as<int>() == 42);

// 处理空optional
std::optional<int> empty_opt;
variant v_empty_opt(empty_opt);
assert(v_empty_opt.is_null());

// 将variant转换回std::optional
std::optional<int> opt_back = v_opt.try_as<int>();
assert(opt_back.has_value());
assert(*opt_back == 42);

// 处理类型不匹配的情况
variant v_str("hello");
std::optional<int> opt_str = v_str.try_as<int>();
assert(!opt_str.has_value());
```

#### 嵌套容器

```cpp
// 构建复杂的嵌套数据结构
std::map<std::string, std::vector<std::tuple<int, std::string>>> complex_data = {
    {"group1", {
        {1, "item1"},
        {2, "item2"}
    }},
    {"group2", {
        {3, "item3"},
        {4, "item4"}
    }}
};

// 转换为variant
variant v_complex;
to_variant(complex_data, v_complex);

// 访问嵌套结构
// 获取group1的第一个元素的第二部分
std::string item = v_complex["group1"][0][1].as<std::string>();
assert(item == "item1");

// 转换回原始类型
auto complex_back = v_complex.as<std::map<std::string, std::vector<std::tuple<int, std::string>>>>();
assert(complex_back["group2"][1] == std::tuple<int, std::string>(4, "item4"));
```

#### 自定义类型与variant

> **注意:** 以下示例仅用于展示自定义类型与variant转换的基本原理。在实际项目中，推荐使用`mc::reflect`反射系统来自动处理自定义类型与variant之间的转换，这样可以减少样板代码并提高维护性。

```cpp
// 自定义数据类型
struct Person {
    std::string name;
    int age;
    std::vector<std::string> skills;
    
    // 相等比较运算符，用于测试
    bool operator==(const Person& other) const {
        return name == other.name && age == other.age && skills == other.skills;
    }
};

// 为Person类型定义转换函数
namespace mc {
    void to_variant(const Person& p, variant& vo) {
        mutable_dict dict;
        dict("name", p.name)
            ("age", p.age);
        
        // 将skills数组转换为variants
        variants skills_array;
        for (const auto& skill : p.skills) {
            skills_array.push_back(skill);
        }
        dict("skills", skills_array);
        
        vo = dict;
    }

    void from_variant(const variant& var, Person& vo) {
        vo.name = var["name"].as<std::string>();
        vo.age = var["age"].as<int>();
        
        // 将variants数组转换回skills
        const variants& skills_array = var["skills"].get_array();
        vo.skills.clear();
        for (const auto& skill : skills_array) {
            vo.skills.push_back(skill.as<std::string>());
        }
    }
}

// 使用自定义类型
Person p{"张三", 30, {"C++", "Python", "JavaScript"}};
variant v_person;
to_variant(p, v_person);

// 验证转换结果
assert(v_person.is_dict());
assert(v_person["name"].as<std::string>() == "张三");
assert(v_person["age"].as<int>() == 30);
assert(v_person["skills"].is_array());
assert(v_person["skills"][0].as<std::string>() == "C++");

// 转换回Person对象
Person p_back;
from_variant(v_person, p_back);
assert(p == p_back);

// 使用容器中的自定义类型
std::vector<Person> people = {
    {"张三", 30, {"C++", "Python"}},
    {"李四", 25, {"Java", "JavaScript"}}
};
variant v_people;
to_variant(people, v_people);

// 验证容器转换结果
assert(v_people.is_array());
assert(v_people.size() == 2);
assert(v_people[0]["name"].as<std::string>() == "张三");
assert(v_people[1]["age"].as<int>() == 25);

// 转换回原始容器
std::vector<Person> people_back = v_people.as<std::vector<Person>>();
assert(people == people_back);
```

## 相关类型

- `mc::dict` - 不可变的键值对集合
- `mc::mutable_dict` - 可变的键值对集合
- `mc::variants` - variant数组
- `mc::blob` - 二进制数据容器

## 比较操作符

variant 类提供了一套完整的比较操作符，使其能够与各种类型进行比较。这些比较操作符提供了高效的比较方式，避免了先转换再比较所带来的不必要性能开销。

### variant 与 variant 之间的比较

```cpp
variant v1(42);
variant v2(42);
variant v3(42.0);   // double类型
variant v4("test");
variant v5(100);

// 相等比较
bool equal1 = (v1 == v2);        // true，相同类型相同值
bool equal2 = (v1 == v3);        // true，不同类型但值相等
bool not_equal = (v1 != v4);     // true，不同类型不同值

// 大小比较
bool less = (v1 < v5);           // true，42 < 100
bool greater = (v5 > v1);        // true，100 > 42
bool less_equal = (v1 <= v2);    // true，42 <= 42
bool greater_equal = (v5 >= v1); // true，100 >= 42

// 不兼容类型比较会抛出异常
try {
    bool result = (v1 < v4);     // 抛出 mc::invalid_op_exception
} catch (const mc::invalid_op_exception& e) {
    // 处理异常
}
```

### variant 与基础类型的比较

variant 支持与各种基础类型的直接比较，无需先进行类型转换。

#### 整数类型比较

```cpp
variant v(42);

// 与不同整数类型比较
bool equal1 = (v == 42);         // true
bool equal2 = (v == 42L);        // true，与int64_t比较
bool equal3 = (42 == v);         // true，反向比较也支持

// 大小比较
bool less = (v < 100);           // true，42 < 100
bool greater = (v > 10);         // true，42 > 10
bool less_equal = (v <= 42);     // true，42 <= 42
bool greater_equal = (100 >= v); // true，100 >= 42
```

#### 浮点数类型比较

```cpp
variant v(3.14);

// 与浮点数比较
bool equal1 = (v == 3.14);       // true
bool equal2 = (v == 3.14f);      // true，与float比较
bool not_equal = (v != 2.718);   // true，3.14 != 2.718

// 大小比较
bool less = (v < 4.0);           // true，3.14 < 4.0
bool greater = (v > 3.0);        // true，3.14 > 3.0
bool less_equal = (v <= 3.14);   // true，3.14 <= 3.14
bool greater_equal = (3.14 >= v); // true，3.14 >= 3.14
```

variant在比较浮点数时使用epsilon值来解决浮点数精度问题，默认的epsilon值为`VARIANT_FLOAT_EPSILON`(1e-6)。这意味着当两个浮点数之差的绝对值小于1e-6时，它们被认为是相等的。

```cpp
variant v(3.14);

// 与浮点数比较
bool equal1 = (v == 3.14);       // true
bool equal2 = (v == 3.14f);      // true，与float比较
bool equal3 = (v == 3.14000001); // true，差值小于epsilon视为相等
bool not_equal = (v != 2.718);   // true，3.14 != 2.718

// 大小比较
bool less = (v < 4.0);           // true，3.14 < 4.0
bool greater = (v > 3.0);        // true，3.14 > 3.0
bool less_equal = (v <= 3.14);   // true，3.14 <= 3.14
bool greater_equal = (3.14 >= v); // true，3.14 >= 3.14
```

> **注意:** `VARIANT_FLOAT_EPSILON`是在构建时定义的宏，默认值为1e-6。如果需要更高或更低的精度要求，可以在构建项目时自定义这个宏的值。

#### 字符串类型比较

variant 支持与std::string和std::string_view类型的比较。对于string_type和blob_type的variant都可以与字符串比较。

```cpp
std::string test_str = "Hello, World!";
variant v_str(test_str);
variant v_blob(blob{{72, 101, 108, 108, 111, 44, 32, 87, 111, 114, 108, 100, 33}}); // "Hello, World!"的ASCII码

// 与字符串比较
bool equal1 = (v_str == test_str);     // true
bool equal2 = (test_str == v_str);     // true，反向比较也支持
bool equal3 = (v_blob == test_str);    // true，blob类型可以与字符串比较

// 大小比较（按字典序）
bool less = (v_str < "Zebra");         // true，"Hello" < "Zebra"
bool greater = (v_str > "Apple");      // true，"Hello" > "Apple"
bool less_equal = (v_str <= test_str); // true，同一字符串
```

#### 容器类型比较

variant 支持与variants、dict和blob等容器类型的比较。

```cpp
// 数组比较
variants arr1 = {1, 2, 3};
variants arr2 = {1, 2, 3};
variants arr3 = {1, 2, 4};
variant v_arr(arr1);

bool equal = (v_arr == arr2);      // true，内容相同
bool not_equal = (v_arr != arr3);  // true，内容不同
bool less = (v_arr < arr3);        // true，[1,2,3] < [1,2,4]

// 对象比较
dict d1 = {{"a", 1}, {"b", 2}};
dict d2 = {{"a", 1}, {"b", 2}};
dict d3 = {{"a", 1}, {"b", 3}};
variant v_dict(d1);

bool equal = (v_dict == d2);       // true，内容相同
bool not_equal = (v_dict != d3);   // true，内容不同
bool less = (v_dict < d3);         // true，{"a":1,"b":2} < {"a":1,"b":3}
```

### 性能优势

直接使用比较操作符比先进行类型转换再比较具有明显的性能优势：

```cpp
variant v(42);

// 不推荐：先转换再比较
if (v.as<int>() == 42) {
    // 涉及类型检查、转换以及可能的异常处理
}

// 推荐：直接使用比较操作符
if (v == 42) {
    // 更高效，避免了不必要的类型转换
}
```

在处理大量数据或性能关键的代码中，直接使用比较操作符可以显著提高性能。

### 特殊情况处理

1. **NaN值比较**：当variant包含NaN（非数字）值时，比较行为遵循IEEE-754标准
   ```cpp
   variant v_nan(std::numeric_limits<double>::quiet_NaN());
   bool equal = (v_nan == v_nan);       // false，NaN不等于自身
   bool not_equal = (v_nan != v_nan);   // true，NaN不等于自身
   bool less = (v_nan < 0.0);           // false
   bool greater = (v_nan > 0.0);        // false
   ```

2. **不兼容类型比较**：当比较不兼容的类型时，会抛出mc::invalid_op_exception异常
   ```cpp
   variant v_num(42);
   variant v_str("test");
   try {
       bool result = (v_num < v_str);    // 抛出异常
   } catch (const mc::invalid_op_exception& e) {
       // 异常消息包含详细的类型信息
       std::cout << e.what() << std::endl; // "不支持的类型比较操作: int32_type < string_type"
   }
   ```

3. **null值比较**：null值在比较中遵循特定规则
   ```cpp
   variant v_null;  // null值
   variant v_int(0);
   bool equal = (v_null == nullptr);    // true
   bool less = (v_null < v_int);        // true，null被视为小于任何非null值
   ```

## 相关类型

- `mc::dict` - 不可变的键值对集合
- `mc::mutable_dict` - 可变的键值对集合
- `mc::variants` - variant数组
- `mc::blob` - 二进制数据容器

## 实现细节

`variant`是`variant_base<>`的类型别名，后者是一个模板类。它使用`type_id`枚举来跟踪当前存储的值的类型，并使用联合体(union)来存储值：

```cpp
union {
    double          m_double;
    int64_t         m_int64;
    uint64_t        m_uint64;
    bool            m_bool;
    string_ptr_type m_string_ptr;
    blob_ptr_type   m_blob_ptr;
    array_ptr_type  m_array_ptr;
    object_ptr_type m_object_ptr;
};
```

对于大类型（字符串、dict、数组、blob），variant使用指针来存储，从而保持variant对象本身的大小相对较小。

## 最佳实践

1. **优先使用try_as或带默认值的as**

   使用`try_as<T>()`或`as<T>(default_value)`而不是直接使用`as<T>()`，可以避免在类型不匹配时抛出异常，使代码更加健壮。

   ```cpp
   // 避免这样做
   try {
       int i = v.as<int>();
       // 使用i
   } catch (const mc::bad_cast_exception& e) {
       // 处理错误
   }

   // 推荐这样做
   if (auto opt = v.try_as<int>()) {
       int i = *opt;
       // 使用i
   } else {
       // 处理错误
   }

   // 或者这样
   int i = v.as<int>(0);  // 转换失败时使用默认值0
   ```

2. **使用visit_with处理多种类型**

   当需要根据variant的实际类型执行不同操作时，使用`visit_with`方法和lambda表达式，而不是一系列的类型检查和转换。

   ```cpp
   // 避免这样做
   if (v.is_int32()) {
       int i = v.as<int>();
       // 处理整数
   } else if (v.is_string()) {
       std::string s = v.as<std::string>();
       // 处理字符串
   } else {
       // 处理其他类型
   }

   // 推荐这样做
   v.visit_with([](auto&& value) {
       using T = std::decay_t<decltype(value)>;
       if constexpr (std::is_same_v<T, int64_t>) {
           // 处理整数
       } else if constexpr (std::is_same_v<T, std::string>) {
           // 处理字符串
       } else {
           // 处理其他类型
       }
   });
   ```

3. **容器直接转换**

   对于容器类型，使用直接转换而不是手动逐个元素处理。

   ```cpp
   // 避免这样做
   std::vector<int> vec;
   const variants& arr = v.get_array();
   for (const auto& item : arr) {
       vec.push_back(item.as<int>());
   }

   // 推荐这样做
   std::vector<int> vec = v.as<std::vector<int>>();
   ```

4. **比较操作**

   直接使用比较操作符（`==`、`!=`、`<`、`>`、`<=`、`>=`）进行比较，而不是先进行类型转换再比较。这不仅代码更简洁，而且性能更好，可以避免不必要的类型转换和潜在的异常处理开销。详见[比较操作符](#比较操作符)章节。

   ```cpp
   // 避免这样做
   if (v.as<int>() == 42) {
       // 涉及类型检查、转换以及可能的异常处理
   }

   // 推荐这样做
   if (v == 42) {
       // 更高效，避免了不必要的类型转换
   }
   
   // 避免这样做
   if (v.as<string>() < "abc") {
       // 创建临时字符串对象进行比较
   }
   
   // 推荐这样做
   if (v < "abc") {
       // 直接比较，无需类型转换
   }
   ```

   variant类型支持与多种基础类型的直接比较，包括整数、浮点数、字符串等，以及容器类型如dict、variants和blob。

5. **性能考虑**

   在循环中使用`as<T>()`方法可能会有性能开销，可以先检查类型或者先进行一次转换。

   ```cpp
   // 避免这样做
   for (const auto& item : items) {
       int value = item["value"].as<int>();
       // 处理value
   }

   // 推荐这样做（如果确信所有item["value"]都是int类型）
   for (const auto& item : items) {
       if (item.contains("value") && item["value"].is_integer()) {
           int value = item["value"].as<int>();
           // 处理value
       }
   }
   ```

6. **对大型数据使用get_*方法而非as_*方法**

   对于string、blob、object(dict)和array(variants)等大型数据类型，`as_*`方法返回的是数据的拷贝，而`get_*`方法返回的是内部数据的引用，后者性能更高。

   ```cpp
   // 避免这样做
   std::string str = v.as_string();  // 创建字符串拷贝
   dict d = v.as_dict();             // 创建dict拷贝
   variants arr = v.as_array();      // 创建数组拷贝
   blob b = v.as_blob();             // 创建blob拷贝
   
   // 推荐这样做
   const std::string& str = v.get_string();  // 获取字符串引用
   const dict& d = v.get_object();           // 获取dict引用
   const variants& arr = v.get_array();      // 获取数组引用
   const blob& b = v.get_blob();             // 获取blob引用
   ```

   在只需读取数据而不需修改的场景中，使用`get_*`方法可以避免不必要的拷贝操作，显著提高性能，特别是对于大型数据结构。

   **注意:** `get_*`方法必须在确保类型正确的情况下使用，否则会抛出异常。在使用前应当先检查类型：

   ```cpp
   // 安全使用get_*方法的推荐做法
   if (v.is_string()) {
       const std::string& str = v.get_string();  // 安全
       // 使用str
   }
   else if (v.is_dict()) {
       const dict& d = v.get_object();  // 安全
       // 使用d
   }
   else if (v.is_array()) {
       const variants& arr = v.get_array();  // 安全
       // 使用arr
   }
   
   // 或者结合try_as进行安全访问
   if (v.is_string()) {
       const std::string& str = v.get_string();
   } else {
       // 使用默认值或其他处理方式
       std::string default_str = "默认值";
       // 或者使用try_as避免异常
       auto opt_str = v.try_as<std::string>();
       if (opt_str) {
           // 处理有效的字符串
       }
   }
   ``` 