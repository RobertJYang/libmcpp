/*
 * Copyright (c) 2023 Huawei Technologies Co., Ltd.
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
#include <mc/database/client.h>
#include <mc/database/object.h>  // 添加对象定义
#include <mc/database/property.h>  // 添加属性定义
#include <mc/reflect.h>
#include <string>

namespace mc {
namespace database {

// 前置声明
template<typename T>
class db_object;

// 测试对象定义
class test_object : public db_object<test_object> {
public:
    test_object() {
        // 初始化属性，建立与db_object的关联
        init_properties();
    }
    
    virtual ~test_object() = default;
    
    property<int> value;
    property<std::string> name;
};

} // namespace database
} // namespace mc

// 添加反射支持
MC_REFLECT(mc::database::test_object, (value)(name))

// 放回命名空间，以便简化代码
namespace mc {
namespace database {

TEST(DatabaseTest, ClientConnection) {
    client client;
    
    // 测试连接
    error_code ec = client.connect();
    EXPECT_EQ(ec, error_code::success);
    EXPECT_TRUE(client.is_connected());
    
    // 测试断开连接
    ec = client.disconnect();
    EXPECT_EQ(ec, error_code::success);
    EXPECT_FALSE(client.is_connected());
}

TEST(DatabaseTest, ObjectCreateAndQuery) {
    client client;
    
    // 连接
    client.connect();
    ASSERT_TRUE(client.is_connected());
    
    // 创建对象
    error_code ec = client.create<test_object>("/test/obj1");
    EXPECT_EQ(ec, error_code::success);
    
    // 重复创建应该失败
    ec = client.create<test_object>("/test/obj1");
    EXPECT_EQ(ec, error_code::already_exists);
    
    // 查询对象
    auto obj = client.query<test_object>("/test/obj1", ec);
    EXPECT_EQ(ec, error_code::success);
    EXPECT_TRUE(obj != nullptr);
    
    // 设置和获取属性
    obj->value = 42;
    obj->name = "测试对象";
    
    EXPECT_EQ(obj->value.get(), 42);
    EXPECT_EQ(obj->name.get(), "测试对象");
    
    // 查询不存在的对象
    auto missing_obj = client.query<test_object>("/test/missing", ec);
    EXPECT_EQ(ec, error_code::not_found);
    EXPECT_TRUE(missing_obj == nullptr);
    
    // 断开连接
    client.disconnect();
}

TEST(DatabaseTest, ObjectRemoval) {
    client client;
    
    // 连接
    client.connect();
    ASSERT_TRUE(client.is_connected());
    
    // 创建对象
    error_code ec = client.create<test_object>("/test/obj2");
    EXPECT_EQ(ec, error_code::success);
    
    // 检查对象是否存在
    bool exists = client.exists("/test/obj2", ec);
    EXPECT_EQ(ec, error_code::success);
    EXPECT_TRUE(exists);
    
    // 删除对象
    ec = client.remove("/test/obj2");
    EXPECT_EQ(ec, error_code::success);
    
    // 检查对象是否已经删除
    exists = client.exists("/test/obj2", ec);
    EXPECT_EQ(ec, error_code::success);
    EXPECT_FALSE(exists);
    
    // 断开连接
    client.disconnect();
}

TEST(DatabaseTest, ChildObjects) {
    client client;
    
    // 连接
    client.connect();
    ASSERT_TRUE(client.is_connected());
    
    // 创建多个对象
    error_code ec = client.create<test_object>("/test/parent/obj1");
    EXPECT_EQ(ec, error_code::success);
    
    ec = client.create<test_object>("/test/parent/obj2");
    EXPECT_EQ(ec, error_code::success);
    
    ec = client.create<test_object>("/test/parent/obj3");
    EXPECT_EQ(ec, error_code::success);
    
    // 列出子对象
    auto children = client.list_children("/test/parent", ec);
    EXPECT_EQ(ec, error_code::success);
    EXPECT_EQ(children.size(), 3);
    
    // 检查子对象名称
    EXPECT_TRUE(std::find(children.begin(), children.end(), "obj1") != children.end());
    EXPECT_TRUE(std::find(children.begin(), children.end(), "obj2") != children.end());
    EXPECT_TRUE(std::find(children.begin(), children.end(), "obj3") != children.end());
    
    // 断开连接
    client.disconnect();
}

}} // namespace mc::database
