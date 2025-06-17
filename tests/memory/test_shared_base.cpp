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
#include <limits>
#include <mc/exception.h>
#include <mc/memory.h>

// 测试用的简单类，直接继承 shared_base
class base_test_object : public mc::shared_base<base_test_object> {
public:
    base_test_object() : m_value(0) {
        ++s_construct_count;
    }

    explicit base_test_object(int value) : m_value(value) {
        ++s_construct_count;
    }

    // 拷贝构造函数
    base_test_object(const base_test_object& other) : mc::shared_base<base_test_object>(other), m_value(other.m_value) {
        ++s_construct_count;
    }

    // 移动构造函数
    base_test_object(base_test_object&& other) noexcept : mc::shared_base<base_test_object>(std::move(other)), m_value(other.m_value) {
        ++s_construct_count;
    }

    // 拷贝赋值运算符
    base_test_object& operator=(const base_test_object& other) {
        if (this != &other) {
            mc::shared_base<base_test_object>::operator=(other);
            m_value = other.m_value;
        }
        return *this;
    }

    // 移动赋值运算符
    base_test_object& operator=(base_test_object&& other) noexcept {
        if (this != &other) {
            mc::shared_base<base_test_object>::operator=(std::move(other));
            m_value = other.m_value;
        }
        return *this;
    }

