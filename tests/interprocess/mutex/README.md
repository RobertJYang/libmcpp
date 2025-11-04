# Interprocess Mutex 模块测试

Copyright (c) 2025 Huawei Technologies Co., Ltd.
openUBMC is licensed under Mulan PSL v2.
You can use this software according to the terms and conditions of the Mulan PSL v2.
You may obtain a copy of Mulan PSL v2 at: http://license.coscl.org.cn/MulanPSL2
THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
MERCHANTABILITY OR FITNESS FOR A PARTICULAR PURPOSE.
See the Mulan PSL v2 for more details.

## 概述

本文档描述了 Interprocess Mutex 模块的测试用例。Mutex 模块提供了进程间同步原语，包括进程间互斥锁（`ipc_mutex`）和进程间读写锁（`ipc_shared_mutex`），以及对应的线程安全包装类（`mutex` 和 `shared_mutex`），用于支持多进程和多线程应用程序的同步和资源共享。

## 测试文件

- `test_ipc_mutex.cpp` - IPC 互斥锁测试（22 个用例）
- `test_shared_mutex.cpp` - IPC 读写锁和包装类测试（66 个用例）

## 详细测试用例

### test_ipc_mutex.cpp - IPC 互斥锁测试（22 个用例）

#### IpcMutexReentrantTryLock
测试重复获取锁（同一进程）应该返回 `false`，不允许重入。

#### IpcMutexUnlockWithoutLock
测试不持有锁时解锁的行为。

#### IpcMutexIsProcessAlive
测试进程存活检查功能。

#### IpcMutexTryLockForSuccess
测试 `try_lock_for` 在超时前成功获取锁的场景。

#### MutexTryLockForSuccess
测试 `mutex` 包装类的 `try_lock_for` 成功场景。

#### IpcMutexCanPreemptOwnerDead
测试 `owner_dead` 路径，验证进程死亡时的锁抢占。

#### IpcMutexCanPreemptTimeout
测试 `timeout` 路径。

#### IpcMutexTryLockCompareExchangeFailure
测试 `compare_exchange_strong` 失败场景。

#### IpcMutexIsProcessAliveErrnoBranch
测试 `is_process_alive` 中 `errno != ESRCH` 分支。

#### IpcMutexCanPreemptNowLessEqualLockTime
测试 `can_preempt` 中 `now <= lock_time` 分支。

#### IpcMutexCanPreemptTimeoutNotMet
测试超时不满足的分支。

#### IpcMutexTryLockOwnerDeadPath
测试 `owner_dead` 路径。

#### IpcMutexTryLockTimeoutPath
测试 `timeout` 路径。

#### IpcMutexTryLockCompareExchangeNotOwnedFailure
测试 `compare_exchange` 在 `not_owned` 路径失败。

#### IpcMutexTryLockCompareExchangePreemptFailure
测试 `compare_exchange` 在抢占路径失败。

#### IpcMutexTryLockForTimeout
测试 `try_lock_for` 超时分支，验证超时行为。

#### IpcMutexLockRetry
测试 `lock` 多次尝试获取锁的分支，验证重试机制。

#### IpcMutexTryLockForRetryAndTimeout
测试 `try_lock_for` 多次尝试和超时的分支。

#### IpcMutexCanPreemptTimeBoundary
测试 `can_preempt` 中时间边界的场景。

#### IpcMutexIsProcessAliveSuccess
测试 `is_process_alive` 中进程存活的情况（`kill` 成功）。

#### IpcMutexTryLockNotOwnedSuccess
测试 `try_lock` 中锁空闲时成功获取的情况。

#### IpcMutexTryLockNotOwnedHighContention
测试 `try_lock` 在高竞争场景下的行为。

### test_shared_mutex.cpp - IPC 读写锁和包装类测试（66 个用例）

### 基础功能测试（4 个用例）

#### BasicThreadSafety
测试 `mutex` 包装类在单进程多线程环境下的基本功能，包括锁的获取和释放。

#### TryLock
测试 `mutex` 的 `try_lock` 方法，验证非阻塞获取锁的行为。

#### TryLockFor
测试 `mutex` 的 `try_lock_for` 方法，验证带超时的锁获取行为。

#### SharedMutexBasicThreadSafety
测试 `shared_mutex` 包装类在单进程多线程环境下的基本功能，包括读锁和写锁的获取和释放。

### ipc_shared_mutex 测试（43 个用例）

#### IpcSharedMutexReentrantTryLockShared
测试重复获取读锁（应该返回 `false`）。

#### IpcSharedMutexReentrantTryLock
测试重复获取写锁（已持有写锁，应该返回 `false`）。

#### IpcSharedMutexUnlockSharedWithoutLock
测试不持有读锁时解锁的行为。

#### IpcSharedMutexUnlockWithoutLock
测试不持有写锁时解锁的行为。

#### IpcSharedMutexCleanupDeadLocks
测试清理死亡锁功能。

#### IpcSharedMutexDeadWriter
测试写锁持有者死亡的处理。

