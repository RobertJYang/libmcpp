# 日志模块迁移计划

## 目录结构

日志模块将重构为以下子模块结构：

```
include/mc/
├── log.h                   # 总入口头文件（包含所有宏定义）
└── log/
    ├── core/               # 核心组件
    │   ├── log.h           # 核心入口头文件
    │   ├── log_level.h     # 日志级别
    │   ├── log_manager.h   # 日志管理器
    │   └── logger.h        # 日志记录器
    ├── message/            # 消息处理
    │   └── log_message.h   # 日志消息
    ├── appenders/          # 日志追加器
    │   ├── appender.h      # 追加器基类
    │   ├── console_appender.h # 控制台追加器
    │   └── file_appender.h # 文件追加器
    └── backends/           # 日志后端
        ├── log_backend.h   # 后端基类
        ├── log_backend_adapter.h # 后端适配器
        ├── log_backend_loader.h  # 后端加载器
        ├── file_log_backend.h    # 文件日志后端
        └── elk_log_backend.h     # ELK日志后端

src/log/
├── core/                   # 核心源文件
│   └── logger.cpp          # 日志记录器实现
└── message/                # 消息源文件
    └── log_message.cpp     # 日志消息实现
```

**注意：** 所有日志宏定义现在都统一放在 `include/mc/log.h` 中，`log_macros.h` 文件已删除。

## 文件修改计划

### 1. 头文件保护宏更新

所有头文件需要更新头文件保护宏，采用以下命名模式：

- core目录：`MC_LOG_CORE_XXX_H`
- message目录：`MC_LOG_MESSAGE_XXX_H`
- appenders目录：`MC_LOG_APPENDERS_XXX_H`
- backends目录：`MC_LOG_BACKENDS_XXX_H`

### 2. 包含路径更新

需要更新所有文件中的包含路径，例如：

- `#include <mc/log/log_level.h>` → `#include <mc/log/core/log_level.h>`
- `#include <mc/log/log_message.h>` → `#include <mc/log/message/log_message.h>`
- `#include <mc/log/appender.h>` → `#include <mc/log/appenders/appender.h>`
- `#include <mc/log/log_backend.h>` → `#include <mc/log/backends/log_backend.h>`

### 3. 移除 log_macros.h

- 所有宏定义已移至 `include/mc/log.h`
- 删除 `include/mc/log/core/log_macros.h` 文件
- 更新 `include/mc/log/core/log.h` 不再包含 `log_macros.h`
- 任何包含 `log_macros.h` 的文件应直接包含 `log.h` 或更新为使用新的宏

### 4. 测试代码更新

所有使用日志模块的测试代码也需要更新包含路径，主要是：

- `tests/log/test_log.cpp`

### 5. 构建文件更新

需要更新 `meson.build` 文件，确保新的目录结构被正确编译和链接。

## 兼容性注意事项

为了保持向后兼容性，我们将保留：

1. 总入口文件 `include/mc/log.h`，它包含了所有日志功能
2. 命名空间保持不变，仍然是 `mc::log`
3. 所有原有的日志宏都保持不变，包括：
   - `MC_LOG_XXX` 系列宏（基础宏）
   - `mc_xlog` 系列宏（带logger参数）
   - `MC_XXX` 系列宏（全局宏）
   - `xlog` 系列宏（使用默认logger）

## 迁移后的优势

1. 更清晰的模块划分，使代码更易于理解和维护
2. 更好的关注点分离，各子模块职责单一
3. 降低了模块之间的耦合度
4. 便于未来扩展新的日志追加器和后端
5. 统一的宏定义入口，减少用户混淆 