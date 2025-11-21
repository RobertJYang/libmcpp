# Core 模块测试

Copyright (c) 2025 Huawei Technologies Co., Ltd.
openUBMC is licensed under Mulan PSL v2.
You can use this software according to the terms and conditions of the Mulan PSL v2.
You may obtain a copy of Mulan PSL v2 at: http://license.coscl.org.cn/MulanPSL2
THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
See the Mulan PSL v2 for more details.

## 概述

本文档描述了 Core 模块的测试用例。Core 模块是 MC++ 库的核心基础模块，提供了应用程序生命周期管理、服务管理、监督器模式、配置管理、插件管理、对象层次结构、信号槽机制、依赖排序、定时器等核心功能。

## 测试文件列表

Core 模块包含以下测试文件：

1. `test_application.cpp` - 应用程序生命周期管理测试
2. `test_config_manager.cpp` - 配置管理器测试
3. `test_config_validator.cpp` - 配置验证器测试
4. `test_connection_manager.cpp` - 连接管理器测试
5. `test_default_supervisor.cpp` - 默认监督器测试
6. `test_dependency_sorter.cpp` - 依赖排序器测试
7. `test_plugin_manager.cpp` - 插件管理器测试
8. `test_service.cpp` - 基础服务基类测试
9. `test_service_factory.cpp` - 服务工厂测试
10. `test_service_manager.cpp` - 服务管理器测试
11. `test_singleton.cpp` - 单例模式测试
12. `test_thread_safe_object.cpp` - 线程安全对象测试
13. `test_timer.cpp` - 定时器测试

## 详细测试用例

### test_application.cpp
应用程序生命周期管理测试（20 个用例）：
- `instance` - 测试单例获取
- `version` - 测试版本设置和获取
- `get_config_manager` - 测试配置管理器获取
- `get_plugin_manager` - 测试插件管理器获取
- `get_service_factory` - 测试服务工厂获取
- `get_service_manager` - 测试服务管理器获取
- `get_supervisor_manager` - 测试监督器管理器获取
- `initialize_success` - 测试初始化成功场景
- `initialize_basic` - 测试基础初始化
- `initialize_with_args_success` - 测试带参数的初始化
- `initialize_parse_command_line_fails` - 测试命令行解析失败
- `initialize_load_config_fails` - 测试配置文件加载失败
- `start_basic` - 测试基础启动
- `start_when_already_running` - 测试重复启动
- `stop_basic` - 测试基础停止
- `stop_when_not_running` - 测试未运行时停止
- `stop_when_already_stopped` - 测试重复停止
- `is_stopped` - 测试停止状态检查
- `lifecycle` - 测试完整生命周期
- `initialize_both_versions` - 测试两种初始化方式

### test_config_manager.cpp
配置管理器测试（33 个用例）：
- `constructor` - 测试构造函数
- `parse_command_line_basic` - 测试基础命令行解析
- `parse_command_line_config_file` - 测试配置文件参数解析
- `parse_command_line_plugin_dir` - 测试插件目录参数解析
- `parse_command_line_threads` - 测试线程数参数解析
- `parse_command_line_invalid_threads` - 测试无效线程数参数
- `parse_command_line_plugins` - 测试插件列表参数解析
- `add_config_application` - 测试添加 Application 配置
- `add_config_service` - 测试添加 Service 配置
- `add_config_supervisor` - 测试添加 Supervisor 配置
- `add_config_plugin` - 测试添加 Plugin 配置
- `get_config_not_found` - 测试获取不存在的配置
- `get_configs` - 测试获取多个配置
- `load_config_file_json` - 测试加载 JSON 配置文件
- `load_config_file_array` - 测试加载数组格式配置
- `load_config_file_not_found` - 测试配置文件不存在
- `load_config_file_invalid_json` - 测试无效 JSON 文件
- `load_config_file_invalid_root_type` - 测试无效根节点类型
- `load_config_file_mixed_entries` - 测试混合条目配置
- `load_config_file_logging_section` - 测试日志配置段
- `load_config_file_toml_not_supported` - 测试 TOML 不支持
- `add_config_invalid` - 测试添加无效配置
- `add_config_non_object_variant` - 测试非对象 variant 配置
- `add_config_unknown_kind` - 测试未知类型配置
- `get_plugin_dir` - 测试获取插件目录
- `get_thread_count` - 测试获取线程数
- `get_plugin_names_empty` - 测试获取空插件列表
- `json_config_loader` - 测试 JSON 配置加载器
- `toml_config_loader` - 测试 TOML 配置加载器
- `json_config_loader_file_not_found` - 测试文件不存在场景
- `json_config_loader_file_open_fails` - 测试文件打开失败
- `json_config_loader_parse_error` - 测试解析错误
- `json_config_loader_valid_json` - 测试有效 JSON 加载

