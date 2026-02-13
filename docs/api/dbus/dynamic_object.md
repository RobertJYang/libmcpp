# dynamic_object 动态对象 API

## 概述

`mc::dbus::dynamic_object` 和 `mc::dbus::dynamic_interface` 提供了一套**运行时可配置**的 DBus 对象模型：

- 不依赖编译期反射和 `MC_OBJECT`/`MC_INTERFACE` 宏；
- 接口、属性、信号都可以在运行时动态创建；
- 主要用于 Lua `ldbus.object` / `ldbus.interface` 绑定，但也可以直接在 C++ 中使用；
- 可以通过 `mc::dbus::sd_bus::register_object()` 注册到 DBus 总线，实现完全动态的服务对象。

典型使用场景：

- 插件或脚本驱动的对象模型（接口和属性定义来自配置 / JSON / Lua）；
- 测试或模拟环境：快速构造一个 DBus 对象树；
- 为 Lua 暴露一个通用的“动态对象”实现。

## 相关类型

### access_property_rsp_code

头文件：`mc/dbus/dynamic_object.h`

```cpp
enum class access_property_rsp_code : int {
    success                        = 0,
    set_sync_property_err          = -1,
    set_ref_property_err           = -2,
    property_convert_type_err      = -3,
    set_property_unknown_err       = -4,
    property_not_exist_err         = -5,
    access_ref_object_property_err = -6,
    interface_not_exist_err        = -7,
    object_not_exist_err           = -8,
    get_ref_property_err           = -9,
    expression_parameter_err       = -10,
    property_invalid_err           = -11
};
```

**含义：**

- **success**：访问成功；
- **interface_not_exist_err**：指定接口不存在；
- **object_not_exist_err**：对象不存在（预留，当前动态对象内部未使用）；
- **property_not_exist_err**：指定属性在接口上不存在；
- **property_invalid_err**：属性值非法或处理过程中抛出异常；
- **set_property_unknown_err**：`set_property()` 内部抛出异常（一般是只读属性被写，或其他运行时错误）。

该枚举主要用于：

- `dynamic_object::try_get_property()`；
- `dynamic_object::try_set_property()`；
- Lua 绑定 `ldbus.object:get_property()` / `set_property()` 返回的状态码。

### dynamic_method / dynamic_property / dynamic_signal

```cpp
struct dynamic_method {
    std::string name;
    std::string i_args;  // 输入签名
    std::string o_args;  // 输出签名
    int32_t     cb_ref;  // （预留）回调引用，当前主要由 Lua 层使用
    uint8_t     flags;
};

struct dynamic_property {
    std::string                  name;
    std::string                  signature;  // DBus 类型签名
    mc::variant                  value;      // 当前值
    bool                         readonly;   // 只读标志
    uint8_t                      flags;      // 预留扩展标志
    shm::weak_ptr<shm::property> shm_prop;   // 共享内存属性指针（可选）
};

struct dynamic_signal {
    std::string name;
    std::string signature;  // DBus 类型签名
    uint8_t     flags;      // 预留扩展标志
};
```

`dynamic_property` 是本模块的核心之一，用于在运行时为接口增加属性，并通过 `mc::variant` 保存其值。

## mc::dbus::dynamic_interface

头文件：`mc/dbus/dynamic_object.h`  
实现：`src/dbus/dynamic_object.cpp`

```cpp
class MC_API dynamic_interface : public mc::core::object {
public:
    explicit dynamic_interface(std::string_view name);

    void             add_property(std::string_view name, dynamic_property prop);
    void             add_method(std::string_view name, dynamic_method m);
    void             add_signal(std::string_view name, dynamic_signal s);
    bool             set_property(std::string property_name, const mc::variant& value);
    mc::variant      get_property(std::string property_name) const;
    bool             has_property(std::string property_name) const;
    std::string_view get_name() const;
    mc::dict         get_all_properties() const;
    void             update_shm_prop(std::string_view property_name, const mc::variant& value);
};
```

### 属性访问语义

