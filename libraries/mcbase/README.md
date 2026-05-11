<!--
* Copyright (c) 2026 Huawei Technologies Co., Ltd.
* openUBMC is licensed under Mulan PSL v2.
* You can use this software according to the terms and conditions of the Mulan PSL v2.
* You may obtain a copy of Mulan PSL v2 at:
* http://license.coscl.org.cn/MulanPSL2
* THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
* EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
* MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
* See the Mulan PSL v2 for more details.
-->

# mcbase

## Sanitizer 调试构建

`mcbase` 支持通过 Meson 选项启用 AddressSanitizer（ASan）和 ThreadSanitizer（TSan），用于持续做内存错误和并发问题排查。

### 直接构建 mcbase

```bash
meson setup builddir-mcbase-asan libraries/mcbase -Dsanitizer=address
meson compile -C builddir-mcbase-asan ./tests/mcbase_test:executable
./builddir-mcbase-asan/tests/mcbase_test --gtest_filter=FixedSizePoolTest.RepeatedJoinThenDestroyAcrossThreads
```

### 从顶层工程转发到 mcbase 子项目

```bash
meson setup builddir-asan . -Dmcbase:sanitizer=address
meson compile -C builddir-asan ./libraries/mcbase/tests/mcbase_test:executable
./builddir-asan/libraries/mcbase/tests/mcbase_test --gtest_filter=FixedSizePoolTest.RepeatedJoinThenDestroyAcrossThreads
```

### ThreadSanitizer

```bash
meson setup builddir-tsan . -Dmcbase:sanitizer=thread
meson compile -C builddir-tsan ./libraries/mcbase/tests/mcbase_test:executable
./builddir-tsan/libraries/mcbase/tests/mcbase_test --gtest_filter=FixedSizePoolTest.RepeatedJoinThenDestroyAcrossThreads
```

### 注意事项

- `sanitizer=address` 启用 ASan。
- `sanitizer=thread` 启用 TSan。
- `mcbase` 的 `sanitizer` 与 `enable_coverage` 不能同时启用，Meson 会在配置阶段直接报错。
- sanitizer 构建会同时作用于 `mcbase` 库和 `mcbase_test`，避免库和测试插桩不一致。
