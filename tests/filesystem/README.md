# Filesystem 模块测试

Copyright (c) 2025 Huawei Technologies Co., Ltd.
openUBMC is licensed under Mulan PSL v2.
You can use this software according to the terms and conditions of the Mulan PSL v2.
You may obtain a copy of Mulan PSL v2 at: http://license.coscl.org.cn/MulanPSL2
THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
See the Mulan PSL v2 for more details.

## 概述

本文档描述了 Filesystem 模块的测试用例。Filesystem 模块提供了跨平台的文件系统操作功能，包括路径操作、文件信息查询、目录操作、文件读写、文件操作（创建、删除、重命名等）、路径规范化、错误处理等功能。

## 测试文件列表

Filesystem 模块包含以下测试文件：

1. `test_filesystem.cpp` - 文件系统功能测试

## 详细测试用例

### test_filesystem.cpp

文件系统功能测试（10 个用例），包括：

#### 路径操作测试
- `PathOperations` - 路径基本操作测试
  - 路径构造和规范化
  - 路径拼接和分解
  - 绝对路径和相对路径处理
  - 路径存在性检查

- `PathJoin` - 路径拼接测试
  - 多段路径拼接
  - 路径分隔符处理
  - 空路径处理
  - 特殊字符处理

- `ComplexPathNormalization` - 复杂路径规范化测试
  - 路径规范化
  - 符号链接处理
  - 相对路径解析
  - 路径规范化边界情况

#### 文件信息测试
- `FileInfo` - 文件信息查询测试
  - 文件大小查询
  - 文件类型判断（普通文件、目录、符号链接等）
  - 文件权限查询
  - 文件修改时间查询
  - 文件存在性检查

#### 目录操作测试
- `DirectoryOperations` - 目录操作测试
  - 目录创建
  - 目录删除
  - 目录遍历
  - 目录存在性检查
  - 嵌套目录操作

#### 文件读写测试
- `FileReadWrite` - 文件读写测试
  - 文件写入
  - 文件读取
  - 文件追加
  - 二进制文件读写
  - 大文件读写

#### 文件操作测试
- `FileOperations` - 文件操作测试
  - 文件创建
  - 文件删除
  - 文件重命名
  - 文件复制
  - 文件移动

#### 复杂场景测试
- `ComplexFileOperationWorkflow` - 复杂文件操作工作流测试
  - 多步骤文件操作
  - 文件操作序列
  - 操作依赖关系
  - 操作回滚

- `ComplexFileOperationErrorHandling` - 复杂文件操作错误处理测试
  - 文件不存在错误处理
  - 权限不足错误处理
  - 磁盘空间不足错误处理
  - 并发访问错误处理
  - 无效路径错误处理

#### 系统信息测试
- `SpaceAndCurrentPath` - 磁盘空间和当前路径测试
  - 磁盘空间查询
  - 当前工作目录获取和设置
  - 路径规范化（处理符号链接）
  - 跨平台路径兼容性

## 测试统计

- **测试文件总数**: 1
- **测试用例总数**: 10

## 运行测试

### 运行所有测试

```bash
cd builddir
ninja test
```

### 运行特定测试文件

```bash
# 运行 filesystem 测试
./tests/libmcpp_test --gtest_filter="FilesystemTest.*"
```

### 运行特定测试用例

```bash
# 运行特定测试用例
./tests/libmcpp_test --gtest_filter="FilesystemTest.PathOperations"
./tests/libmcpp_test --gtest_filter="FilesystemTest.FileReadWrite"
./tests/libmcpp_test --gtest_filter="FilesystemTest.ComplexFileOperationErrorHandling"
```

## 测试注意事项

1. **临时文件**: 测试会创建临时文件和目录，所有资源都会在测试完成后自动清理
2. **跨平台兼容性**: 测试使用 `mc::filesystem::temp_directory_path()` 获取临时目录，确保跨平台兼容性
3. **路径规范化**: 在 macOS 等系统上，路径可能包含符号链接（如 `/var` 指向 `/private/var`），测试使用 `mc::filesystem::fs::canonical()` 进行路径规范化
4. **权限测试**: 某些测试可能需要特定权限，如果权限不足，相关测试可能会失败
5. **并发访问**: 文件操作测试会验证并发访问的安全性，确保不会出现竞态条件
6. **资源清理**: 所有测试都会在 `TearDown()` 中清理创建的临时文件和目录，确保不会留下测试残留