### test_config_validator.cpp
配置验证器测试（20 个用例）：
- `validate_app_config_success` - 测试 Application 配置验证成功
- `validate_app_config_missing_kind` - 测试缺少 kind 字段
- `validate_app_config_missing_api_version` - 测试缺少 api_version 字段
- `validate_app_config_missing_name` - 测试缺少 name 字段
- `validate_app_config_missing_plugin_dir` - 测试缺少 plugin_dir 字段
- `validate_supervisor_config_success` - 测试 Supervisor 配置验证成功
- `validate_supervisor_config_missing_kind` - 测试缺少 kind 字段
- `validate_supervisor_config_missing_api_version` - 测试缺少 api_version 字段
- `validate_supervisor_config_missing_name` - 测试缺少 name 字段
- `validate_supervisor_config_negative_max_restarts` - 测试负数最大重启次数
- `validate_service_config_success` - 测试 Service 配置验证成功
- `validate_service_config_missing_kind` - 测试缺少 kind 字段
- `validate_service_config_missing_api_version` - 测试缺少 api_version 字段
- `validate_service_config_missing_name` - 测试缺少 name 字段
- `validate_service_config_missing_type` - 测试缺少 type 字段
- `validate_plugin_config_success` - 测试 Plugin 配置验证成功
- `validate_plugin_config_missing_kind` - 测试缺少 kind 字段
- `validate_plugin_config_missing_api_version` - 测试缺少 api_version 字段
- `validate_plugin_config_missing_name` - 测试缺少 name 字段
- `validate_plugin_config_missing_version` - 测试缺少 version 字段

### test_connection_manager.cpp
连接管理器测试（10 个用例）：
- `add_connection` - 测试添加连接
- `add_multiple_connections` - 测试添加多个连接
- `add_connection_with_id` - 测试使用指定 ID 添加连接
- `add_connection_duplicate_id` - 测试重复 ID 处理
- `remove_connection` - 测试移除连接
- `remove_nonexistent_connection` - 测试移除不存在的连接（合并了原 remove_nonexistent_connection_id）
- `remove_connections` - 测试批量移除连接
- `remove_nonexistent_signal_connections` - 测试移除不存在的信号连接
- `clear` - 测试清空所有连接
- `auto_generated_ids_unique` - 测试自动生成唯一 ID
- `multiple_signals` - 测试多个信号管理