#### IpcSharedMutexRegisterReaderAlreadyRegistered
测试已注册读者的分支。

#### IpcSharedMutexUnlockSharedZeroCount
测试 `unlock_shared` 中 `reader_count == 0` 分支。

#### IpcSharedMutexAggressiveCleanupNoReaders
测试 `aggressive_cleanup` 中 `reader_count == 0` 分支。

#### IpcSharedMutexTryLockWithActiveReaders
测试有活跃读者时的写锁获取行为。

#### IpcSharedMutexTryLockSharedWithAliveWriter
测试写者存活时的读锁获取行为。

#### IpcSharedMutexFindFreeSlotUsesLastFree
测试使用 `last_free_slot` 的分支。

#### IpcSharedMutexFindFreeSlotTraversal
测试遍历查找的分支。

#### IpcSharedMutexShouldCleanReaderZeroPid
测试 `should_clean_reader` 中 `reader_pid == 0` 分支。

#### IpcSharedMutexShouldCleanReaderCurrentPid
测试 `reader_pid == current_pid` 分支。

#### IpcSharedMutexIsWriterAliveZeroPid
测试 `is_writer_alive` 中 `writer_pid == 0` 分支。

#### IpcSharedMutexAggressiveCleanupWithCleaned
测试 `aggressive_cleanup` 中 `cleaned_count > 0` 分支。

#### IpcSharedMutexAggressiveCleanupWriter
测试清理写者的分支。

#### IpcSharedMutexTryLockStillHasReaders
测试清理后仍有读者的分支。

#### IpcSharedMutexRegisterReaderFullSlots
测试槽位满的情况。

#### IpcSharedMutexRegisterReaderAfterCleanup
测试清理后找到槽位的情况。

#### IpcSharedMutexIsWriterAliveZeroTime
测试 `is_writer_alive` 中 `write_time == 0` 分支。

#### IpcSharedMutexShouldCleanReaderZeroTime
测试 `should_clean_reader` 中 `read_time == 0` 分支。

#### IpcSharedMutexAggressiveCleanupNoCleaned
测试 `aggressive_cleanup` 中 `cleaned_count == 0` 分支。

#### IpcSharedMutexAggressiveCleanupNoWriter
测试 `m_writer_pid == 0` 分支。

#### IpcSharedMutexAggressiveCleanupSamePidWriter
测试 `m_writer_pid == pid` 分支。

#### IpcSharedMutexAggressiveCleanupWriterAlive
测试 `is_writer_alive` 返回 `true` 分支。

#### IpcSharedMutexAggressiveCleanupReaderCountUpdate
测试 `reader_count` 更新分支。

#### IpcSharedMutexTryLockSharedDeadWriter
测试写者死亡时的读锁获取。

#### IpcSharedMutexTryLockDeadWriter
测试写者死亡时的写锁获取。

#### IpcSharedMutexTryLockAfterCleanupStillHasReaders
测试清理后仍有读者。

#### IpcSharedMutexRegisterReaderStillNoSlotAfterCleanup
测试清理后仍找不到槽位。

#### IpcSharedMutexIsWriterAliveNonZeroTime
测试 `write_time > 0` 分支。

#### IpcSharedMutexIsWriterAliveNotTimeout
测试 `now - write_time <= timeout` 分支。

#### IpcSharedMutexTryLockSharedNoWriter
测试 `m_writer_pid == 0` 分支。

#### IpcSharedMutexTryLockSharedSamePidWriter
测试 `m_writer_pid == pid` 分支。

#### IpcSharedMutexTryLockNoWriter
测试 `m_writer_pid == 0` 分支。

#### IpcSharedMutexTryLockNoReaders
测试 `m_reader_count == 0` 分支。

#### IpcSharedMutexTryLockAfterCleanupNoReaders
测试清理后 `reader_count == 0` 分支。

#### IpcSharedMutexFindFreeSlotLastFreeOutOfRange
测试 `m_last_free_slot >= MAX_READERS` 分支。

#### IpcSharedMutexFindFreeSlotLastFreeOccupied
测试 `m_readers[m_last_free_slot].pid != 0` 分支。

#### IpcSharedMutexRegisterReaderCleanupReturnsZero
测试 `aggressive_cleanup == 0` 分支。

#### IpcSharedMutexShouldCleanReaderNotTimeout
测试 `now - read_time <= timeout` 分支。

### shared_mutex 包装类测试（18 个用例）

#### IpcSharedMutexLockSharedRetry
测试 `lock_shared` 多次尝试获取读锁的分支，验证重试机制。

#### IpcSharedMutexLockRetry
测试 `lock` 多次尝试获取写锁的分支，验证重试机制。

#### IpcSharedMutexRegisterReaderWithWriter
测试 `register_reader_unsafe` 中持有写锁时不能再获取读锁的分支。

#### IpcMutexLockRetry
测试 `mutex` 包装类的 `lock` 多次尝试获取锁的分支。

