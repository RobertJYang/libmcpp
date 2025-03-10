#include "mc/object.h"
#include <gtest/gtest.h>

using namespace mc;

// 测试对象创建和基本属性
TEST(ObjectTest, Creation) {
    object obj("test");
    EXPECT_EQ("test", obj.name());
    EXPECT_EQ(nullptr, obj.parent());
    EXPECT_TRUE(obj.children().empty());
}

// 测试父子关系
TEST(ObjectTest, ParentChild) {
    object parent("parent");
    object* child = new object("child", &parent);

    // 检查父子关系
    EXPECT_EQ(&parent, child->parent());
    EXPECT_EQ(1, parent.children().size());
    EXPECT_EQ(child, parent.children()[0]);

    // 测试查找子对象
    EXPECT_EQ(child, parent.find_child("child"));
    EXPECT_EQ(nullptr, parent.find_child("nonexistent"));
}

// 测试更改父对象
TEST(ObjectTest, ChangeParent) {
    object parent1("parent1");
    object parent2("parent2");
    object* child = new object("child", &parent1);

    // 检查初始父子关系
    EXPECT_EQ(&parent1, child->parent());
    EXPECT_EQ(1, parent1.children().size());
    EXPECT_EQ(0, parent2.children().size());

    // 更改父对象
    child->set_parent(&parent2);

    // 检查更改后的父子关系
    EXPECT_EQ(&parent2, child->parent());
    EXPECT_EQ(0, parent1.children().size());
    EXPECT_EQ(1, parent2.children().size());
    EXPECT_EQ(child, parent2.children()[0]);
}

// 测试递归查找子对象
TEST(ObjectTest, FindChildRecursive) {
    object root("root");
    object* child1 = new object("child1", &root);
    object* child2 = new object("child2", child1);
    object* child3 = new object("child3", child2);

    // 测试递归查找
    EXPECT_EQ(child1, root.find_child_recursive("child1"));
    EXPECT_EQ(child2, root.find_child_recursive("child2"));
    EXPECT_EQ(child3, root.find_child_recursive("child3"));
    EXPECT_EQ(nullptr, root.find_child_recursive("nonexistent"));

    // 直接子对象查找不应该找到孙子对象
    EXPECT_EQ(nullptr, root.find_child("child2"));
    EXPECT_EQ(nullptr, root.find_child("child3"));
}

// 测试对象析构时自动删除子对象
TEST(ObjectTest, Destruction) {
    object* parent = new object("parent");
    new object("child1", parent);
    object* child2 = new object("child2", parent);
    new object("grandchild", child2);

    // 验证对象树构建正确
    EXPECT_EQ(2, parent->children().size());
    EXPECT_EQ(1, child2->children().size());

    // 删除父对象应该级联删除所有子对象
    delete parent;
    // 不需要额外的断言，如果有内存问题会在运行时崩溃
    // 这个测试主要是为了确保没有内存泄漏或重复删除
}