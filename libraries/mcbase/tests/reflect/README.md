/*
* Copyright (c) 2025 Huawei Technologies Co., Ltd.
* openUBMC is licensed under Mulan PSL v2.
* You can use this software according to the terms and conditions of the Mulan PSL v2.
* You may obtain a copy of Mulan PSL v2 at:
*         http://license.coscl.org.cn/MulanPSL2
* THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
* EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
* MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
* See the Mulan PSL v2 for more details.
*/

# reflect 模块测试说明

## 测试目标
- 覆盖 `mc::reflect` 体系的枚举元数据、签名解析与反射工厂等核心能力。
- 验证复杂嵌套结构、命名空间特化、基类信息等反射场景的正确性。
- 提供异常与非法输入用例，确保接口在边界条件下行为稳定。

## 当前测试文件
- `test_enum_metadata.cpp`
  - 通过本地 `color` 枚举与 `enum_metadata` 校验 `get_names`、`get_value` / `try_get_value`、`get_name`、`try_get_name`、`get_values` 快照等接口。
  - 验证 `variant` 解析辅助接口的成功与失败分支。
- `test_signature.cpp`
  - 覆盖签名构造、`is_valid`、`signature::validate`、`signature_iterator` 异常分支。
  - 针对 `a{si}` 容器签名验证 `get_complete_types`、字典键值迭代等完整流程。
- `test_reflect.cpp` 及其他 `test_reflect_*` 文件
  - 聚焦反射工厂注册、静态信息、命名空间与嵌套类型处理。
  - 包含自定义成员信息、基类继承、静态注册等组合场景。
- `test_reflect_errors.cpp`
  - 覆盖 `throw_bad_enum_cast`、`throw_not_enum_type`、`throw_enum_value_not_found`、`throw_enum_not_support_create_object` 等异常辅助函数，确保 `reflect.cpp` 中未命中的分支得到执行。

## 新增/更新要点
- 新增 `enum_metadata` 自建枚举用例，去除对 `MC_REFLECT_ENUM` 宏的依赖，直接验证 `enum_metadata` API。
- 更新签名测试，改为通过 `signature::validate` 与 `signature_iterator` 检查非法签名，确保与实现逻辑一致。
- 强化 `get_complete_types` 期望，明确数组与字典嵌套组合的解析行为。
- **优化测试用例**：将覆盖率测试用例合并到现有测试文件中。
  - `test_reflect.cpp` 中新增：`ToVariantCreatesDict`、`ToVariantDirectToDict`、`FromVariantFromArray`、`FromVariantFromArrayIncomplete`、`FromVariantFromArrayExcess`、`FromVariantInvalidType`、`EnumToVariantToDict`。
  - `test_signature.cpp` 中新增：`first_type_empty_string`、`assign_string_view`、`assign_string`、`from_variant`。
  - 覆盖 `reflect.h` 中 `to_variant` 和 `from_variant` 的所有分支。
  - 覆盖 `signature.h` 中 `first_type` 空字符串分支、`operator=` 的 `string_view` 和 `string` 分支、从 `variant` 构造的分支。

## 运行方式
```bash
meson test -C builddir --suite reflect
```

## 测试统计

- **测试文件总数**: 12 个
- **测试用例总数**: 82 个
  - `test_reflect.cpp`: 17 个用例
  - `test_signature.cpp`: 8 个用例
  - `test_enum_metadata.cpp`: 3 个用例
  - `test_reflect_errors.cpp`: 6 个用例
  - `test_reflection_namespace.cpp`: 12 个用例
  - `test_reflect_static_info.cpp`: 6 个用例
  - `test_reflect_name.cpp`: 3 个用例
  - `test_reflection_factory.cpp`: 12 个用例
  - `test_nested_reflect.cpp`: 6 个用例
  - `test_method_call.cpp`: 5 个用例
  - `test_reflect_base_class.cpp`: 3 个用例
  - `test_custom_member_info.cpp`: 1 个用例

## 测试优化说明

### 测试用例分析

经过分析，reflect 模块的测试用例设计合理，没有发现明显的重复测试用例。所有测试用例都有其特定的测试目的：

1. **基础功能测试**：
   - `test_reflect.cpp` - 测试基本的反射功能，包括类反射、枚举反射、成员访问、部分更新、嵌套对象、变体互操作性、序列化等
   - `test_nested_reflect.cpp` - 测试嵌套反射功能，包括嵌套类反射、嵌套枚举反射、部分嵌套更新、复杂嵌套序列化等

2. **功能覆盖测试**：
   - 每个测试用例都专注于特定的功能点或场景
   - 测试用例之间相互补充，形成完整的测试覆盖

3. **测试用例统计**：
   - 已更新测试用例总数为 82 个（实际统计）
   - 各文件的测试用例数量已更新

## 后续规划
- 根据覆盖率报告，补充 `reflect.cpp` 其余分支（如自定义类型注册失败路径），目前新增错误辅助函数测试已覆盖原先缺失的枚举相关异常路径。
- 为 `signature_helper` 与模板特化路径补充更复杂的组合测试。