- **add_property(name, prop)**  
  在接口上注册一个新属性。`prop.name` 与 `name` 字符串应保持一致。

- **set_property(property_name, value)**  
  - 当属性存在且非只读：更新 `value`，返回 `true`；
  - 当属性不存在：返回 `false`；
  - 当属性为只读：抛出 `mc::exception("property is read-only")`。

- **get_property(property_name)**  
  - 当属性存在：返回 `mc::variant` 值；
  - 当属性不存在：返回默认构造的 `mc::variant`（即“空”值）。

- **get_all_properties()**  
  返回 `mc::dict` 映射：键为属性名字符串，值为 `mc::variant`。

- **update_shm_prop(property_name, value)**  
  仅更新属性的共享内存表示，不修改本地 `prop.value`。
  
  **行为：**
  - 当属性存在且 `shm_prop` 有效：更新共享内存中的属性值，返回；
  - 当属性不存在或 `shm_prop` 无效：静默返回，不执行任何操作。
  
  **与 `set_property` 的区别：**
  - `set_property`：同时更新本地值（`prop.value`）和共享内存（如果 `shm_prop` 有效）；
  - `update_shm_prop`：仅更新共享内存，不修改本地值。
  
  **使用场景：**
  - 当属性已链接到共享内存，需要同步更新共享内存而不改变本地缓存时；
  - 在属性值由外部源更新后，需要将新值同步到共享内存时。

## mc::dbus::dynamic_object

头文件：`mc/dbus/dynamic_object.h`  
实现：`src/dbus/dynamic_object.cpp`

```cpp
class MC_API dynamic_object : public mc::engine::object_impl {
public:
    explicit dynamic_object(mc::core::object* parent = nullptr);

    mc::variant get_property(std::string_view property_name,
                             std::string_view interface_name,
                             int options = 0) const override;

    bool set_property(std::string_view property_name,
                      const mc::variant& value,
                      std::string_view interface_name) override;

    mc::dict get_all_properties(std::string_view interface_name = {},
                                int options = 0) const override;

    bool has_property(std::string_view property_name,
                      std::string_view interface_name) const override;

    std::tuple<int, mc::variant> try_get_property(std::string_view property_name,
                                                  std::string_view interface_name) const;

    int try_set_property(std::string_view property_name,
                         const mc::variant& value,
                         std::string_view interface_name);

    void                              add_interface(mc::shared_ptr<dynamic_interface>);
    mc::shared_ptr<dynamic_interface> get_interface(std::string intf_name) const;

    std::string_view                   get_class_name() const override;
    std::string_view                   get_path_pattern() const override;
    const mc::engine::object_metadata& get_metadata() const override;

    void update_shm_prop(std::string_view property_name, const mc::variant& value, std::string_view interface_name);
};
```

### 接口管理

- **add_interface(shared_ptr<dynamic_interface>)**  
  将接口按名称（`dynamic_interface::get_name()`）挂载到当前对象。

- **get_interface(name)**  
  - 找到时返回对应的 `shared_ptr<dynamic_interface>`；
  - 否则返回 `nullptr`。

注意：`dynamic_object` 内部维护一张 `std::map<std::string, shared_ptr<dynamic_interface>> m_interfaces`，键是接口名字符串。

### 属性访问（C++ API）

#### set_property / get_property / has_property

这些方法实现了 `mc::engine::abstract_object` 的虚函数，供引擎和上层框架直接调用：

- `set_property(property_name, value, interface_name)`  
  - 当接口存在且接口上的 `set_property()` 返回 `true`：返回 `true`；  
  - 当接口不存在：返回 `false`；  
  - 当接口存在但属性不存在：返回 `false`；  
  - 当属性只读或出现异常：抛出 `mc::exception`。

- `get_property(property_name, interface_name, options)`  
  - 当接口 / 属性存在：返回属性值；
  - 当接口不存在或属性不存在：返回空 `mc::variant`；
  - `options` 参数目前未使用（为与其它对象实现保持一致而保留）。

