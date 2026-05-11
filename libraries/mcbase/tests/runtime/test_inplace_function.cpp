/*
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * openUBMC is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *         http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 */

#include <gtest/gtest.h>

#include <mc/small_function.h>

#include <atomic>
#include <memory>

namespace {

struct small_counting_handler {
    static inline std::atomic<int> live_allocations{0};

    int* call_count{nullptr};

    static void* operator new(std::size_t size)
    {
        ++live_allocations;
        return ::operator new(size);
    }

    static void* operator new(std::size_t, void* ptr) noexcept
    {
        return ptr;
    }

    static void operator delete(void* ptr) noexcept
    {
        --live_allocations;
        ::operator delete(ptr);
    }

    static void operator delete(void*, void*) noexcept {}

    void operator()() const
    {
        ++(*call_count);
    }
};

struct large_counting_handler {
    static inline std::atomic<int> live_allocations{0};

    int  buffer[32]{};
    int* call_count{nullptr};

    static void* operator new(std::size_t size)
    {
        ++live_allocations;
        return ::operator new(size);
    }

    static void* operator new(std::size_t, void* ptr) noexcept
    {
        return ptr;
    }

    static void operator delete(void* ptr) noexcept
    {
        --live_allocations;
        ::operator delete(ptr);
    }

    static void operator delete(void*, void*) noexcept {}

    void operator()() const
    {
        ++(*call_count);
    }
};

TEST(small_function_test, stores_move_only_callable_and_invokes_it)
{
    int result = 0;

    mc::small_function<void(), 64> func([state = std::make_unique<int>(7), &result]() mutable {
        result = *state;
    });

    ASSERT_TRUE(static_cast<bool>(func));
    func();
    EXPECT_EQ(result, 7);
}

TEST(small_function_test, keeps_small_callable_inline)
{
    int call_count = 0;
    small_counting_handler::live_allocations.store(0);

    {
        mc::small_function<void(), 64> func(small_counting_handler{&call_count});
        ASSERT_TRUE(static_cast<bool>(func));
        EXPECT_EQ(small_counting_handler::live_allocations.load(), 0);
        func();
    }

    EXPECT_EQ(call_count, 1);
    EXPECT_EQ(small_counting_handler::live_allocations.load(), 0);
}

TEST(small_function_test, move_transfers_callable_and_invalidates_source)
{
    int call_count = 0;

    mc::small_function<void(), 64> source([&call_count]() {
        ++call_count;
    });

    mc::small_function<void(), 64> target(std::move(source));

    EXPECT_FALSE(static_cast<bool>(source));
    ASSERT_TRUE(static_cast<bool>(target));
    target();
    EXPECT_EQ(call_count, 1);
}

TEST(small_function_test, stores_large_callable_on_heap_and_releases_it)
{
    int call_count = 0;
    large_counting_handler::live_allocations.store(0);

    {
        mc::small_function<void(), 64> func(large_counting_handler{{}, &call_count});
        ASSERT_TRUE(static_cast<bool>(func));
        EXPECT_EQ(large_counting_handler::live_allocations.load(), 1);
        func();
    }

    EXPECT_EQ(call_count, 1);
    EXPECT_EQ(large_counting_handler::live_allocations.load(), 0);
}

} // namespace