### test_default_supervisor.cpp
默认监督器测试（约 54 个用例，包含约 50 个 default_supervisor_test 和 4 个 SupervisorManagerTest）：
- `constructor` - 测试构造函数
- `init` - 测试初始化
- `add_service` - 测试添加服务
- `add_null_service` - 测试添加空服务
- `add_duplicate_service` - 测试添加重复服务
- `remove_service` - 测试移除服务
- `remove_nonexistent_service` - 测试移除不存在的服务
- `get_service` - 测试获取服务
- `start` - 测试启动
- `start_empty` - 测试空服务列表启动
- `stop` - 测试停止
- `cleanup` - 测试清理
- `is_healthy` - 测试健康检查
- `add_child` - 测试添加子对象
- `add_null_child` - 测试添加空子对象
- `remove_child` - 测试移除子对象
- `get_child` - 测试获取子对象
- `start_with_dependencies` - 测试带依赖启动
- `get_config` - 测试获取配置
- `get_name` - 测试获取名称
- `restart_all_services` - 测试重启所有服务
- `get_service_stop_order` - 测试获取服务停止顺序
- `get_strategy_name_one_for_one` - 测试 one_for_one 策略名称
- `get_strategy_name_one_for_all` - 测试 one_for_all 策略名称
- `get_strategy_name_rest_for_one` - 测试 rest_for_one 策略名称
- `get_strategy_name_unknown` - 测试未知策略名称
- `handle_service_crash_service_not_found` - 测试服务未找到时的崩溃处理
- `handle_service_crash_exceeds_max_restarts` - 测试超过最大重启次数
- `handle_service_crash_unlimited_restarts` - 测试无限制重启
- `handle_service_crash_one_for_all_strategy` - 测试 one_for_all 策略崩溃处理
- `handle_service_crash_rest_for_one_strategy` - 测试 rest_for_one 策略崩溃处理
- `StartOneService` - 测试启动单个服务（覆盖所有分支）
- `StartOneServiceException` - 测试启动服务时的异常处理
- `StopOneService` - 测试停止单个服务（覆盖所有分支）
- `StopOneServiceException` - 测试停止服务时的异常处理
- `StopOneServiceFailure` - 测试停止服务失败的情况
- `RestartOneService` - 测试重启单个服务（覆盖所有分支）
- `RestartOneServiceStopFailure` - 测试重启服务时 stop 失败的情况（已删除重复的 restart_service_stop_fails_rolls_back）
- `RestartOneServiceStartFailure` - 测试重启服务时 start 失败的情况
- `RestartAllServicesWithCountReset` - 测试重启所有服务（包括重启计数重置）
- `RestartAllServicesExceedsMaxRestarts` - 测试超过最大重启次数
- `RestartAllServicesStartFailure` - 测试重启所有服务时启动失败的情况
- `RestartDependentServices` - 测试重启依赖服务（覆盖所有分支）
- `RestartDependentServicesFailure` - 测试重启依赖服务失败的情况
- `StopOneChildSupervisor` - 测试停止单个子监督器（覆盖所有分支）
- `StopOneChildSupervisorFailure` - 测试停止子监督器失败的情况
- `StopOneChildSupervisorException` - 测试停止子监督器时的异常处理
- `StartChildSupervisorFailure` - 测试启动时子监督器失败的分支
- `AddChildDuplicate` - 测试重复添加子监督器
- `ComplexScenarioAllUncoveredFeatures` - 测试复杂场景，融合所有未覆盖的功能

**SupervisorManagerTest（4 个）**：
- `ManageSupervisorsLifecycle` - 测试监督器管理器的生命周期管理（创建、添加、启动、停止）
- `StartFailureIsHandled` - 测试启动失败处理
- `StopFailureIsHandled` - 测试停止失败处理
- `InitializeFromConfigsAddsSupervisors` - 测试从配置初始化并添加监督器

### test_dependency_sorter.cpp
依赖排序器测试（6 个用例）：
- `build_dependency_graph_simple` - 测试构建简单依赖图
- `sort_for_startup_simple_chain` - 测试简单链式依赖启动排序
- `sort_for_startup_circular_dependency` - 测试循环依赖启动排序
- `sort_for_shutdown_simple_chain` - 测试简单链式依赖停止排序
- `has_circular_dependency` - 测试循环依赖检测
- `has_no_circular_dependency` - 测试无循环依赖检测

### test_service.cpp
服务基类测试（3 个用例）：
- `default_initialization` - 验证默认构造名称、状态、依赖和配置引用
- `constructor_and_mutators` - 覆盖 `set_name`、`set_state`、`set_dependencies`
- `default_behavior_methods` - 调用基础实现的生命周期钩子与回调

### test_service_factory.cpp
服务工厂测试（3 个用例）：
- `register_and_create_service_success` - 注册成功的服务类型，验证 `create_service`、`has_service` 与 `get_service_types`
- `register_service_registers_options_when_available` - 覆盖检测 `register_options` 的分支，验证 CLI/配置选项注册
- `create_service_handles_missing_or_failed_services` - 触发未注册及初始化失败时的空指针返回路径

### test_plugin_manager.cpp
插件管理器测试（22 个用例）：
- `constructor_destructor` - 测试构造和析构
- `set_get_plugin_dir` - 测试设置和获取插件目录
- `register_plugin` - 测试注册插件
- `register_null_plugin` - 测试注册空插件
- `register_duplicate_plugin` - 测试注册重复插件
- `find_plugin` - 测试查找插件
- `find_nonexistent_plugin` - 测试查找不存在的插件
- `get_loaded_plugins` - 测试获取已加载插件列表
- `unload_plugin` - 测试卸载插件
- `unload_nonexistent_plugin` - 测试卸载不存在的插件
- `unload_all_plugins` - 测试卸载所有插件
- `init_plugins` - 测试初始化插件
- `load_plugins_empty_list` - 测试加载空插件列表
- `multiple_plugins` - 测试多个插件管理
- `load_plugin_missing_library` - 测试加载不存在的库
- `load_plugin_stub_shared_object` - 测试加载存根共享对象
- `load_plugin_without_factory_symbols` - 测试加载没有工厂符号的库
- `load_plugin_already_loaded` - 测试加载已加载的插件（覆盖 "already loaded" 分支）
- `load_plugins_with_names` - 测试指定插件名称列表（覆盖 plugin_names 非空分支）
- `load_plugins_from_directory` - 测试从目录加载所有插件（覆盖目录遍历分支）
- `load_plugins_directory_error` - 测试目录遍历异常处理（覆盖 filesystem_error 分支）
- `load_plugins_register_failure` - 测试插件注册失败的情况

