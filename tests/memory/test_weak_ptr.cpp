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

#include <gtest/gtest.h>
#include <mc/exception.h>
#include <mc/memory.h>
#include <test_utilities/test_base.h>
#include <vector>

// 测试用的简单类
class weak_test_object : public mc::shared_base<weak_test_object> {
public:
    weak_test_object() : m_value(100) {
        ++s_construct_count;
    }

    explicit weak_test_object(int value) : m_value(value) {
        ++s_construct_count;
    }

    ~weak_test_object() {
        ++s_destruct_count;
        m_value = -1; // 标记为已析构
    }

    int get_value() const {
        return m_value;
    }

    void set_value(int value) {
        m_value = value;
    }

    static void reset_counters() {
        s_construct_count = 0;
        s_destruct_count  = 0;
    }

    static int get_construct_count() {
        return s_construct_count;
    }

    static int get_destruct_count() {
        return s_destruct_count;
    }

private:
    int        m_value;
    static int s_construct_count;
    static int s_destruct_count;
};

int weak_test_object::s_construct_count = 0;
int weak_test_object::s_destruct_count  = 0;

// 派生类用于测试类型转换
class weak_derived_object : public weak_test_object {
public:
    weak_derived_object() : weak_test_object(200) {
    }

    weak_derived_object(int value) : weak_test_object(value) {
    }

    int get_doubled_value() const {
        return get_value() * 2;
    }
};

class weak_ptr_test : public mc::test::TestBase {
protected:
    void SetUp() override {
        weak_test_object::reset_counters();
    }

    void TearDown() override {
        // 确保所有对象都被正确析构
        EXPECT_EQ(weak_test_object::get_construct_count(), weak_test_object::get_destruct_count());
    }
};

// 测试默认构造
TEST_F(weak_ptr_test, DefaultConstruction) {
    mc::weak_ptr<weak_test_object> weak;

    EXPECT_FALSE(weak);
    EXPECT_TRUE(weak.empty());
    EXPECT_TRUE(weak.expired());
    EXPECT_EQ(weak.get(), nullptr);
    EXPECT_EQ(weak.use_count(), 0);
    EXPECT_FALSE(weak.lock());
}

// 测试 nullptr 构造
TEST_F(weak_ptr_test, NullptrConstruction) {
    mc::weak_ptr<weak_test_object> weak(nullptr);

    EXPECT_FALSE(weak);
    EXPECT_TRUE(weak.empty());
    EXPECT_TRUE(weak.expired());
    EXPECT_EQ(weak.get(), nullptr);
    EXPECT_EQ(weak.use_count(), 0);
}

// 测试从 shared_ptr 构造
TEST_F(weak_ptr_test, ConstructionFromSharedPtr) {
    auto                           shared = mc::make_shared<weak_test_object>(300);
    mc::weak_ptr<weak_test_object> weak(shared);

    EXPECT_TRUE(weak);
    EXPECT_FALSE(weak.empty());
    EXPECT_FALSE(weak.expired());
    EXPECT_EQ(weak.get(), shared.get());
    EXPECT_EQ(weak.use_count(), 1);

    auto locked = weak.lock();
    EXPECT_TRUE(locked);
    EXPECT_EQ(locked.get(), shared.get());
    EXPECT_EQ(locked->get_value(), 300);
    EXPECT_EQ(shared.use_count(), 2); // shared + locked
}

// 测试从原始指针构造
TEST_F(weak_ptr_test, ConstructionFromRawPointer) {
    auto                           shared = mc::make_shared<weak_test_object>(400);
    mc::weak_ptr<weak_test_object> weak(shared.get());

    EXPECT_TRUE(weak);
    EXPECT_FALSE(weak.empty());
    EXPECT_FALSE(weak.expired());
    EXPECT_EQ(weak.get(), shared.get());
    EXPECT_EQ(weak.use_count(), 1);
}

// 测试拷贝构造
TEST_F(weak_ptr_test, CopyConstruction) {
    auto                           shared = mc::make_shared<weak_test_object>(500);
    mc::weak_ptr<weak_test_object> weak1(shared);
    mc::weak_ptr<weak_test_object> weak2(weak1);

    EXPECT_TRUE(weak1);
    EXPECT_TRUE(weak2);
    EXPECT_EQ(weak1.get(), weak2.get());
    EXPECT_EQ(weak1.get(), shared.get());
    EXPECT_EQ(weak1.use_count(), 1);
    EXPECT_EQ(weak2.use_count(), 1);
    EXPECT_FALSE(weak1.expired());
    EXPECT_FALSE(weak2.expired());
}

