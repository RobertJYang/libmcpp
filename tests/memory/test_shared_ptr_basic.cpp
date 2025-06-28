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
#include <vector>

// 测试用的简单类
class test_object : public mc::enable_shared_from_this<test_object> {
public:
    test_object() : m_value(42) {
        ++s_construct_count;
    }

    explicit test_object(int value) : m_value(value) {
        ++s_construct_count;
    }

    ~test_object() {
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

int test_object::s_construct_count = 0;
int test_object::s_destruct_count  = 0;

// 派生类用于测试类型转换
class derived_object : public test_object {
public:
    derived_object() : test_object(100) {
    }

    derived_object(int value) : test_object(value) {
    }

    int get_derived_value() const {
        return get_value() * 2;
    }
};

class SharedPtrBasicTest : public ::testing::Test {
protected:
    void SetUp() override {
        test_object::reset_counters();
    }

    void TearDown() override {
        // 确保所有对象都被正确析构
        EXPECT_EQ(test_object::get_construct_count(), test_object::get_destruct_count());
    }
};

// 测试默认构造和空指针
TEST_F(SharedPtrBasicTest, DefaultConstruction) {
    mc::shared_ptr<test_object> ptr;

    EXPECT_FALSE(ptr);
    EXPECT_EQ(ptr.get(), nullptr);
    EXPECT_EQ(ptr.use_count(), 0);
    EXPECT_FALSE(ptr.unique());
}

// 测试 nullptr 构造
TEST_F(SharedPtrBasicTest, NullptrConstruction) {
    mc::shared_ptr<test_object> ptr(nullptr);

    EXPECT_FALSE(ptr);
    EXPECT_EQ(ptr.get(), nullptr);
    EXPECT_EQ(ptr.use_count(), 0);
}

// 测试 make_shared 创建
TEST_F(SharedPtrBasicTest, MakeShared) {
    auto ptr = mc::make_shared<test_object>();

    EXPECT_TRUE(ptr);
    EXPECT_NE(ptr.get(), nullptr);
    EXPECT_EQ(ptr.use_count(), 1);
    EXPECT_TRUE(ptr.unique());
    EXPECT_EQ(ptr->get_value(), 42);
    EXPECT_EQ(test_object::get_construct_count(), 1);
    EXPECT_EQ(test_object::get_destruct_count(), 0);
}

// 测试带参数的 make_shared
TEST_F(SharedPtrBasicTest, MakeSharedWithArgs) {
    auto ptr = mc::make_shared<test_object>(123);

    EXPECT_TRUE(ptr);
    EXPECT_EQ(ptr->get_value(), 123);
    EXPECT_EQ(ptr.use_count(), 1);
}

// 测试拷贝构造
TEST_F(SharedPtrBasicTest, CopyConstruction) {
    auto ptr1 = mc::make_shared<test_object>(456);
    auto ptr2(ptr1);

    EXPECT_TRUE(ptr1);
    EXPECT_TRUE(ptr2);
    EXPECT_EQ(ptr1.get(), ptr2.get());
    EXPECT_EQ(ptr1.use_count(), 2);
    EXPECT_EQ(ptr2.use_count(), 2);
    EXPECT_FALSE(ptr1.unique());
    EXPECT_FALSE(ptr2.unique());
    EXPECT_EQ(ptr1->get_value(), 456);
    EXPECT_EQ(ptr2->get_value(), 456);
}

// 测试移动构造
TEST_F(SharedPtrBasicTest, MoveConstruction) {
    auto  ptr1    = mc::make_shared<test_object>(789);
    auto* raw_ptr = ptr1.get();
    auto  ptr2(std::move(ptr1));

    EXPECT_FALSE(ptr1);
    EXPECT_TRUE(ptr2);
    EXPECT_EQ(ptr1.get(), nullptr);
    EXPECT_EQ(ptr2.get(), raw_ptr);
    EXPECT_EQ(ptr1.use_count(), 0);
    EXPECT_EQ(ptr2.use_count(), 1);
    EXPECT_TRUE(ptr2.unique());
    EXPECT_EQ(ptr2->get_value(), 789);
}

// 测试拷贝赋值
TEST_F(SharedPtrBasicTest, CopyAssignment) {
    auto ptr1 = mc::make_shared<test_object>(111);
    auto ptr2 = mc::make_shared<test_object>(222);

    EXPECT_NE(ptr1.get(), ptr2.get());
    EXPECT_EQ(ptr1->get_value(), 111);
    EXPECT_EQ(ptr2->get_value(), 222);

    ptr2 = ptr1;

    EXPECT_EQ(ptr1.get(), ptr2.get());
    EXPECT_EQ(ptr1.use_count(), 2);
    EXPECT_EQ(ptr2.use_count(), 2);
    EXPECT_EQ(ptr1->get_value(), 111);
    EXPECT_EQ(ptr2->get_value(), 111);
}

// 测试移动赋值
TEST_F(SharedPtrBasicTest, MoveAssignment) {
    auto  ptr1     = mc::make_shared<test_object>(333);
    auto  ptr2     = mc::make_shared<test_object>(444);
    auto* raw_ptr1 = ptr1.get();

    ptr2 = std::move(ptr1);

    EXPECT_FALSE(ptr1);
    EXPECT_TRUE(ptr2);
    EXPECT_EQ(ptr1.get(), nullptr);
    EXPECT_EQ(ptr2.get(), raw_ptr1);
    EXPECT_EQ(ptr1.use_count(), 0);
    EXPECT_EQ(ptr2.use_count(), 1);
    EXPECT_EQ(ptr2->get_value(), 333);
}

// 测试 reset 操作
TEST_F(SharedPtrBasicTest, Reset) {
    auto ptr = mc::make_shared<test_object>(555);

    EXPECT_TRUE(ptr);
    EXPECT_EQ(ptr.use_count(), 1);

    ptr.reset();

    EXPECT_FALSE(ptr);
    EXPECT_EQ(ptr.get(), nullptr);
    EXPECT_EQ(ptr.use_count(), 0);
}

// 测试 reset 为新对象
TEST_F(SharedPtrBasicTest, ResetWithNewObject) {
    auto ptr = mc::make_shared<test_object>(666);
    auto obj = new test_object(777);

    ptr.reset(obj);

    EXPECT_TRUE(ptr);
    EXPECT_EQ(ptr.get(), obj);
    EXPECT_EQ(ptr.use_count(), 1);
    EXPECT_EQ(ptr->get_value(), 777);
}

// 测试交换操作
TEST_F(SharedPtrBasicTest, Swap) {
    auto  ptr1     = mc::make_shared<test_object>(888);
    auto  ptr2     = mc::make_shared<test_object>(999);
    auto* raw_ptr1 = ptr1.get();
    auto* raw_ptr2 = ptr2.get();

    ptr1.swap(ptr2);

    EXPECT_EQ(ptr1.get(), raw_ptr2);
    EXPECT_EQ(ptr2.get(), raw_ptr1);
    EXPECT_EQ(ptr1->get_value(), 999);
    EXPECT_EQ(ptr2->get_value(), 888);
}

// 测试操作符重载
TEST_F(SharedPtrBasicTest, Operators) {
    auto ptr = mc::make_shared<test_object>(1010);

    // 测试 operator*
    EXPECT_EQ((*ptr).get_value(), 1010);

    // 测试 operator->
    EXPECT_EQ(ptr->get_value(), 1010);
    ptr->set_value(2020);
    EXPECT_EQ(ptr->get_value(), 2020);

    // 测试隐式转换为原始指针
    test_object* raw_ptr = ptr;
    EXPECT_EQ(raw_ptr, ptr.get());
    EXPECT_EQ(raw_ptr->get_value(), 2020);

    // 测试 operator bool
    EXPECT_TRUE(static_cast<bool>(ptr));
    ptr.reset();
    EXPECT_FALSE(static_cast<bool>(ptr));
}

// 测试类型转换构造
TEST_F(SharedPtrBasicTest, TypeConversionConstruction) {
    auto                        derived_ptr = mc::make_shared<derived_object>(150);
    mc::shared_ptr<test_object> base_ptr(derived_ptr);

    EXPECT_TRUE(base_ptr);
    EXPECT_EQ(base_ptr.get(), derived_ptr.get());
    EXPECT_EQ(base_ptr.use_count(), 2);
    EXPECT_EQ(derived_ptr.use_count(), 2);
    EXPECT_EQ(base_ptr->get_value(), 150);
}

// 测试类型转换赋值
TEST_F(SharedPtrBasicTest, TypeConversionAssignment) {
    auto                        derived_ptr = mc::make_shared<derived_object>(250);
    mc::shared_ptr<test_object> base_ptr;

    base_ptr = derived_ptr;

    EXPECT_TRUE(base_ptr);
    EXPECT_EQ(base_ptr.get(), derived_ptr.get());
    EXPECT_EQ(base_ptr.use_count(), 2);
    EXPECT_EQ(base_ptr->get_value(), 250);
}

// 测试静态类型转换
TEST_F(SharedPtrBasicTest, StaticPointerCast) {
    auto base_ptr    = mc::make_shared<derived_object>(350);
    auto derived_ptr = base_ptr.template static_pointer_cast<derived_object>();

    EXPECT_TRUE(derived_ptr);
    EXPECT_EQ(derived_ptr.get(), base_ptr.get());
    EXPECT_EQ(derived_ptr.use_count(), 2);
    EXPECT_EQ(derived_ptr->get_derived_value(), 700);
}

// 测试动态类型转换
TEST_F(SharedPtrBasicTest, DynamicPointerCast) {
    auto                        base_ptr    = mc::make_shared<derived_object>(450);
    mc::shared_ptr<test_object> test_ptr    = base_ptr;
    auto                        derived_ptr = test_ptr.template dynamic_pointer_cast<derived_object>();

    EXPECT_TRUE(derived_ptr);
    EXPECT_EQ(derived_ptr.get(), base_ptr.get());
    EXPECT_EQ(derived_ptr.use_count(), 3);
    EXPECT_EQ(derived_ptr->get_derived_value(), 900);

    // 测试失败的动态转换
    auto test_only_ptr = mc::make_shared<test_object>(550);
    auto failed_cast   = test_only_ptr.template dynamic_pointer_cast<derived_object>();
    EXPECT_FALSE(failed_cast);
    EXPECT_EQ(failed_cast.get(), nullptr);
}

// 测试引用计数边界情况
TEST_F(SharedPtrBasicTest, ReferenceCountBoundaries) {
    auto ptr = mc::make_shared<test_object>();

    // 创建大量拷贝
    std::vector<mc::shared_ptr<test_object>> ptrs;
    for (int i = 0; i < 1000; ++i) {
        ptrs.push_back(ptr);
    }

    EXPECT_EQ(ptr.use_count(), 1001);

    // 清空一半
    ptrs.erase(ptrs.begin(), ptrs.begin() + 500);
    EXPECT_EQ(ptr.use_count(), 501);

    // 清空剩余
    ptrs.clear();
    EXPECT_EQ(ptr.use_count(), 1);
    EXPECT_TRUE(ptr.unique());
}

// 测试生命周期管理
TEST_F(SharedPtrBasicTest, LifecycleManagement) {
    EXPECT_EQ(test_object::get_construct_count(), 0);
    EXPECT_EQ(test_object::get_destruct_count(), 0);

    {
        auto ptr1 = mc::make_shared<test_object>();
        EXPECT_EQ(test_object::get_construct_count(), 1);
        EXPECT_EQ(test_object::get_destruct_count(), 0);

        {
            auto ptr2 = ptr1;
            EXPECT_EQ(ptr1.use_count(), 2);
            EXPECT_EQ(test_object::get_construct_count(), 1);
            EXPECT_EQ(test_object::get_destruct_count(), 0);
        }

        EXPECT_EQ(ptr1.use_count(), 1);
        EXPECT_EQ(test_object::get_construct_count(), 1);
        EXPECT_EQ(test_object::get_destruct_count(), 0);
    }

    EXPECT_EQ(test_object::get_construct_count(), 1);
    EXPECT_EQ(test_object::get_destruct_count(), 1);
}

// 测试循环引用检测（这个测试应该展示潜在的循环引用问题）
class circular_test_a;
class circular_test_b;

class circular_test_a : public mc::enable_shared_from_this<circular_test_a> {
public:
    mc::shared_ptr<circular_test_b> m_b_ptr;

    ~circular_test_a() {
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

class circular_test_b : public mc::enable_shared_from_this<circular_test_b> {
public:
    mc::shared_ptr<circular_test_a> m_a_ptr;

    ~circular_test_b() {
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

int circular_test_a::s_destruct_count = 0;
int circular_test_b::s_destruct_count = 0;

TEST_F(SharedPtrBasicTest, CircularReferenceDetection) {
    circular_test_a::reset_counter();
    circular_test_b::reset_counter();

    {
        auto a = mc::make_shared<circular_test_a>();
        auto b = mc::make_shared<circular_test_b>();

        // 创建循环引用
        a->m_b_ptr = b;
        b->m_a_ptr = a;

        EXPECT_EQ(a.use_count(), 2);
        EXPECT_EQ(b.use_count(), 2);
    }

    // 由于循环引用，对象不会被自动销毁
    // 这是一个已知的限制，用户需要手动打破循环
    // 这个测试主要是为了文档化这种行为

    // 注意：在真实的代码中，应该使用 weak_ptr 来打破循环引用
}