#### IpcMutexTryLockForRetry
测试 `mutex` 包装类的 `try_lock_for` 多次尝试和超时的分支。

#### IpcSharedMutexTryLockSharedSamePidWriterBlocked
测试 `try_lock_shared` 中写锁持有者就是当前进程时的分支。

#### IpcSharedMutexTryLockSharedDeadWriterCleared
测试 `try_lock_shared` 中写锁持有者不是当前进程但已死亡时的分支。

#### IpcSharedMutexTryLockDeadWriterCleared
测试 `try_lock` 中写锁持有者不是当前进程但已死亡时的分支。

#### IpcSharedMutexTryLockWithMultipleReaders
测试 `try_lock` 中有多个读者时的分支。

#### IpcSharedMutexTryLockSharedWithAliveWriterBlocked
测试 `try_lock_shared` 中写锁持有者不是当前进程且存活时的分支。

#### IpcSharedMutexAggressiveCleanupReaderCountDecrease
测试 `aggressive_cleanup_readers_unsafe` 中读者计数更新的分支。

#### SharedMutexReentrantTryLock
测试重复获取写锁（应该返回 `false`）。

#### SharedMutexReentrantLock
测试重复获取写锁（应该抛出异常）。

#### SharedMutexUnlockWithoutLock
测试不持有写锁时解锁（应该抛出异常）。

#### SharedMutexUnlockSharedWithoutLock
测试不持有读锁时解锁（应该抛出异常）。

#### SharedMutexTryLockSharedWithWriter
测试写锁持有时的读锁获取行为。

#### SharedMutexMultipleReaders
测试多读者计数和并发，验证多个线程可以同时持有读锁。

#### SharedMutexWriterBlocksReaders
测试写锁阻塞读者，验证写锁获取时读者无法获取读锁。

## 运行测试

### 运行所有 mutex 模块测试

```bash
# 运行所有 IPC 互斥锁测试
./builddir/tests/libmcpp_test --gtest_filter="IpcMutexTestFixture.*"

# 运行所有 IPC 读写锁和包装类测试
./builddir/tests/libmcpp_test --gtest_filter="SharedMutexTestFixture.*"

# 或通过 meson 传递参数
meson test -C builddir --test-args="--gtest_filter=IpcMutexTestFixture.*"
meson test -C builddir --test-args="--gtest_filter=SharedMutexTestFixture.*"
```

### 运行特定测试用例

```bash
# 运行 IPC 互斥锁测试
./builddir/tests/libmcpp_test --gtest_filter="IpcMutexTestFixture.IpcMutexReentrantTryLock"

# 运行所有 IPC 互斥锁测试
./builddir/tests/libmcpp_test --gtest_filter="IpcMutexTestFixture.*"

# 运行基础功能测试
./builddir/tests/libmcpp_test --gtest_filter="SharedMutexTestFixture.BasicThreadSafety"

# 运行所有 ipc_shared_mutex 测试
./builddir/tests/libmcpp_test --gtest_filter="SharedMutexTestFixture.IpcSharedMutex*"

# 运行所有 shared_mutex 包装类测试
./builddir/tests/libmcpp_test --gtest_filter="SharedMutexTestFixture.SharedMutex*"
```

## 测试统计

- **测试文件总数**: 2 个
- **测试用例总数**: 88 个
      - `test_ipc_mutex.cpp`: 22 个用例
        - ipc_mutex 测试: 21 个用例
        - mutex 包装类测试: 1 个用例
      - `test_shared_mutex.cpp`: 66 个用例
        - 基础功能测试: 4 个用例
        - ipc_shared_mutex 测试: 43 个用例
        - shared_mutex 包装类测试: 18 个用例
        - mutex 包装类测试: 1 个用例
- **测试覆盖的功能模块**:
  - `ipc_mutex` - 进程间互斥锁
  - `ipc_shared_mutex` - 进程间读写锁
  - `mutex` - 互斥锁包装类（线程安全）
  - `shared_mutex` - 读写锁包装类（线程安全）

## 测试注意事项

1. **进程间锁语义**: `ipc_mutex` 和 `ipc_shared_mutex` 基于进程 ID，同一进程的不同线程会被认为是"重复获取"。测试中使用包装后的 `mutex` 和 `shared_mutex` 类来支持单进程多线程场景。

2. **共享内存依赖**: 所有互斥锁测试都依赖共享内存，测试框架会自动创建和管理共享内存。

3. **并发测试**: 大量测试用例涉及多线程并发，测试了线程安全性和竞争条件。

4. **分支覆盖率**: 测试用例重点覆盖了各种条件分支，包括错误处理、边界条件和状态转换路径。

5. **超时测试**: `IpcMutexTryLockForTimeout` 测试需要保持锁超过超时时间，确保正确验证超时行为。

6. **进程死亡处理**: 多个测试用例验证了进程死亡时的锁清理和抢占机制。

7. **槽位管理**: `ipc_shared_mutex` 使用固定大小的读者槽位数组，测试覆盖了槽位满、查找、清理等各种场景。