// 测试移动构造
TEST_F(weak_ptr_test, MoveConstruction) {
    auto                           shared = mc::make_shared<weak_test_object>(600);
    mc::weak_ptr<weak_test_object> weak1(shared);
    auto*                          raw_ptr = weak1.get();
    mc::weak_ptr<weak_test_object> weak2(std::move(weak1));

    EXPECT_FALSE(weak1);
    EXPECT_TRUE(weak1.empty());
    EXPECT_TRUE(weak2);
    EXPECT_FALSE(weak2.empty());
    EXPECT_EQ(weak1.get(), nullptr);
    EXPECT_EQ(weak2.get(), raw_ptr);
    EXPECT_EQ(weak2.get(), shared.get());
}

// 测试类型转换构造
TEST_F(weak_ptr_test, TypeConversionConstruction) {
    auto                              derived_shared = mc::make_shared<weak_derived_object>(700);
    mc::weak_ptr<weak_derived_object> derived_weak(derived_shared);
    mc::weak_ptr<weak_test_object>    base_weak(derived_weak);

    EXPECT_TRUE(base_weak);
    EXPECT_EQ(base_weak.get(), derived_weak.get());
    EXPECT_EQ(base_weak.get(), derived_shared.get());
    EXPECT_EQ(base_weak.use_count(), 1);

    auto locked = base_weak.lock();
    EXPECT_TRUE(locked);
    EXPECT_EQ(locked->get_value(), 700);
}

// 测试拷贝赋值
TEST_F(weak_ptr_test, CopyAssignment) {
    auto                           shared1 = mc::make_shared<weak_test_object>(800);
    auto                           shared2 = mc::make_shared<weak_test_object>(900);
    mc::weak_ptr<weak_test_object> weak1(shared1);
    mc::weak_ptr<weak_test_object> weak2(shared2);

    EXPECT_NE(weak1.get(), weak2.get());

    weak2 = weak1;

    EXPECT_EQ(weak1.get(), weak2.get());
    EXPECT_EQ(weak1.get(), shared1.get());
    EXPECT_EQ(weak1.use_count(), 1);
    EXPECT_EQ(weak2.use_count(), 1);
}

// 测试移动赋值
TEST_F(weak_ptr_test, MoveAssignment) {
    auto                           shared1 = mc::make_shared<weak_test_object>(1000);
    auto                           shared2 = mc::make_shared<weak_test_object>(1100);
    mc::weak_ptr<weak_test_object> weak1(shared1);
    mc::weak_ptr<weak_test_object> weak2(shared2);
    auto*                          raw_ptr1 = weak1.get();

    weak2 = std::move(weak1);

    EXPECT_FALSE(weak1);
    EXPECT_TRUE(weak1.empty());
    EXPECT_TRUE(weak2);
    EXPECT_EQ(weak1.get(), nullptr);
    EXPECT_EQ(weak2.get(), raw_ptr1);
    EXPECT_EQ(weak2.get(), shared1.get());
}

// 测试从 shared_ptr 赋值
TEST_F(weak_ptr_test, AssignmentFromSharedPtr) {
    auto                           shared = mc::make_shared<weak_test_object>(1200);
    mc::weak_ptr<weak_test_object> weak;

    EXPECT_TRUE(weak.empty());

    weak = shared;

    EXPECT_FALSE(weak.empty());
    EXPECT_EQ(weak.get(), shared.get());
    EXPECT_EQ(weak.use_count(), 1);
    EXPECT_FALSE(weak.expired());
}

// 测试类型转换赋值
TEST_F(weak_ptr_test, TypeConversionAssignment) {
    auto                              derived_shared = mc::make_shared<weak_derived_object>(1300);
    mc::weak_ptr<weak_derived_object> derived_weak(derived_shared);
    mc::weak_ptr<weak_test_object>    base_weak;

    base_weak = derived_weak;

    EXPECT_TRUE(base_weak);
    EXPECT_EQ(base_weak.get(), derived_weak.get());
    EXPECT_EQ(base_weak.use_count(), 1);

    // 测试从不同类型的 shared_ptr 赋值
    mc::weak_ptr<weak_test_object>   base_weak2;
    mc::shared_ptr<weak_test_object> base_shared = derived_shared;
    base_weak2                                   = base_shared;

    EXPECT_TRUE(base_weak2);
    EXPECT_EQ(base_weak2.get(), derived_shared.get());
}

// 测试 reset 操作
TEST_F(weak_ptr_test, Reset) {
    auto                           shared = mc::make_shared<weak_test_object>(1400);
    mc::weak_ptr<weak_test_object> weak(shared);

    EXPECT_FALSE(weak.empty());
    EXPECT_FALSE(weak.expired());

    weak.reset();

    EXPECT_TRUE(weak.empty());
    EXPECT_TRUE(weak.expired());
    EXPECT_EQ(weak.get(), nullptr);
    EXPECT_EQ(weak.use_count(), 0);
}