- `has_property(property_name, interface_name)`  
  - 接口存在且属性存在 -> `true`；
  - 否则 `false`。

- `get_all_properties(interface_name, options)`  
  - 当接口存在：返回该接口上所有属性组成的 `mc::dict`；
  - 当接口不存在：返回空 `mc::dict`。

#### try_get_property / try_set_property

`try_*` 方法适合需要显式错误码而不是异常的场景（例如 Lua 绑定）：

```cpp
std::tuple<int, mc::variant>
dynamic_object::try_get_property(std::string_view property_name,
                                 std::string_view interface_name) const;

int dynamic_object::try_set_property(std::string_view property_name,
                                     const mc::variant& value,
                                     std::string_view interface_name);
```

**try_get_property 行为：**

- 接口不存在：返回 `{ static_cast<int>(access_property_rsp_code::interface_not_exist_err), {} }`；
- 属性不存在：返回 `{ static_cast<int>(access_property_rsp_code::property_not_exist_err), {} }`；
- 正常获取：返回 `{ static_cast<int>(access_property_rsp_code::success), value }`；
- `dynamic_interface::get_property` 抛出异常：返回 `{ static_cast<int>(access_property_rsp_code::property_invalid_err), {} }`。

**try_set_property 行为：**

- 接口不存在：返回 `static_cast<int>(access_property_rsp_code::interface_not_exist_err)`；
- 属性不存在：返回 `static_cast<int>(access_property_rsp_code::property_not_exist_err)`；
- 正常设置：返回 `static_cast<int>(access_property_rsp_code::success)`；
- `dynamic_interface::set_property` 抛出异常（例如写只读属性）：  
  返回 `static_cast<int>(access_property_rsp_code::set_property_unknown_err)`。

Lua 绑定 `ldbus.object` 的 `get_property` / `set_property` 会直接将这些错误码返回给 Lua 侧（见 `src/luaclib/dbus/inner/l_object.cpp`）。

#### update_shm_prop

`update_shm_prop` 方法用于仅更新属性的共享内存表示，而不修改本地缓存值：

```cpp
void dynamic_object::update_shm_prop(std::string_view property_name,
                                      const mc::variant& value,
                                      std::string_view interface_name);

void dynamic_interface::update_shm_prop(std::string_view property_name,
                                        const mc::variant& value);
```

**行为：**

- **dynamic_object::update_shm_prop**：
  - 接口不存在：静默返回，不执行任何操作；
  - 接口存在：调用接口的 `update_shm_prop` 方法。

- **dynamic_interface::update_shm_prop**：
  - 属性不存在：静默返回；
  - 属性存在但 `shm_prop` 无效（`weak_ptr` 已过期或未设置）：静默返回；
  - 属性存在且 `shm_prop` 有效：更新共享内存中的属性值，**不修改** `prop.value`。

**与 `set_property` 的区别：**

| 方法 | 更新本地值 | 更新共享内存 | 检查只读标志 |
|------|-----------|-------------|-------------|
| `set_property` | ✅ 是 | ✅ 是（如果 `shm_prop` 有效） | ✅ 是 |
| `update_shm_prop` | ❌ 否 | ✅ 是（如果 `shm_prop` 有效） | ❌ 否 |

**使用场景：**

1. **外部值同步**：当属性值由外部源（如硬件读取、网络更新）更新后，需要将新值同步到共享内存，但不想覆盖本地缓存。
2. **共享内存优先**：当属性已链接到共享内存，需要确保共享内存中的值是最新的，而本地值可能稍后更新。
3. **性能优化**：避免不必要的本地值更新，直接更新共享内存以提高性能。

**注意事项：**

- `update_shm_prop` 不会检查属性的只读标志，也不会抛出异常（即使属性为只读也会更新共享内存）。
- 如果属性未链接到共享内存（`shm_prop` 无效），此方法不会产生任何效果。
- 此方法不会触发属性变更通知或信号。

### 元数据与对象路径

`dynamic_object` 继承自 `mc::engine::object_impl`，但不使用反射宏注册接口，因此：

