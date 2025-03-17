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

# 配置系统设计

## 1. 概述

配置系统采用基于 Kubernetes 风格的声明式配置机制，通过 JSON 格式的配置文件来定义应用程序的各种资源。这种设计具有以下特点：

- 声明式：用户只需声明期望的状态，系统负责实现和维护
- 结构化：配置项按照预定义的模式组织，便于验证和处理
- 可扩展：支持多种资源类型，易于添加新的配置类型
- 统一管理：所有配置集中在一个文件中管理

## 2. 配置结构

### 2.1 基础结构

所有配置项都继承自 `resource_base` 结构，包含以下基础字段：

```cpp
struct resource_base {
    std::string api_version;             // API版本
    std::string kind;                    // 资源类型
    metadata meta;                       // 元数据
};
```

其中 `metadata` 结构包含：

```cpp
struct metadata {
    std::string name;                    // 资源名称
    std::optional<dict> labels;          // 标签集合
    std::optional<dict> annotations;     // 注解集合
};
```

### 2.2 资源类型

系统支持以下资源类型：

#### 2.2.1 应用程序配置 (Application)

```cpp
struct app_config : public resource_base {
    std::string plugin_dir;              // 插件目录
    std::vector<std::string> plugins;    // 插件列表
    unsigned int threads;                // 线程数量
};
```

#### 2.2.2 监督器配置 (Supervisor)

```cpp
struct supervisor_config : public resource_base {
    supervisor_strategy strategy;        // 监督策略
    int max_restarts;                    // 最大重启次数
    std::vector<std::string> services;   // 服务列表
};
```

监督策略包括：
- `one_for_one`：只重启失败的服务
- `one_for_all`：重启所有服务
- `rest_for_one`：重启失败服务及其后定义的服务

#### 2.2.3 服务配置 (Service)

```cpp
struct service_config : public resource_base {
    std::string type;                    // 服务类型
    std::vector<std::string> dependencies; // 依赖的服务
    dict properties;                     // 服务属性
};
```

#### 2.2.4 插件配置 (Plugin)

```cpp
struct plugin_config : public resource_base {
    std::string version;                 // 插件版本
    dict properties;                     // 插件属性
};
```

## 3. 配置验证

系统为每种资源类型提供了专门的验证器，确保配置的正确性：

```cpp
class config_validator {
public:
    static bool validate_app_config(const app_config& config);
    static bool validate_supervisor_config(const supervisor_config& config);
    static bool validate_service_config(const service_config& config);
    static bool validate_plugin_config(const plugin_config& config);
};
```

验证规则包括：
- 基本字段验证：检查必要字段（kind、api_version、meta.name）是否存在
- 类型特定验证：根据资源类型验证特定字段
- 依赖关系验证：检查服务之间的依赖关系是否有效 

## 4. 配置示例

### 4.1 应用配置示例

```json
{
    "kind": "app_config",
    "api_version": "v1",
    "meta": {
        "name": "my_app"
    },
    "spec": {
        "plugin_dir": "/usr/local/lib/mc/plugins",
        "plugins": ["plugin1", "plugin2"],
        "thread_count": 4
    }
}
```

### 4.2 监督者配置示例

```json
{
    "kind": "supervisor_config",
    "api_version": "v1",
    "meta": {
        "name": "main_supervisor"
    },
    "spec": {
        "strategy": "one_for_one",
        "max_restart": 3,
        "services": [
            {
                "name": "service1",
                "type": "plugin",
                "deps": [],
                "props": {
                    "plugin_name": "plugin1"
                }
            },
            {
                "name": "service2",
                "type": "plugin",
                "deps": ["service1"],
                "props": {
                    "plugin_name": "plugin2"
                }
            }
        ]
    }
}
``` 

## 5. 配置加载过程

配置加载过程包括以下步骤：

1. 解析配置
   - 读取配置文件
   - 解析 JSON 格式
   - 转换为内部数据结构

2. 验证配置
   - 验证基本字段
   - 验证资源类型特定字段
   - 验证依赖关系

3. 应用配置
   - 创建资源实例
   - 建立资源关系
   - 初始化资源状态

4. 错误处理
   - 配置解析错误
   - 验证失败错误
   - 资源创建错误

配置加载失败时，系统会：
- 记录详细错误信息
- 回滚已创建的资源
- 保持系统处于安全状态 

## 6. 扩展机制

系统提供了灵活的扩展机制，支持添加新的资源类型和验证规则：

1. 添加新资源类型
   ```cpp
   struct custom_resource {
       std::string kind;
       std::string api_version;
       meta_data meta;
       custom_spec spec;
   };
   ```

2. 注册验证器
   ```cpp
   class custom_validator {
   public:
       static bool validate(const custom_resource& config);
   };
   ```

3. 注册资源处理器
   ```cpp
   class custom_handler {
   public:
       bool create(const custom_resource& config);
       bool update(const custom_resource& config);
       bool remove(const custom_resource& config);
   };
   ```

扩展时需要注意：
- 遵循现有的配置结构
- 实现必要的验证规则
- 提供完整的错误处理
- 保持向后兼容性 

## 7. 最佳实践

### 7.1 配置组织

1. 配置文件结构
   - 按功能模块组织配置文件
   - 使用有意义的文件名
   - 保持配置文件简洁

2. 配置项命名
   - 使用小写字母
   - 使用下划线分隔
   - 使用有意义的名称

3. 配置项分组
   - 相关配置项放在一起
   - 使用注释说明分组
   - 保持逻辑顺序

### 7.2 配置管理

1. 版本控制
   - 使用语义化版本
   - 记录配置变更历史
   - 保持向后兼容

2. 环境配置
   - 区分开发/测试/生产环境
   - 使用环境变量
   - 避免硬编码

3. 安全配置
   - 敏感信息加密
   - 权限控制
   - 审计日志

### 7.3 错误处理

1. 验证错误
   - 提供清晰的错误信息
   - 指出具体问题位置
   - 给出修复建议

2. 运行时错误
   - 优雅降级
   - 自动恢复
   - 错误通知

3. 监控告警
   - 配置变更监控
   - 异常情况告警
   - 性能指标监控 