// 测试 swap 操作
TEST_F(weak_ptr_test, Swap) {
    auto                           shared1 = mc::make_shared<weak_test_object>(1500);
    auto                           shared2 = mc::make_shared<weak_test_object>(1600);
    mc::weak_ptr<weak_test_object> weak1(shared1);
    mc::weak_ptr<weak_test_object> weak2(shared2);
    auto*                          raw_ptr1 = weak1.get();
    auto*                          raw_ptr2 = weak2.get();

    weak1.swap(weak2);

    EXPECT_EQ(weak1.get(), raw_ptr2);
    EXPECT_EQ(weak2.get(), raw_ptr1);
    EXPECT_EQ(weak1.get(), shared2.get());
    EXPECT_EQ(weak2.get(), shared1.get());
}

// 测试 lock 和 expired 的基本行为
TEST_F(weak_ptr_test, LockAndExpiredBasic) {
    mc::weak_ptr<weak_test_object> weak;

    {
        auto shared = mc::make_shared<weak_test_object>(1700);
        weak        = shared;

        EXPECT_FALSE(weak.expired());
        EXPECT_EQ(weak.use_count(), 1);

        auto locked = weak.lock();
        EXPECT_TRUE(locked);
        EXPECT_EQ(locked.get(), shared.get());
        EXPECT_EQ(locked->get_value(), 1700);
        EXPECT_EQ(weak.use_count(), 2); // shared + locked
    }

    // shared_ptr 已超出作用域，对象应该被销毁
    EXPECT_TRUE(weak.expired());
    EXPECT_EQ(weak.use_count(), 0);

    auto locked = weak.lock();
    EXPECT_FALSE(locked);
    EXPECT_EQ(locked.get(), nullptr);
}

// 测试多个 weak_ptr 指向同一对象
TEST_F(weak_ptr_test, MultipleWeakPtrs) {
    auto shared = mc::make_shared<weak_test_object>(1800);

    std::vector<mc::weak_ptr<weak_test_object>> weak_ptrs;
    for (int i = 0; i < 10; ++i) {
        weak_ptrs.emplace_back(shared);
    }

    // 所有 weak_ptr 都应该指向同一对象
    for (const auto& weak : weak_ptrs) {
        EXPECT_TRUE(weak);
        EXPECT_FALSE(weak.expired());
        EXPECT_EQ(weak.get(), shared.get());
        EXPECT_EQ(weak.use_count(), 1);

        auto locked = weak.lock();
        EXPECT_TRUE(locked);
        EXPECT_EQ(locked.get(), shared.get());
    }

    // 释放 shared_ptr
    shared.reset();

    // 所有 weak_ptr 都应该过期
    for (const auto& weak : weak_ptrs) {
        EXPECT_TRUE(weak.expired());
        EXPECT_EQ(weak.use_count(), 0);
        EXPECT_FALSE(weak.lock());
    }
}

// 测试 weak_ptr 在对象生命周期中的行为
TEST_F(weak_ptr_test, LifecycleManagement) {
    EXPECT_EQ(weak_test_object::get_construct_count(), 0);
    EXPECT_EQ(weak_test_object::get_destruct_count(), 0);

    mc::weak_ptr<weak_test_object> weak;

    {
        auto shared = mc::make_shared<weak_test_object>(1900);
        weak        = shared;

        EXPECT_EQ(weak_test_object::get_construct_count(), 1);
        EXPECT_EQ(weak_test_object::get_destruct_count(), 0);
        EXPECT_FALSE(weak.expired());

        {
            auto locked = weak.lock();
            EXPECT_TRUE(locked);
            EXPECT_EQ(shared.use_count(), 2);
            EXPECT_EQ(weak_test_object::get_destruct_count(), 0);
        }

        EXPECT_EQ(shared.use_count(), 1);
        EXPECT_EQ(weak_test_object::get_destruct_count(), 0);
    }

    // shared_ptr 已销毁，对象应该被析构
    EXPECT_EQ(weak_test_object::get_construct_count(), 1);
    EXPECT_EQ(weak_test_object::get_destruct_count(), 1);
    EXPECT_TRUE(weak.expired());
    EXPECT_FALSE(weak.lock());
}

// 测试循环引用的解决
class circular_a;
class circular_b;

class circular_a : public mc::shared_base<circular_a> {
public:
    mc::weak_ptr<circular_b> m_b_weak; // 使用 weak_ptr 打破循环