    ~base_test_object() {
        ++s_destruct_count;
        m_value = -9999; // 标记为已析构
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

int base_test_object::s_construct_count = 0;
int base_test_object::s_destruct_count  = 0;

class SharedBaseTest : public ::testing::Test {
protected:
    void SetUp() override {
        base_test_object::reset_counters();
    }

    void TearDown() override {
        // 确保所有对象都被正确析构
        EXPECT_EQ(base_test_object::get_construct_count(), base_test_object::get_destruct_count());
    }
};

// 测试对象初始状态
TEST_F(SharedBaseTest, InitialState) {
    auto obj = new base_test_object(42);

    // 初始状态：未被管理，未销毁，引用计数为 INIT_VALUE
    EXPECT_FALSE(obj->is_managed());
    EXPECT_FALSE(obj->is_destroyed());
    EXPECT_EQ(obj->ref_count(), std::numeric_limits<uint32_t>::max());
    EXPECT_EQ(obj->weak_count(), 0);
    EXPECT_EQ(obj->get_value(), 42);

    delete obj;
}

// 测试手动引用计数管理
TEST_F(SharedBaseTest, ManualReferenceCountManagement) {
    auto obj = new base_test_object(100);

    // 第一次 add_ref 应该将状态从 INIT_VALUE 转换为 1
    obj->add_ref();
    EXPECT_TRUE(obj->is_managed());
    EXPECT_FALSE(obj->is_destroyed());
    EXPECT_EQ(obj->ref_count(), 1);

    // 再次 add_ref 应该增加计数
    obj->add_ref();
    EXPECT_EQ(obj->ref_count(), 2);

    // 第一次 release_ref 应该减少计数
    bool should_delete = obj->release_ref();
    EXPECT_FALSE(should_delete);
    EXPECT_EQ(obj->ref_count(), 1);
    EXPECT_FALSE(obj->is_destroyed());

    // 最后一次 release_ref 应该返回 true
    should_delete = obj->release_ref();
    EXPECT_TRUE(should_delete);
    EXPECT_EQ(obj->ref_count(), 0);
    EXPECT_TRUE(obj->is_destroyed());

    delete obj;
}

// 测试弱引用计数管理
TEST_F(SharedBaseTest, WeakReferenceCountManagement) {
    auto obj = new base_test_object(200);

    // 添加弱引用
    obj->add_weak_ref();
    EXPECT_EQ(obj->weak_count(), 1);

    obj->add_weak_ref();
    EXPECT_EQ(obj->weak_count(), 2);

    // 释放弱引用
    bool should_delete = obj->release_weak_ref();
    EXPECT_FALSE(should_delete); // 对象未销毁，不应删除
    EXPECT_EQ(obj->weak_count(), 1);

    // 强引用计数管理
    obj->add_ref();
    EXPECT_EQ(obj->ref_count(), 1);

    bool strong_released = obj->release_ref();
    EXPECT_TRUE(strong_released);
    EXPECT_TRUE(obj->is_destroyed());

    // 现在释放弱引用，应该返回 true（可以删除）
    should_delete = obj->release_weak_ref();
    EXPECT_TRUE(should_delete);
    EXPECT_EQ(obj->weak_count(), 0);

    delete obj;
}

// 测试 try_add_ref 功能
TEST_F(SharedBaseTest, TryAddRefFunctionality) {
    auto obj = new base_test_object(300);

    // 未管理状态下的 try_add_ref 应该失败
    bool success = obj->try_add_ref();
    EXPECT_FALSE(success);
    EXPECT_FALSE(obj->is_managed());

    // 手动管理后的 try_add_ref 应该成功
    obj->add_ref();
    EXPECT_TRUE(obj->is_managed());
    EXPECT_EQ(obj->ref_count(), 1);

    success = obj->try_add_ref();
    EXPECT_TRUE(success);
    EXPECT_EQ(obj->ref_count(), 2);

    // 释放到销毁状态后的 try_add_ref 应该失败
    obj->release_ref();
    obj->release_ref();
    EXPECT_TRUE(obj->is_destroyed());

    success = obj->try_add_ref();
    EXPECT_FALSE(success);

    delete obj;
}

// 测试状态转换
TEST_F(SharedBaseTest, StateTransitions) {
    auto obj = new base_test_object(400);

    // 初始状态：INIT_VALUE
    EXPECT_EQ(obj->ref_count(), std::numeric_limits<uint32_t>::max());
    EXPECT_FALSE(obj->is_managed());
    EXPECT_FALSE(obj->is_destroyed());

    // 状态转换：INIT_VALUE -> 1
    obj->add_ref();
    EXPECT_EQ(obj->ref_count(), 1);
    EXPECT_TRUE(obj->is_managed());
    EXPECT_FALSE(obj->is_destroyed());

    // 状态转换：1 -> 2
    obj->add_ref();
    EXPECT_EQ(obj->ref_count(), 2);
    EXPECT_TRUE(obj->is_managed());
    EXPECT_FALSE(obj->is_destroyed());

    // 状态转换：2 -> 1
    obj->release_ref();
    EXPECT_EQ(obj->ref_count(), 1);
    EXPECT_TRUE(obj->is_managed());
    EXPECT_FALSE(obj->is_destroyed());

    // 状态转换：1 -> DESTROYED (0)
    bool should_delete = obj->release_ref();
    EXPECT_TRUE(should_delete);
    EXPECT_EQ(obj->ref_count(), 0);
    EXPECT_TRUE(obj->is_managed()); // 仍然被管理过
    EXPECT_TRUE(obj->is_destroyed());

    delete obj;
}

// 测试异常情况
TEST_F(SharedBaseTest, ExceptionCases) {
    auto obj = new base_test_object(500);

    // 对已销毁对象调用 add_ref 应该抛异常
    obj->add_ref();
    obj->release_ref();
    EXPECT_TRUE(obj->is_destroyed());

    EXPECT_THROW(obj->add_ref(), mc::invalid_op_exception);

    // 对未管理或已销毁对象调用 release_ref 应该抛异常
    EXPECT_THROW(obj->release_ref(), mc::invalid_op_exception);

    delete obj;

    // 对未管理对象调用 release_ref 应该抛异常
    auto obj2 = new base_test_object(600);
    EXPECT_THROW(obj2->release_ref(), mc::invalid_op_exception);

    delete obj2;
}

// 测试拷贝和移动构造函数
TEST_F(SharedBaseTest, CopyAndMoveConstructors) {
    // 记录初始计数器状态
    int initial_construct_count = base_test_object::get_construct_count();
    int initial_destruct_count  = base_test_object::get_destruct_count();

    auto obj1 = new base_test_object(700);

    // 增加引用计数
    obj1->add_ref();

    // 拷贝构造：新对象应该处于初始状态
    auto obj2 = new base_test_object(*obj1);
    EXPECT_EQ(obj2->ref_count(), base_test_object::INVALID);
    EXPECT_FALSE(obj2->is_managed());
    EXPECT_FALSE(obj2->is_destroyed());
    EXPECT_EQ(obj2->get_value(), 700);

    // 移动构造：同样应该处于初始状态
    auto obj3 = new base_test_object(std::move(*obj1));
    EXPECT_EQ(obj3->ref_count(), base_test_object::INVALID);
    EXPECT_FALSE(obj3->is_managed());
    EXPECT_FALSE(obj3->is_destroyed());
    EXPECT_EQ(obj3->get_value(), 700);

    // 原对象的状态不应该改变
    EXPECT_EQ(obj1->ref_count(), 1);
    EXPECT_TRUE(obj1->is_managed());
    EXPECT_TRUE(obj1->release_ref());

    // 清理所有对象
    delete obj1;
    delete obj2;
    delete obj3;

    // 验证构造和析构计数匹配
    int final_construct_count = base_test_object::get_construct_count();
    int final_destruct_count  = base_test_object::get_destruct_count();
    EXPECT_EQ(final_construct_count - initial_construct_count,
              final_destruct_count - initial_destruct_count);
}

// 测试赋值操作符
TEST_F(SharedBaseTest, AssignmentOperators) {
    auto obj1 = new base_test_object(800);
    auto obj2 = new base_test_object(900);

    obj1->add_ref();
    obj2->add_ref();

    auto obj1_initial_count = obj1->ref_count();
    auto obj2_initial_count = obj2->ref_count();

    // 拷贝赋值：引用计数状态不应该改变
    *obj2 = *obj1;
    EXPECT_EQ(obj1->ref_count(), obj1_initial_count);
    EXPECT_EQ(obj2->ref_count(), obj2_initial_count);
    EXPECT_EQ(obj2->get_value(), 800); // 值被拷贝

    // 移动赋值：引用计数状态不应该改变
    *obj2 = std::move(*obj1);
    EXPECT_EQ(obj1->ref_count(), obj1_initial_count);
    EXPECT_EQ(obj2->ref_count(), obj2_initial_count);
    EXPECT_EQ(obj2->get_value(), 800); // 值被移动

    obj1->release_ref();
    obj2->release_ref();
    delete obj1;
    delete obj2;
}

// 测试 shared_from_this 和 weak_from_this
TEST_F(SharedBaseTest, FromThisMethods) {
    auto obj = new base_test_object(1000);

    // 在未管理状态下调用 shared_from_this
    auto shared = obj->shared_from_this();
    EXPECT_TRUE(shared);
    EXPECT_EQ(shared.get(), obj);
    EXPECT_TRUE(obj->is_managed());
    EXPECT_EQ(obj->ref_count(), 1);

    // weak_from_this
    auto weak = obj->weak_from_this();
    EXPECT_TRUE(weak);
    EXPECT_EQ(weak.get(), obj);
    EXPECT_FALSE(weak.expired());

    // 多次调用 shared_from_this
    auto shared2 = obj->shared_from_this();
    EXPECT_EQ(shared2.get(), obj);
    EXPECT_EQ(obj->ref_count(), 2);

    // 清理
    shared.reset();
    shared2.reset();

    // weak_ptr 应该过期
    EXPECT_TRUE(weak.expired());
    EXPECT_FALSE(weak.lock());
}

// 测试 from_raw 静态方法
TEST_F(SharedBaseTest, FromRawStaticMethod) {
    // 测试 nullptr
    auto null_ptr = base_test_object::from_raw(nullptr);
    EXPECT_FALSE(null_ptr);

    // 测试未管理对象，add_ref = true
    auto obj     = new base_test_object(1100);
    auto shared1 = base_test_object::from_raw(obj, true);

    EXPECT_TRUE(shared1);
    EXPECT_EQ(shared1.get(), obj);
    EXPECT_TRUE(obj->is_managed());
    EXPECT_EQ(obj->ref_count(), 1);

    // 测试已管理对象，add_ref = true（需要再增加引用）
    auto shared2 = base_test_object::from_raw(obj, true);
    EXPECT_TRUE(shared2);
    EXPECT_EQ(shared2.get(), obj);
    EXPECT_EQ(obj->ref_count(), 2); // shared1 + shared2

    // 清理
    shared1.reset();
    shared2.reset();
}

// 测试引用计数边界值
TEST_F(SharedBaseTest, ReferenceCountBoundaries) {
    auto obj = new base_test_object(1200);

    // 测试从 INIT_VALUE 到 1 的转换
    EXPECT_EQ(obj->ref_count(), std::numeric_limits<uint32_t>::max());
    obj->add_ref();
    EXPECT_EQ(obj->ref_count(), 1);

    // 测试大量增加引用计数
    for (int i = 0; i < 1000; ++i) {
        obj->add_ref();
    }
    EXPECT_EQ(obj->ref_count(), 1001);

    // 测试大量减少引用计数
    for (int i = 0; i < 1000; ++i) {
        bool should_delete = obj->release_ref();
        EXPECT_FALSE(should_delete);
    }
    EXPECT_EQ(obj->ref_count(), 1);

    // 最后一次释放
    bool should_delete = obj->release_ref();
    EXPECT_TRUE(should_delete);
    EXPECT_EQ(obj->ref_count(), 0);
    EXPECT_TRUE(obj->is_destroyed());

    delete obj;
}

// 测试混合强弱引用场景
TEST_F(SharedBaseTest, MixedStrongWeakReferences) {
    auto obj = new base_test_object(1300);

    // 添加弱引用
    obj->add_weak_ref();
    obj->add_weak_ref();
    EXPECT_EQ(obj->weak_count(), 2);

    // 添加强引用
    obj->add_ref();
    EXPECT_EQ(obj->ref_count(), 1);
    EXPECT_TRUE(obj->is_managed());

    // try_add_ref 应该成功
    bool success = obj->try_add_ref();
    EXPECT_TRUE(success);
    EXPECT_EQ(obj->ref_count(), 2);

    // 释放所有强引用
    obj->release_ref();
    obj->release_ref();
    EXPECT_TRUE(obj->is_destroyed());
    EXPECT_EQ(obj->ref_count(), 0);

    // try_add_ref 应该失败
    success = obj->try_add_ref();
    EXPECT_FALSE(success);

    // 释放弱引用
    bool should_delete = obj->release_weak_ref();
    EXPECT_FALSE(should_delete); // 还有一个弱引用
    EXPECT_EQ(obj->weak_count(), 1);

    should_delete = obj->release_weak_ref();
    EXPECT_TRUE(should_delete); // 最后一个弱引用
    EXPECT_EQ(obj->weak_count(), 0);

    delete obj;
}

// 测试并发场景的基础（单线程模拟）
TEST_F(SharedBaseTest, ConcurrencyBasics) {
    // 由于对象一旦销毁就不能重新初始化，我们在循环中创建新对象
    for (int iteration = 0; iteration < 10; ++iteration) { // 减少迭代次数
        auto obj = new base_test_object(1400 + iteration);

        // 确保处于初始状态
        EXPECT_FALSE(obj->is_managed());
        EXPECT_EQ(obj->ref_count(), std::numeric_limits<uint32_t>::max());

        // 第一次 add_ref
        obj->add_ref();
        EXPECT_TRUE(obj->is_managed());
        EXPECT_EQ(obj->ref_count(), 1);

        // 多次 try_add_ref
        for (int i = 0; i < 10; ++i) {
            bool success = obj->try_add_ref();
            EXPECT_TRUE(success);
        }

        EXPECT_EQ(obj->ref_count(), 11);

        // 释放所有引用
        for (int i = 0; i < 11; ++i) {
            bool should_delete = obj->release_ref();
            if (i < 10) {
                EXPECT_FALSE(should_delete);
            } else {
                EXPECT_TRUE(should_delete);
            }
        }

        EXPECT_TRUE(obj->is_destroyed());
        EXPECT_EQ(obj->ref_count(), 0);

        delete obj;
    }
}