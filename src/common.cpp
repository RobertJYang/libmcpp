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

#include <mc/common.h>

#ifdef __linux__
#include <sys/syscall.h>
#include <sys/types.h>
#include <unistd.h>
#elif defined(_WIN32) || defined(_WIN64)
#include <processthreadsapi.h>
#include <windows.h>
#elif defined(__APPLE__)
#include <pthread.h>
#endif

namespace mc {

thread_id get_thread_id() {
#ifdef __linux__
    // Linux平台使用系统调用获取线程ID
    return static_cast<thread_id>(syscall(SYS_gettid));
#elif defined(_WIN32) || defined(_WIN64)
    // Windows平台使用GetCurrentThreadId
    return static_cast<thread_id>(GetCurrentThreadId());
#elif defined(__APPLE__)
    // macOS平台使用pthread_threadid_np
    uint64_t tid;
    pthread_threadid_np(nullptr, &tid);
    return static_cast<thread_id>(tid);
#else
    // 其他平台使用C++标准库的线程ID作为fallback
    // 注意：这可能不是真实的系统线程ID
    auto std_tid = std::this_thread::get_id();
    return static_cast<thread_id>(std::hash<std::thread::id>{}(std_tid));
#endif
}

} // namespace mc