    ~circular_a() {
        ++s_destruct_count;
    }

    static void reset_counter() {
        s_destruct_count = 0;
    }
    static int get_destruct_count() {
        return s_destruct_count;
    }

private:
    static int s_destruct_count;
};

class circular_b : public mc::shared_base<circular_b> {
public:
    mc::shared_ptr<circular_a> m_a_shared; // 保持强引用

    ~circular_b() {
        ++s_destruct_count;
    }

    static void reset_counter() {
        s_destruct_count = 0;
    }
    static int get_destruct_count() {
        return s_destruct_count;
    }

private:
    static int s_destruct_count;
};

int circular_a::s_destruct_count = 0;
int circular_b::s_destruct_count = 0;

TEST_F(weak_ptr_test, CircularReferenceResolution) {
    circular_a::reset_counter();
    circular_b::reset_counter();

    {
        auto a = mc::make_shared<circular_a>();
        auto b = mc::make_shared<circular_b>();

        // 创建单向强引用和反向弱引用
        a->m_b_weak   = b;
        b->m_a_shared = a;

        EXPECT_EQ(a.use_count(), 2); // a + b->m_a_shared
        EXPECT_EQ(b.use_count(), 1); // b (weak_ptr 不增加引用计数)
        EXPECT_FALSE(a->m_b_weak.expired());

        // 从 weak_ptr 获取 shared_ptr
        auto b_locked = a->m_b_weak.lock();
        EXPECT_TRUE(b_locked);
        EXPECT_EQ(b_locked.get(), b.get());
        EXPECT_EQ(b.use_count(), 2); // b + b_locked
    }

    // 由于使用了 weak_ptr 打破循环，对象应该被正确销毁
    EXPECT_EQ(circular_a::get_destruct_count(), 1);
    EXPECT_EQ(circular_b::get_destruct_count(), 1);
}

// 测试 weak_ptr 的比较操作
TEST_F(weak_ptr_test, ComparisonOperators) {
    auto shared1 = mc::make_shared<weak_test_object>(2000);
    auto shared2 = mc::make_shared<weak_test_object>(2100);

    mc::weak_ptr<weak_test_object> weak1(shared1);
    mc::weak_ptr<weak_test_object> weak2(shared1);
    mc::weak_ptr<weak_test_object> weak3(shared2);
    mc::weak_ptr<weak_test_object> weak_null;

    // 测试相等比较
    EXPECT_TRUE(weak1 == weak2);
    EXPECT_FALSE(weak1 == weak3);
    EXPECT_FALSE(weak1 == weak_null);
    EXPECT_TRUE(weak_null == nullptr);
    EXPECT_TRUE(nullptr == weak_null);

    // 测试不等比较
    EXPECT_FALSE(weak1 != weak2);
    EXPECT_TRUE(weak1 != weak3);
    EXPECT_TRUE(weak1 != weak_null);
    EXPECT_FALSE(weak_null != nullptr);
}

// 测试 weak_ptr 的边界情况
TEST_F(weak_ptr_test, EdgeCases) {
    // 测试对已过期的 weak_ptr 进行操作
    mc::weak_ptr<weak_test_object> expired_weak;
    {
        auto shared  = mc::make_shared<weak_test_object>(2200);
        expired_weak = shared;
    }

    EXPECT_TRUE(expired_weak.expired());
    EXPECT_EQ(expired_weak.use_count(), 0);
    EXPECT_FALSE(expired_weak.lock());

    // 测试自赋值
    mc::weak_ptr<weak_test_object> weak;

    auto shared = mc::make_shared<weak_test_object>(2300);
    weak        = shared;

    // 测试与自己交换
    auto* original_ptr = weak.get();
    weak.swap(weak);
    EXPECT_EQ(weak.get(), original_ptr);
}

// 测试 shared_from_this 和 weak_from_this
TEST_F(weak_ptr_test, SharedFromThisAndWeakFromThis) {
    auto shared = mc::make_shared<weak_test_object>(2400);

    // 测试 shared_from_this
    auto shared2 = shared->shared_from_this();
    EXPECT_EQ(shared.get(), shared2.get());
    EXPECT_EQ(shared.use_count(), 2);

    // 测试 weak_from_this
    auto weak = shared->weak_from_this();
    EXPECT_TRUE(weak);
    EXPECT_EQ(weak.get(), shared.get());
    EXPECT_FALSE(weak.expired());
    EXPECT_EQ(weak.use_count(), 2); // shared + shared2

    auto locked = weak.lock();
    EXPECT_TRUE(locked);
    EXPECT_EQ(locked.get(), shared.get());
    EXPECT_EQ(shared.use_count(), 3); // shared + shared2 + locked
}