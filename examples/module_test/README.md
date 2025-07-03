/*
 * Copyright (c) 2024 Huawei Technologies Co., Ltd.
 * openUBMC is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *         http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 */

# 简化模块系统示例

## 设计重点

基于您澄清的实际使用场景，重新调整了模块系统的设计重点：

### 🎯 核心定位
- **C++侧**: 实现模块，导出功能给 expr2 脚本使用
- **expr2脚本侧**: 使用 import/require 语法加载和使用模块
- **重点**: 简化C++模块的实现和导出体验

### 🔧 技术特点
1. **极简语法**: 只需两个宏即可定义模块
2. **导出为主**: 重点是将C++功能导出给脚本使用
3. **自动注册**: 模块会自动注册到模块管理器
4. **脚本友好**: 为expr2脚本引擎优化的接口设计

## 使用方法

### 1. 定义模块（头文件）

```cpp
#include <mc/module.h>

// 定义模块
MC_MODULE(devices)

namespace mc::devices {
    // 定义您的类和函数
    class sensor {
    public:
        void set_name(const std::string& name);
        const std::string& get_name() const;
        double read();
        // ...
    };
}

// 导出给脚本使用
MC_EXPORT((mc::devices::sensor, "Sensor"),
    ((set_name, "setName"))
    ((get_name, "getName"))
    ((read, "read")))
```

### 2. 实现模块（源文件）

```cpp
#include "devices.h"

// 实现模块反射工厂
MC_REGISTER_BUILTIN_MODULE(devices)

namespace mc::devices {
    // 实现您的类和函数
    void sensor::set_name(const std::string& name) {
        m_name = name;
    }
    // ...
}
```

### 3. 在expr2脚本中使用（未来）

```javascript
// 类似于 Lua 的 require
const devices = require("devices");

// 创建对象
const sensor = new devices.Sensor();
sensor.setName("温度传感器");
const value = sensor.read();
```

## 项目结构

```
simple_module/
├── simple_devices.h     # 模块接口定义
├── simple_devices.cpp   # 模块实现
├── main.cpp            # C++侧演示程序
├── README.md           # 说明文档
└── meson.build        # 构建配置
```

## 构建说明

```bash
# 在项目根目录
cd builddir
meson compile simple_devices_example
./examples/simple_module/simple_devices_example
```

## 设计优势

### 对于C++开发者
- **简洁**: 只需两个宏即可定义模块
- **清晰**: 明确的导出语义，知道哪些功能会被脚本使用
- **灵活**: 可以同时在C++和脚本中使用

### 对于脚本开发者
- **一致**: 所有模块都通过相同的require/import方式加载
- **完整**: 导出的功能包含完整的类型信息和方法
- **安全**: 通过反射系统确保类型安全

### 对于架构设计
- **隔离**: 每个模块有独立的反射工厂
- **可扩展**: 支持动态模块加载（预留）
- **高效**: 自动缓存和延迟加载

## 后续计划

1. **集成expr2**: 在expr2脚本引擎中实现真正的require/import语法
2. **动态加载**: 支持.so/.dll动态模块加载
3. **工具链**: 提供模块生成和管理工具
4. **文档完善**: 提供完整的开发指南和API文档

## 总结

这个设计真正实现了"**C++实现，脚本使用**"的目标，让C++开发者可以轻松地将功能导出给脚本系统使用，同时保持了架构的清晰性和可扩展性。

主要改进：
- ✅ 简化了C++侧的模块定义语法
- ✅ 突出了导出功能的核心作用
- ✅ 为expr2脚本引擎预留了完整的接口
- ✅ 移除了不必要的C++侧复杂功能

这个设计更符合实际的使用场景和需求！

# 模块系统测试

本示例演示了 libmcpp 模块系统的完整功能，包括模块加载、使用、卸载以及重新加载。

## 测试内容

### 1-5. 基础模块测试

包含了四种不同模式的模块加载测试：

1. **标准模块模式** (`mc.devices` -> `modules/mc/devices.so`)
2. **标准模块模式** (`mc.network` -> `modules/mc/network.so`)  
3. **init.so 模式** (`mc.protocol` -> `modules/mc/protocol/init.so`)
4. **深层路径模式** (`mc.test.database` -> `modules/mc/test/database.so`)

### 6. 模块两阶段卸载机制验证 ⭐

这是我们新增的重要功能，用于验证模块系统的两阶段卸载机制：

#### 什么是两阶段卸载？

模块系统采用智能指针管理，卸载分为两个阶段：

1. **第一阶段：逻辑卸载** - 调用 `module_manager.unload()` 
   - 仅从模块管理器中移除模块引用
   - 动态库仍在内存中，持有 `module_ptr` 的代码仍可使用
   
2. **第二阶段：物理卸载** - 释放所有 `module_ptr` 引用
   - 当最后一个 `module_ptr` 被销毁时
   - 动态库才真正从内存中卸载

#### 验证机制

我们在测试模块中添加了 **卸载检测器**：

```cpp
// 在 devices/module.cpp 和 network/module.cpp 中
class module_unload_detector {
public:
    module_unload_detector() {
        // 模块加载时打印
        std::cout << "[DEVICES MODULE] 模块加载 #" << ++s_load_count << std::endl;
    }
    
    ~module_unload_detector() {
        // 动态库真正卸载时打印
        std::cout << "[DEVICES MODULE] 动态库真正卸载!" << std::endl;
    }
};

// 全局对象，当动态库卸载时析构函数会被调用
static module_unload_detector g_unload_detector;
```

#### 测试流程

1. **保存模块引用** - 获取并持有 `module_ptr`
2. **第一阶段卸载** - 调用 `unload()` 但模块仍可用
3. **第二阶段卸载** - 释放 `module_ptr`，观察卸载日志
4. **重新加载验证** - 重新加载模块，验证真正的重新初始化

#### 预期输出

运行测试时，您应该看到类似输出：

```
[DEVICES MODULE] 模块加载 #1 (地址: 0x...)
调用 module_manager.unload('mc.devices')...
模块管理器状态: 未加载
✓ 模块引用仍然可用: after_unload_test
✓ 验证成功：第一阶段卸载后，持有引用的模块仍可使用
释放 devices_module_ref...
注意观察控制台输出，应该看到 '[DEVICES MODULE] 动态库真正卸载!' 消息
[DEVICES MODULE] 动态库真正卸载! 地址: 0x...
重新加载 devices 模块...
[DEVICES MODULE] 模块加载 #2 (地址: 0x...)
```

## 验证要点

✅ **逻辑卸载验证** - `unload()` 后模块管理器认为模块已卸载，但持有引用的代码仍可使用  
✅ **物理卸载验证** - 释放所有引用后，观察到析构函数被调用  
✅ **重新加载验证** - 重新加载后获得新的模块实例  
✅ **控制台输出验证** - 通过控制台输出确认真正的重新初始化  

## 实际意义

这种两阶段卸载机制确保了：

- **安全性** - 不会意外卸载仍在使用中的模块
- **灵活性** - 支持热更新场景（先加载新版本，再卸载旧版本）
- **一致性** - 引用计数自动管理模块生命周期

这是现代模块系统的标准设计模式，类似于 shared_ptr 的工作原理。

## 运行测试

```bash
cd builddir
meson compile
./examples/module_test_example
```

注意观察控制台输出中的模块加载/卸载消息，这些是验证两阶段卸载机制的关键信息。 