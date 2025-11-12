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

# IO 模块测试

## 概述

`tests/io` 覆盖 `src/io` 目录下的核心组件：`io_buffer` 与 `io_stream`。测试聚焦缓冲区管理、链式操作、读写位置控制、字节序与对齐等场景，确保自定义 I/O 基础设施在复杂场景下行为稳定。

## 文件结构

- `test_io_buffer.cpp`：针对 `mc::io::io_buffer` 的单元测试。
- `test_io_stream.cpp`：针对 `mc::io::io_stream` 的单元测试。
- `meson.build`：将上述测试编译并加入统一测试入口。

## 覆盖重点

### test_io_buffer.cpp
- **读写接口**：`write`, `read`, `read_some`, `try_read` 的正常与越界行为，包括对 `read_some(offset, void*, length)` 的异常分支。
- **链式缓冲区**：`append_to_chain`, `insert_after`, `compute_chain_length`, `normalize` 覆盖链表操作与移动语义。
- **内存管理**：`copy_buffer`, `take_ownership`, `wrap`, 共享缓冲区的引用计数与 `unshare` 等场景。
- **容量控制**：`reserve`, `ensure_tailroom`, `align`、`headroom`/`tailroom` 计算与扩展逻辑。
- **偏移写入与裁剪**：`write_at_offset` 的扩展与覆盖路径，`trim_start`/`trim_end` 对数据指针的调整。
- **错误处理**：非法参数、长度超界、空指针的异常。

### test_io_stream.cpp
- **流式读写**：`write`, `read`, `read_some`, `try_read`, `peek`, `get_writeable_data`，涵盖成功与失败分支。
- **位置控制**：`seek_read`, `seek_write`, `skip`, `readable_bytes`, `written_bytes`，包含负向 seek 扩展头部空间的分支。
- **对齐处理**：`align`, `align_read`、`try_align_read` 及尾部空间不足时的扩容逻辑。
- **缓冲共享**：不可写流的异常路径、`release_buffer`, `reset`。
- **字节序与边界场景**：大小端写读、写位置扩展、`reserve` 对头尾空间的影响及 `get_data` 的视图行为。

## 运行方式

```bash
# 构建
meson setup builddir
ninja -C builddir

# 仅运行 IO 测试
./builddir/tests/libmcpp_test --gtest_filter="IOBufferTest.*:io_stream_test.*"
```

建议在修改 `src/io` 相关逻辑后运行上述测试，确保缓冲区与流的读写语义保持一致，并关注覆盖率报表中新场景的变化。