- `get_class_name()` 返回空字符串（`""`）；
- `get_path_pattern()` 返回 `"*"`（匹配任意路径）；
- `get_metadata()` 通过懒初始化构造一个空的 `object_metadata`：

```cpp
const mc::engine::object_metadata& dynamic_object::get_metadata() const {
    if (!m_metadata) {
        if (!m_reflect_metadata) {
            m_reflect_metadata = std::make_unique<mc::reflect::struct_metadata>("dynamic_object", -1);
        }
        m_metadata = std::make_unique<mc::engine::object_metadata>("dynamic_object", *m_reflect_metadata);
    }
    return *m_metadata;
}
```

对象路径则通过基类 `object_impl::set_object_path()` 进行设置（例如在 Lua 的 `ldbus.object.new(path)` 中调用）。

## 与 sd_bus 的集成

`mc::dbus::sd_bus` 提供了注册动态对象的接口：

```cpp
// 头文件: mc/dbus/sd_bus.h
void register_object(mc::shared_ptr<dynamic_object> object);
```

使用方式：

1. 创建并配置 `dynamic_object`，设置对象路径；
2. 为其添加一个或多个 `dynamic_interface`；
3. 调用 `sd_bus::register_object()` 将其注册到总线。

注册后，sd_bus 的路径分发逻辑会将对该对象路径的 Properties/Get/Set 请求转发到 `dynamic_object`，再由 `dynamic_interface` 完成实际的属性读写。

## 与 Lua ldbus.object 的关系

Lua 模块 `ldbus.object` / `ldbus.interface` 实际上就是 `dynamic_object` / `dynamic_interface` 的包装层：

- `ldbus.interface.new(name)` -> `mc::make_shared<dynamic_interface>(name)`；
- `interface:add_property(name, signature, default_value, readonly, flags)` -> `dynamic_interface::add_property()`；
- `ldbus.object.new(path)` -> `mc::make_shared<dynamic_object>()` 并设置对象路径；
- `object:register_interface(interface)` -> `dynamic_object::add_interface()`；
- `object:set_property(intf_name, prop_name, value)` -> `dynamic_object::try_set_property()`；
- `object:get_property(intf_name, prop_name)` -> `dynamic_object::try_get_property()`。

详细的 Lua 使用示例参见：  
`docs/lua_api/dbus/object.md` 和 `tests/lua/dbus/test_object.lua`。

## 示例代码（C++）

```cpp
using namespace mc::dbus;

// 创建动态对象并设置路径
auto obj = mc::make_shared<dynamic_object>();
obj->set_object_path("/org/example/DynamicObject");

// 创建接口
auto intf = mc::make_shared<dynamic_interface>("org.example.Dynamic");

dynamic_property prop{
    "Status",
    "s",                  // string 签名
    mc::variant("init"),  // 默认值
    false,                // 非只读
    0
};

intf->add_property("Status", std::move(prop));

// 挂载接口到对象
obj->add_interface(intf);

// 注册到 sd_bus
sd_bus bus(true, false);
bus.request_name("org.example.DynamicService");
bus.register_object(obj);

// 之后可以通过 DBus 客户端读取 /org/example/DynamicObject 上
// org.example.Dynamic 接口的 Status 属性

// 示例：更新共享内存属性（不修改本地值）
obj->update_shm_prop("Status", mc::variant("updated_in_shm"), "org.example.Dynamic");
// 此时共享内存中的 Status 已更新，但 obj 的本地 prop.value 仍为 "init"
```

## 测试

与 `dynamic_object` 相关的单元测试包括：

- `tests/dbus/test_dynamic_object.cpp`  
  - 覆盖 `get_property` / `set_property` / `try_get_property` / `try_set_property` 行为；
  - 覆盖多接口、多属性、只读属性、数组属性等场景。

- `tests/lua/dbus/test_object.lua`  
  - 测试 Lua 层 `ldbus.object` / `ldbus.interface` 对动态对象的封装行为。