### test_service_manager.cpp
服务管理器测试（18 个用例）：
- `add_and_get_service` - 测试添加和获取服务
- `remove_service_stops_and_cleans_up` - 测试移除服务并清理
- `initialize_from_configs_builds_services` - 测试从配置构建服务
- `initialize_from_configs_detects_cycle` - 测试循环依赖检测
- `start_services_handles_failure` - 测试启动失败处理
- `no_dependencies` - 测试无依赖场景
- `simple_dependency` - 测试简单依赖
- `multiple_dependencies` - 测试多个依赖
- `multiple_dependency_chains` - 测试多个依赖链
- `circular_dependency` - 测试循环依赖
- `add_remove_services` - 测试添加和移除服务
- `start_stop_services` - 测试启动和停止服务
- `service_start_failure` - 测试服务启动失败
- `service_stop_failure` - 测试服务停止失败
- `service_cleanup_exception` - 测试服务清理异常
- `empty_service_list` - 测试空服务列表
- `service_manager_cleanup` - 测试服务管理器清理
- `complex_dependency_graph` - 测试复杂依赖图

### test_singleton.cpp
单例模式测试（14 个用例）：
- `LifecycleAndTryGet` - 覆盖基础生命周期、try_get、reset 与重建
- `TaggedSingletonIsolation` - 验证不同标签的单例隔离与重置行为
- `CustomCreatorScenarios` - 覆盖复杂初始化与普通自定义创建器
- `NonLeakySingleton` - 测试非泄露单例销毁
- `LeakySingleton` - 测试泄露单例释放路径
- `ComplexConcurrentSingletonAccess` - 测试并发访问与增量一致性
- `DestroyInstancesRunsDestructors` - 验证 singleton_manager::destroy_instances 行为
- `LeakySingletonNotDestroyedByManager` - 验证泄露单例不受 destroy_instances 影响
- `DestroyInstancesInReverseOrder` - 测试按相反顺序销毁实例
- `ManagerResetClearsAllStates` - 测试管理器重置清除所有状态
- `CreatedFlagReflectsLifecycle` - 测试创建标志反映生命周期
- `InstanceIgnoresSubsequentArguments` - 测试实例忽略后续参数
- `LeakySingletonSurvivesDestroyInstances` - 测试泄露单例在销毁实例后仍然存活
- `DestroyInstancesIsIdempotent` - 测试销毁实例是幂等的

### test_thread_safe_object.cpp
线程安全对象测试（21 个用例）：
- `BasicParentChildRelationship` - 测试基础父子关系
- `ConcurrentEnsureImplAccess` - 测试并发确保实现访问
- `ConcurrentPropertyAccess` - 测试并发属性访问
- `ExecutorAssignment` - 测试执行器分配
- `ConnectionManagementHelpers` - 测试连接管理辅助功能（连接、断开、信号断开）
- `ConcurrentAddChildren` - 测试并发添加子对象
- `ConcurrentParentChildOperations` - 测试并发父子操作
- `ConcurrentDestruction` - 测试并发销毁
- `ParentDestructionHandling` - 测试父对象销毁处理
- `ConcurrentNameOperations` - 测试并发名称操作
- `ConcurrentSetParent` - 测试并发设置父对象
- `ConcurrentDestructionAccess` - 测试并发销毁访问
- `ObjectCopyConstructor` - 测试 object 的复制构造函数
- `ObjectCopyAssignment` - 测试 object 的复制赋值运算符
- `ObjectBaseMoveOperations` - 测试 object_base 的移动操作
- `DisconnectAll` - 测试 disconnect_all 方法
- `WeakFromThis` - 测试 weak_from_this 方法
- `WeakFromThisConst` - 测试 const 版本的 weak_from_this
- `ObjectImplCopyOperations` - 测试 object_impl 的复制操作
- `ClearConnections` - 测试 clear_connections 方法
- `ComplexScenarioAllUncoveredFeatures` - 测试复杂场景，融合所有未覆盖的功能

