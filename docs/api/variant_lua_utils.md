# Lua 绑定 API 文档

## 概述

本文档介绍 `mc::variant` 和 `mc::variants` 与 Lua 堆栈之间的双向转换接口（命名空间：`mc::lua`）。这些接口支持在 C++ 和 Lua 之间无缝传递数据，适用于脚本引擎集成、配置管理、数据交换等场景。

## 目录

- [API 接口](#api-接口)
- [类型转换映射](#类型转换映射)
- [应用场景](#应用场景)
- [使用示例](#使用示例)
- [技术细节](#技术细节)
- [注意事项](#注意事项)

## API 接口

### C++ -> Lua 转换

#### variant_to_lua

将 `mc::variant` 的值推入 Lua 堆栈作为返回值。

**函数签名：**
```cpp
int variant_to_lua(::lua_State* L, const variant& v);
```

**参数：**
- `L`: Lua 状态机指针
- `v`: 要转换的 variant 值

**返回值：**
- 推入堆栈的元素数量（通常为 1）

**功能：**
- 根据 variant 的类型，将值转换为对应的 Lua 类型并推入堆栈
- 支持所有 variant 支持的类型（null、整数、浮点数、布尔值、字符串、数组、对象、blob、extension）
- 递归处理嵌套结构（数组和对象）
- **支持直接传入 `mc::variants` 类型**（会隐式转换为 variant，并作为 Lua table 推入）

**应用场景：**
- 从 C++ 函数返回 variant 值给 Lua
- 将 C++ 配置数据传递给 Lua 脚本
- 在 Lua 中访问 C++ 对象属性
- **将 `mc::variants` 作为 Lua table 传递**：`variant_to_lua(L, vs)`

#### variants_to_lua

将 `mc::variants` 作为多个返回值推入 Lua 堆栈。

**函数签名：**
```cpp
int variants_to_lua(::lua_State* L, const variants& vs);
```

**参数：**
- `L`: Lua 状态机指针
- `vs`: 要转换的 variants 值

**返回值：**
- 推入堆栈的元素数量（等于 variants 的大小）

**功能：**
- 将 variants 中的每个元素依次推入堆栈
- 支持 Lua 函数返回多个值
- **注意**：这与 `variant_to_lua(L, vs)` 不同，后者将 variants 作为单个 table 推入

**应用场景：**
- 从 C++ 函数返回多个值给 Lua
- 批量传递数据给 Lua 脚本

**与 `variant_to_lua` 的区别：**
```cpp
mc::variants vs = {1, 2, 3};

// variants_to_lua: 推入 3 个值到栈上
variants_to_lua(L, vs);  // 栈: 1, 2, 3  (返回 3)

// variant_to_lua: 推入 1 个 table 到栈上  
variant_to_lua(L, vs);   // 栈: {1, 2, 3}  (返回 1)
```

### Lua -> C++ 转换

#### lua_to_variant

从 Lua 堆栈读取一个值并转换为 `mc::variant`。

**函数签名：**
```cpp
variant lua_to_variant(::lua_State* L, int index);
```

**参数：**
- `L`: Lua 状态机指针
- `index`: 堆栈索引（可以是负数，表示从栈顶开始计数）
  - 正数：从栈底开始计数（1 是第一个元素）
  - 负数：从栈顶开始计数（-1 是栈顶元素）

**返回值：**
- 转换后的 variant 值

**功能：**
- 根据 Lua 值的类型，转换为对应的 variant 类型
- 自动判断 table 是数组还是字典
- 递归处理嵌套结构

**应用场景：**
- Lua 调用 C++ 函数时，从堆栈获取调用参数
- 在 C++ 函数中处理 Lua 传递的参数
- 从 Lua 堆栈读取配置数据或全局变量

#### lua_to_variants（三参数版本）

从 Lua 堆栈读取多个值并转换为 `mc::variants`。

**函数签名：**
```cpp
variants lua_to_variants(::lua_State* L, int start_index, int count);
```

**参数：**
- `L`: Lua 状态机指针
- `start_index`: 起始堆栈索引
- `count`: 要读取的元素数量
  - 正数：读取指定数量的元素
  - -1：读取从 start_index 到栈顶的所有元素

**返回值：**
- 转换后的 variants 值

**功能：**
- 从指定索引开始，读取指定数量的值
- 支持读取到栈顶的所有值（count = -1）

**应用场景：**
- Lua 调用 C++ 函数时，获取多个调用参数
- 批量读取 Lua 堆栈中的参数数据

#### lua_to_variants（两参数版本）

从 Lua 堆栈读取从指定索引到栈顶的所有值并转换为 `mc::variants`。

**函数签名：**
```cpp
variants lua_to_variants(::lua_State* L, int start_index);
```

**参数：**
- `L`: Lua 状态机指针
- `start_index`: 起始堆栈索引

**返回值：**
- 转换后的 variants 值

**功能：**
- 便捷接口，自动读取从 start_index 到栈顶的所有值
- 内部调用三参数版本，传入 `count = -1`

**应用场景：**
- 简化调用，无需指定参数数量
- Lua 调用 C++ 函数时，获取所有调用参数

## 类型转换映射

### C++ -> Lua 类型映射

| variant 类型 | Lua 类型 | 说明 |
|-------------|---------|------|
| `null_type` | `nil` | 空值 |
| `int8_type`, `int16_type`, `int32_type`, `int64_type` | `number` (lua_Integer) | 有符号整数 |
| `uint8_type`, `uint16_type`, `uint32_type`, `uint64_type` | `number` (lua_Number) | 无符号整数（Lua 5.1 使用 number） |
| `double_type` | `number` (lua_Number) | 双精度浮点数 |
| `bool_type` | `boolean` | 布尔值 |
| `string_type` | `string` | 字符串 |
| `array_type` (variants) | `table` (数组) | 数组形式，索引从 1 开始 |
| `object_type` (dict) | `table` (字典) | 键值对形式 |
| `blob_type` | `string` | 二进制数据转换为字符串 |
| `extension_type` | `string` | 通过 `as_string()` 转换为字符串表示 |

### Lua -> C++ 类型映射

| Lua 类型 | variant 类型 | 说明 |
|---------|-------------|------|
| `nil` | `null_type` | 空值 |
| `number` (整数) | 根据值大小选择：`int8_type`, `int16_type`, `int32_type`, `int64_type` | 自动选择最合适的整数类型 |
| `number` (浮点数) | `double_type` | 双精度浮点数 |
| `boolean` | `bool_type` | 布尔值 |
| `string` | `string_type` | 字符串 |
| `table` (数组) | `array_type` (variants) | 所有键是连续正整数从 1 开始 |
| `table` (字典) | `object_type` (dict) | 包含非整数键或键不连续 |

### Table 类型判断规则

Lua table 会被判断为数组或字典，判断规则如下：

**数组条件（全部满足）：**
1. 所有键都是正整数
2. 键从 1 开始连续
3. 没有其他非整数键
4. 空 table 被视为数组

**字典条件（不满足数组条件）：**
- 包含非整数键
- 整数键不连续
- 整数键不从 1 开始

**示例：**
```lua
-- 数组
{1, 2, 3}                    -- 数组
{"a", "b", "c"}              -- 数组（键是 1, 2, 3）
{}                           -- 数组（空 table）

-- 字典
{name="张三", age=30}         -- 字典（非整数键）
{[1]=100, [3]=300}           -- 字典（键不连续）
{a=1, b=2, 1="first"}         -- 字典（混合键）
```

## 应用场景

### 1. 脚本引擎集成

在 C++ 应用中嵌入 Lua 脚本引擎，实现动态配置和扩展功能。

```cpp
// C++ 调用 Lua 函数（C++ -> Lua）
lua_State* L = luaL_newstate();
luaL_openlibs(L);

// 传递参数给 Lua
mc::variant config = load_config();
mc::lua::variant_to_lua(L, config);
lua_setglobal(L, "config");

// 调用 Lua 函数
lua_getglobal(L, "process_data");
lua_pcall(L, 0, 1, 0);

// 获取返回值（从 Lua 堆栈读取）
mc::variant result = mc::lua::lua_to_variant(L, -1);
lua_pop(L, 1);

// Lua 调用 C++ 函数（Lua -> C++）
int lua_process_data(lua_State* L) {
    // 从堆栈获取 Lua 传递的参数
    mc::variant input = mc::lua::lua_to_variant(L, 1);
    
    // 处理数据
    mc::variant output = process(input);
    
    // 返回结果给 Lua
    mc::lua::variant_to_lua(L, output);
    return 1; // 返回 1 个值
}
```

### 2. 配置管理

从 Lua 脚本中读取配置数据，或向 Lua 脚本传递配置。

```cpp
// 从 Lua 脚本读取配置（从堆栈读取全局变量）
luaL_dofile(L, "config.lua");
lua_getglobal(L, "app_config");
mc::variant config = mc::lua::lua_to_variant(L, -1);
lua_pop(L, 1);

// 使用配置
if (config.is_dict()) {
    const auto& dict = config.get_object();
    std::string name = dict["name"].as_string();
    int port = dict["port"].as_int64();
}

// Lua 调用 C++ 函数获取配置
int lua_get_config(lua_State* L) {
    // 从堆栈获取 Lua 传递的配置键
    std::string key = mc::lua::lua_to_variant(L, 1).as_string();
    
    // 获取配置值
    mc::variant value = get_config_value(key);
    
    // 返回给 Lua
    mc::lua::variant_to_lua(L, value);
    return 1;
}
```

### 3. 数据交换

在 C++ 和 Lua 之间传递复杂数据结构。

```cpp
// C++ -> Lua：传递复杂数据
mc::dict data = {
    {"users", mc::variants{"张三", "李四", "王五"}},
    {"settings", mc::dict{{"theme", "dark"}, {"lang", "zh-CN"}}}
};
mc::variant v_data(data);
mc::lua::variant_to_lua(L, v_data);
lua_setglobal(L, "app_data");

// Lua -> C++：Lua 调用 C++ 函数处理数据
int lua_process_data(lua_State* L) {
    // 从堆栈获取 Lua 传递的数据
    mc::variant input = mc::lua::lua_to_variant(L, 1);
    
    // 处理数据
    mc::variant result = process_data(input);
    
    // 返回结果给 Lua
    mc::lua::variant_to_lua(L, result);
    return 1;
}
```

### 4. 插件系统

实现插件系统，允许 Lua 插件与 C++ 核心系统交互。

```cpp
// 插件调用 C++ API
int lua_plugin_call(lua_State* L) {
    // 获取 Lua 传递的参数
    mc::variants args = mc::lua::lua_to_variants(L, 1, -1);
    
    // 调用 C++ 函数处理
    mc::variants results = process_plugin_request(args);
    
    // 返回结果给 Lua
    return mc::lua::variants_to_lua(L, results);
}
```

### 5. 测试和调试

在测试中使用 Lua 脚本生成测试数据或验证结果。

```cpp
// 使用 Lua 生成测试数据（从堆栈读取返回值）
luaL_dostring(L, "return {1, 2, 3, {name='test', value=42}}");
mc::variant test_data = mc::lua::lua_to_variant(L, -1);
lua_pop(L, 1);

// Lua 调用 C++ 测试函数
int lua_test_function(lua_State* L) {
    // 从堆栈获取测试参数
    mc::variants args = mc::lua::lua_to_variants(L, 1); // 获取所有参数
    
    // 执行测试
    bool result = run_test(args);
    
    // 返回结果
    mc::lua::variant_to_lua(L, result);
    return 1;
}
```

## 使用示例

### 基本用法

```cpp
#include <mc/variant/variant_lua_utils.h>
#include <lua.h>
#include <lauxlib.h>

// 创建 Lua 状态机
lua_State* L = luaL_newstate();
luaL_openlibs(L);

// C++ -> Lua：将 variant 推入堆栈（用于返回值或设置全局变量）
mc::variant v = 42;
mc::lua::variant_to_lua(L, v);
// 现在堆栈顶部有一个值为 42 的 number

// Lua -> C++：Lua 调用 C++ 函数时，从堆栈获取参数
int lua_add_numbers(lua_State* L) {
    // 从堆栈获取 Lua 传递的两个参数
    int a = mc::lua::lua_to_variant(L, 1).as_int64();
    int b = mc::lua::lua_to_variant(L, 2).as_int64();
    
    // 计算结果并返回给 Lua
    mc::lua::variant_to_lua(L, a + b);
    return 1; // 返回 1 个值
}

// 清理
lua_close(L);
```

### variants 转换

```cpp
// 将 variants 作为多个返回值推入堆栈
mc::variants vs = {1, 2, 3};
int count = mc::lua::variants_to_lua(L, vs);
// 堆栈顶部有 3 个值：1, 2, 3

// 将 variants 作为单个 table 推入堆栈
mc::variants vs2 = {10, 20, 30};
int count2 = mc::lua::variant_to_lua(L, vs2);  // 直接传入 variants，会隐式转换
// 堆栈顶部有 1 个 table，包含 {1=10, 2=20, 3=30}

// Lua -> C++：Lua 调用 C++ 函数时，获取多个参数
int lua_process_args(lua_State* L) {
    // 从堆栈获取所有参数（从索引 1 开始）
    mc::variants args = mc::lua::lua_to_variants(L, 1); // 读取所有参数
    
    // 或者指定参数数量
    mc::variants args2 = mc::lua::lua_to_variants(L, 1, 3); // 读取 3 个参数
    
    // 处理参数并返回结果
    mc::variant result = process(args);
    mc::lua::variant_to_lua(L, result);
    return 1;
}
```

### 复杂类型转换

```cpp
// 字典转换
mc::dict obj = {{"name", "张三"}, {"age", 30}};
mc::variant v_obj(obj);
mc::lua::variant_to_lua(L, v_obj);
// 堆栈顶部有一个 table，包含 {"name"="张三", "age"=30}

// 数组转换
mc::variants arr = {1, "hello", true};
mc::variant v_arr(arr);
mc::lua::variant_to_lua(L, v_arr);
// 堆栈顶部有一个 table，包含 {1=1, 2="hello", 3=true}

// 嵌套结构
mc::dict nested = {
    {"users", mc::variants{"张三", "李四"}},
    {"config", mc::dict{{"theme", "dark"}}}
};
mc::variant v_nested(nested);
mc::lua::variant_to_lua(L, v_nested);
```

### Lua 调用 C++ 函数示例

```cpp
// 注册 C++ 函数供 Lua 调用
int lua_calculate(lua_State* L) {
    // 从堆栈获取 Lua 传递的参数
    int a = mc::lua::lua_to_variant(L, 1).as_int64();
    int b = mc::lua::lua_to_variant(L, 2).as_int64();
    
    // 计算并返回多个值
    mc::variants results = {a + b, a - b, a * b};
    return mc::lua::variants_to_lua(L, results); // 返回多个值
}

// 在 Lua 中使用
// local sum, diff, prod = calculate(10, 20)
```

## 技术细节

### Lua 版本支持

实现仅支持 **Lua 5.1** 版本，使用以下 Lua 5.1 特有的 API：

- `lua_objlen`: 获取 table 的数组部分大小
- `lua_rawgeti`: 从 table 中获取整数索引的值
- `lua_tonumber` / `lua_pushnumber`: 处理 number 类型（Lua 5.1 只有 number 类型，没有独立的 integer 类型）

### 错误处理

所有函数使用异常处理机制：

- **无效的堆栈索引**：抛出 `mc::exception`，包含索引信息
- **不支持的类型**：抛出 `mc::exception`，包含类型信息
- **转换失败**：抛出 `mc::exception`，包含详细错误信息

### 内存管理

- **variant 和 variants**：使用 RAII 管理内存，自动释放资源
- **Lua 堆栈**：由 Lua 虚拟机管理，调用者负责保持堆栈平衡
- **字符串数据**：Lua 会复制字符串数据，确保在 Lua 使用期间有效

### 性能考虑

- **预分配**：创建 table 时预分配容量，减少扩容操作
- **引用访问**：使用 `get_array()` 和 `get_object()` 获取引用，避免拷贝
- **递归优化**：递归转换时保持堆栈平衡，避免堆栈溢出

## 注意事项

1. **堆栈管理**
   - 调用者需要负责管理 Lua 堆栈，确保不会溢出
   - 每次推入值后，需要相应弹出或使用
   - 使用负数索引时要注意堆栈状态

2. **索引规范**
   - 支持正数和负数索引
   - 正数索引从 1 开始（栈底）
   - 负数索引从 -1 开始（栈顶）
   - 索引必须在有效范围内（1 到 `lua_gettop(L)`）

3. **类型选择**
   - 整数类型会根据值的大小自动选择最合适的类型
   - 大整数可能转换为 `int64_type` 或 `uint64_type`
   - 浮点数统一使用 `double_type`

4. **Table 判断**
   - 空 table 被视为数组
   - 判断基于键的类型和连续性
   - 混合键的 table 会被视为字典

5. **Extension 类型**
   - extension 类型会通过 `as_string()` 转换为字符串表示
   - 如果需要保留原始类型信息，需要在转换前处理

6. **嵌套结构**
   - 支持任意深度的嵌套结构
   - 递归转换会自动处理嵌套的数组和对象
   - 注意避免循环引用

## 文件组织

```
include/mc/lua/
  └── variant_utils.h        # Lua 转换工具函数声明

src/luaclib/
  ├── utils/
  │   └── variant_utils.cpp      # Lua 转换工具函数实现
  └── dft/
      └── variant_convert.cpp  # DFT 性能测试模块

docs/api/
  └── variant_lua_utils.md   # 本文档
```

## 相关文档

- [variant API 文档](../api/variant.md)
- [variants API 文档](../api/variants.md)
- [dict API 文档](../api/dict.md)