### test_timer.cpp
定时器测试（12 个用例）：
- `test_single_shot` - 测试单次定时器
- `test_periodic_timer` - 测试周期性定时器
- `test_cancel_timer` - 测试取消定时器
- `test_multiple_timers` - 测试多个定时器
- `test_nested_timers` - 测试嵌套定时器
- `test_timer_interval` - 测试定时器间隔
- `test_timer_status` - 测试定时器状态
- `test_timer_restart` - 测试定时器重启
- `test_timer_interval_change` - 测试定时器间隔变更
- `test_null_callback` - 测试空回调处理
- `test_zero_interval_timer` - 测试零间隔定时器
- `test_timer_destruction` - 测试定时器销毁

## 运行测试

### 运行所有 core 模块测试

```bash
# 运行所有模块测试
meson test -C builddir

# 或者使用 ninja
ninja -C builddir test
```

**注意**: 
- meson 构建配置中只有一个测试注册（`libmcpp unit tests`），不能使用 `--suite core` 运行特定模块测试
- 要通过 meson 传递参数给测试可执行文件，使用 `--test-args` 选项
- 推荐直接运行可执行文件，更简单直接

### 运行特定测试文件

```bash
# 测试应用程序（所有用例）- 直接运行可执行文件（推荐）
./builddir/tests/libmcpp_test --gtest_filter="application_test.*"

# 或通过 meson 传递参数
meson test -C builddir --test-args="--gtest_filter=application_test.*"

# 测试配置管理器（所有用例）
./builddir/tests/libmcpp_test --gtest_filter="config_manager_test.*"
meson test -C builddir --test-args="--gtest_filter=config_manager_test.*"

# 测试配置验证器（所有用例）
./builddir/tests/libmcpp_test --gtest_filter="config_validator_test.*"
meson test -C builddir --test-args="--gtest_filter=config_validator_test.*"

# 测试连接管理器（所有用例）
./builddir/tests/libmcpp_test --gtest_filter="connection_manager_test.*"
meson test -C builddir --test-args="--gtest_filter=connection_manager_test.*"

# 测试默认监督器（所有用例）
./builddir/tests/libmcpp_test --gtest_filter="default_supervisor_test.*:SupervisorManagerTest.*"
meson test -C builddir --test-args="--gtest_filter=default_supervisor_test.*:SupervisorManagerTest.*"

# 测试依赖排序器（所有用例）
./builddir/tests/libmcpp_test --gtest_filter="dependency_sorter_test.*"
meson test -C builddir --test-args="--gtest_filter=dependency_sorter_test.*"

# 测试插件管理器（所有用例）
./builddir/tests/libmcpp_test --gtest_filter="plugin_manager_test.*"
meson test -C builddir --test-args="--gtest_filter=plugin_manager_test.*"

# 测试服务管理器（所有用例）
./builddir/tests/libmcpp_test --gtest_filter="service_manager_test.*"
meson test -C builddir --test-args="--gtest_filter=service_manager_test.*"

# 测试单例（所有用例）
./builddir/tests/libmcpp_test --gtest_filter="singleton_test.*"
meson test -C builddir --test-args="--gtest_filter=singleton_test.*"

# 测试线程安全对象（所有用例）
./builddir/tests/libmcpp_test --gtest_filter="ThreadSafeObjectTest.*"
meson test -C builddir --test-args="--gtest_filter=ThreadSafeObjectTest.*"

# 测试定时器（所有用例）
./builddir/tests/libmcpp_test --gtest_filter="timer_test.*"
meson test -C builddir --test-args="--gtest_filter=timer_test.*"
```

### 运行特定测试用例

```bash
# 使用 gtest 过滤器运行特定用例（直接运行可执行文件，推荐）
./builddir/tests/libmcpp_test --gtest_filter="application_test.get_config_manager"

# 或通过 meson 传递参数
meson test -C builddir --test-args="--gtest_filter=application_test.get_config_manager"

# 运行多个测试用例（使用通配符）
./builddir/tests/libmcpp_test --gtest_filter="config_manager_test.parse_command_line_*"
meson test -C builddir --test-args="--gtest_filter=config_manager_test.parse_command_line_*"
```

## 测试统计

- **测试文件总数**: 13 个
- **测试用例总数**: 约 248+ 个
  - `test_application.cpp`: 20 个
  - `test_config_manager.cpp`: 33 个
  - `test_config_validator.cpp`: 20 个
  - `test_connection_manager.cpp`: 10 个（已删除重复的 remove_nonexistent_connection_id）
  - `test_default_supervisor.cpp`: 约 54 个（约 50 个 default_supervisor_test + 4 个 SupervisorManagerTest，已删除重复的 restart_service_stop_fails_rolls_back）
  - `test_dependency_sorter.cpp`: 6 个
  - `test_plugin_manager.cpp`: 22 个
  - `test_service.cpp`: 3 个
  - `test_service_factory.cpp`: 3 个
  - `test_service_manager.cpp`: 18 个
  - `test_singleton.cpp`: 14 个
  - `test_thread_safe_object.cpp`: 21 个
  - `test_timer.cpp`: 12 个
- **测试覆盖的功能模块**:
  - Application（应用程序）
  - Config Manager（配置管理）
  - Config Validator（配置验证）
  - Connection Manager（连接管理）
  - Default Supervisor（默认监督器）
  - Dependency Sorter（依赖排序）
  - Plugin Manager（插件管理）
  - Service Manager（服务管理）
  - Singleton（单例模式）
  - Thread Safe Object（线程安全对象）
  - Timer（定时器）

## 代码覆盖率

当前测试覆盖率：
- **Line Coverage**: 约 63.8%
- **Branch Coverage**: 约 25.3%
- **Function Coverage**: 持续改进中

### 调试测试

```bash
# 使用 gdb 调试测试
gdb --args ./builddir/tests/libmcpp_test --gtest_filter="application_test.initialize_success"

# 启用详细输出（直接运行可执行文件）
./builddir/tests/libmcpp_test --gtest_filter="application_test.*" --verbose

# 或通过 meson 启用详细输出
meson test -C builddir --verbose --test-args="--gtest_filter=application_test.*"
```

## 测试注意事项

1. **meson test 命令格式**: meson 构建配置中只有一个测试注册（`libmcpp unit tests`），要传递参数给测试可执行文件，使用 `--test-args` 选项。格式：`meson test -C builddir --test-args="--gtest_filter=..."`。注意不要在 `--test-args` 中使用引号包裹整个参数，gtest_filter 的值可以用引号。
2. **直接运行可执行文件**: 推荐直接运行 `./builddir/tests/libmcpp_test`，这样更简单直接，不需要通过 meson。
3. **并发测试**: `test_thread_safe_object.cpp` 包含并发安全性测试，需要多线程环境支持。
4. **配置文件测试**: `test_config_manager.cpp` 需要创建临时配置文件，测试会自动清理。
5. **服务基础能力与依赖测试**: `test_service.cpp` 验证服务基类默认实现，`test_service_factory.cpp` 覆盖服务注册与创建，`test_service_manager.cpp` 与 `test_dependency_sorter.cpp` 覆盖服务依赖与管理逻辑。
6. **监督器策略测试**: `test_default_supervisor.cpp` 测试多种故障恢复策略。
7. **单例测试**: `test_singleton.cpp` 使用 `reset_for_test()` 方法在测试之间重置单例状态。
8. **测试隔离**: `test_application.cpp` 采用条件清理策略，只在测试确实启动了应用时才清理，避免影响其他模块（如 engine 模块、dbus 模块）的测试。这确保了测试之间的独立性。
9. **DBus 环境**: core 模块的测试不会直接操作 DBus，但会确保共享内存锁文件（`/dev/shm/init_shm.lock`）存在，以支持需要 DBus 的其他测试正常运行。注意：`/dev/shm` 路径仅在 Linux 系统上使用（通过 `#ifdef __linux__` 条件编译）。
10. **跨平台路径**: 所有测试用例使用 `mc::filesystem::temp_directory_path()` 获取临时目录，避免硬编码 `/tmp` 等路径，确保测试在不同操作系统上都能正常运行。
11. **连接管理**: `test_thread_safe_object.cpp` 中的 `ConnectionManagementHelpers` 测试验证了信号连接和断开功能，包括显式断开连接和信号断开所有连接。
12. **监督器管理**: `test_default_supervisor.cpp` 中的 `SupervisorManagerTest` 测试覆盖了监督器管理器的完整生命周期，包括创建、启动、停止和失败处理。

