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
#include <mc/dbus/dynamic_object.h>
#include <mc/dbus/sd_bus.h>
#include <mc/dbus/shm/shm_tree.h>
#include <mc/memory.h>
#include <mc/variant.h>
#include <test_utilities/test_base.h>

using namespace mc::dbus;

static sd_bus*                        test_bus;
static mc::shared_ptr<dynamic_object> test_object;

class DynamicObjectTest : public mc::test::TestWithDbusDaemon {
protected:
    static void SetUpTestSuite()
    {
        mc::test::TestWithDbusDaemon::SetUpTestSuite();
        // Create sd_bus instance
        test_bus = new sd_bus(true, false);
        test_bus->request_name("org.test.dynamic_object");

        // Create dynamic object
        test_object = mc::make_shared<dynamic_object>();
        test_object->set_object_path("/org/test/dynamic_object/TestObject");

        // Create and configure first interface
        auto             interface1 = mc::make_shared<dynamic_interface>("org.test.dynamic_object.Interface1");
        dynamic_property prop1{"StringProp", "s", mc::variant("initial_value"), false, 0, {}};
        dynamic_property prop2{"IntProp", "i", mc::variant(42), false, 0, {}};
        dynamic_property prop3{"BoolProp", "b", mc::variant(true), false, 0, {}};
        interface1->add_property("StringProp", std::move(prop1));
        interface1->add_property("IntProp", std::move(prop2));
        interface1->add_property("BoolProp", std::move(prop3));

        // Create and configure second interface
        auto             interface2 = mc::make_shared<dynamic_interface>("org.test.dynamic_object.Interface2");
        dynamic_property prop4{"DoubleProp", "d", mc::variant(3.14), false, 0, {}};
        dynamic_property prop5{"ReadOnlyProp", "s", mc::variant("readonly_value"), true, 0, {}};
        interface2->add_property("DoubleProp", std::move(prop4));
        interface2->add_property("ReadOnlyProp", std::move(prop5));

        // Create and configure third interface
        auto             interface3 = mc::make_shared<dynamic_interface>("org.test.dynamic_object.Interface3");
        dynamic_property prop6{"ArrayProp", "ai", mc::variant(mc::variants{1, 2, 3, 4}), false, 0, {}};
        interface3->add_property("ArrayProp", std::move(prop6));

        // Add interfaces to object
        test_object->add_interface(interface1);
        test_object->add_interface(interface2);
        test_object->add_interface(interface3);

        // Register object with sd_bus
        test_bus->register_object(test_object);
    }

    static void TearDownTestSuite()
    {
        delete test_bus;
        test_object.reset();
        mc::test::TestWithDbusDaemon::TearDownTestSuite();
    }

    void SetUp() override
    {
        // Reset properties to initial values before each test to ensure independence
        test_object->set_property("StringProp", mc::variant("initial_value"), "org.test.dynamic_object.Interface1");
        test_object->set_property("IntProp", mc::variant(42), "org.test.dynamic_object.Interface1");
        test_object->set_property("BoolProp", mc::variant(true), "org.test.dynamic_object.Interface1");
        test_object->set_property("ArrayProp", mc::variant(mc::variants{1, 2, 3, 4}),
                                  "org.test.dynamic_object.Interface3");
    }
};

// Test get_property with valid property
TEST_F(DynamicObjectTest, test_get_property_valid)
{
    // Test string property
    auto value1 = test_object->get_property("StringProp", "org.test.dynamic_object.Interface1");
    ASSERT_TRUE(value1.is_string());
    EXPECT_EQ(value1.as_string(), "initial_value");

    // Test int property
    auto value2 = test_object->get_property("IntProp", "org.test.dynamic_object.Interface1");
    ASSERT_TRUE(value2.is_int32());
    EXPECT_EQ(value2.as_int32(), 42);

    // Test bool property
    auto value3 = test_object->get_property("BoolProp", "org.test.dynamic_object.Interface1");
    ASSERT_TRUE(value3.is_bool());
    EXPECT_EQ(value3.as_bool(), true);

    // Test double property from second interface
    auto value4 = test_object->get_property("DoubleProp", "org.test.dynamic_object.Interface2");
    ASSERT_TRUE(value4.is_double());
    EXPECT_DOUBLE_EQ(value4.as_double(), 3.14);
}

// Test get_property with non-existent property
TEST_F(DynamicObjectTest, test_get_property_nonexistent)
{
    auto value = test_object->get_property("NonExistentProp", "org.test.dynamic_object.Interface1");
    ASSERT_TRUE(value.is_null());
}

// Test get_property with non-existent interface
TEST_F(DynamicObjectTest, test_get_property_nonexistent_interface)
{
    auto value = test_object->get_property("StringProp", "org.test.dynamic_object.NonExistentInterface");
    ASSERT_TRUE(value.is_null());
}

// Test set_property with valid property
TEST_F(DynamicObjectTest, test_set_property_valid)
{
    // Set string property
    bool ret1 =
        test_object->set_property("StringProp", mc::variant("new_string_value"), "org.test.dynamic_object.Interface1");
    EXPECT_TRUE(ret1);
    auto value1 = test_object->get_property("StringProp", "org.test.dynamic_object.Interface1");
    ASSERT_TRUE(value1.is_string());
    EXPECT_EQ(value1.as_string(), "new_string_value");

    // Set int property
    bool ret2 = test_object->set_property("IntProp", mc::variant(100), "org.test.dynamic_object.Interface1");
    EXPECT_TRUE(ret2);
    auto value2 = test_object->get_property("IntProp", "org.test.dynamic_object.Interface1");
    ASSERT_TRUE(value2.is_int32());
    EXPECT_EQ(value2.as_int32(), 100);

    // Set bool property
    bool ret3 = test_object->set_property("BoolProp", mc::variant(false), "org.test.dynamic_object.Interface1");
    EXPECT_TRUE(ret3);
    auto value3 = test_object->get_property("BoolProp", "org.test.dynamic_object.Interface1");
    ASSERT_TRUE(value3.is_bool());
    EXPECT_EQ(value3.as_bool(), false);
}

// Test set_property with non-existent property
TEST_F(DynamicObjectTest, test_set_property_nonexistent)
{
    bool ret = test_object->set_property("NonExistentProp", mc::variant("value"), "org.test.dynamic_object.Interface1");
    EXPECT_FALSE(ret);
}

// Test set_property with non-existent interface
TEST_F(DynamicObjectTest, test_set_property_nonexistent_interface)
{
    bool ret =
        test_object->set_property("StringProp", mc::variant("value"), "org.test.dynamic_object.NonExistentInterface");
    EXPECT_FALSE(ret);
}

// Test set_property with readonly property
TEST_F(DynamicObjectTest, test_set_property_readonly)
{
    EXPECT_THROW(
        test_object->set_property("ReadOnlyProp", mc::variant("new_value"), "org.test.dynamic_object.Interface2"),
        mc::exception);
}

// Test try_get_property with valid property
TEST_F(DynamicObjectTest, test_try_get_property_valid)
{
    auto [ret_code, value] = test_object->try_get_property("StringProp", "org.test.dynamic_object.Interface1");
    EXPECT_EQ(ret_code, static_cast<int>(access_property_rsp_code::success));
    ASSERT_TRUE(value.is_string());
    EXPECT_EQ(value.as_string(), "initial_value");

    auto [ret_code2, value2] = test_object->try_get_property("IntProp", "org.test.dynamic_object.Interface1");
    EXPECT_EQ(ret_code2, static_cast<int>(access_property_rsp_code::success));
    ASSERT_TRUE(value2.is_int32());
    EXPECT_EQ(value2.as_int32(), 42);
}

// Test try_get_property with non-existent property
TEST_F(DynamicObjectTest, test_try_get_property_nonexistent)
{
    auto [ret_code, value] = test_object->try_get_property("NonExistentProp", "org.test.dynamic_object.Interface1");
    EXPECT_EQ(ret_code, static_cast<int>(access_property_rsp_code::property_not_exist_err));
    ASSERT_TRUE(value.is_null());
}

// Test try_get_property with non-existent interface
TEST_F(DynamicObjectTest, test_try_get_property_nonexistent_interface)
{
    auto [ret_code, value] =
        test_object->try_get_property("StringProp", "org.test.dynamic_object.NonExistentInterface");
    EXPECT_EQ(ret_code, static_cast<int>(access_property_rsp_code::interface_not_exist_err));
    ASSERT_TRUE(value.is_null());
}

// Test try_set_property with valid property
TEST_F(DynamicObjectTest, test_try_set_property_valid)
{
    int ret_code =
        test_object->try_set_property("StringProp", mc::variant("try_set_value"), "org.test.dynamic_object.Interface1");
    EXPECT_EQ(ret_code, static_cast<int>(access_property_rsp_code::success));

    auto [ret_code2, value] = test_object->try_get_property("StringProp", "org.test.dynamic_object.Interface1");
    EXPECT_EQ(ret_code2, static_cast<int>(access_property_rsp_code::success));
    ASSERT_TRUE(value.is_string());
    EXPECT_EQ(value.as_string(), "try_set_value");
}

// Test try_set_property with non-existent property
TEST_F(DynamicObjectTest, test_try_set_property_nonexistent)
{
    int ret_code =
        test_object->try_set_property("NonExistentProp", mc::variant("value"), "org.test.dynamic_object.Interface1");
    EXPECT_EQ(ret_code, static_cast<int>(access_property_rsp_code::property_not_exist_err));
}

// Test try_set_property with non-existent interface
TEST_F(DynamicObjectTest, test_try_set_property_nonexistent_interface)
{
    int ret_code = test_object->try_set_property("StringProp", mc::variant("value"),
                                                 "org.test.dynamic_object.NonExistentInterface");
    EXPECT_EQ(ret_code, static_cast<int>(access_property_rsp_code::interface_not_exist_err));
}

// Test try_set_property with readonly property
TEST_F(DynamicObjectTest, test_try_set_property_readonly)
{
    int ret_code =
        test_object->try_set_property("ReadOnlyProp", mc::variant("new_value"), "org.test.dynamic_object.Interface2");
    EXPECT_EQ(ret_code, static_cast<int>(access_property_rsp_code::set_property_unknown_err));
}

// Test multiple interfaces with same property name
TEST_F(DynamicObjectTest, test_multiple_interfaces_same_property_name)
{
    // Add a property with same name to another interface
    auto             interface4 = mc::make_shared<dynamic_interface>("org.test.dynamic_object.Interface4");
    dynamic_property prop{"CommonProp", "s", mc::variant("interface4_value"), false, 0, {}};
    interface4->add_property("CommonProp", std::move(prop));
    test_object->add_interface(interface4);

    // Get property from Interface1
    auto value1 = test_object->get_property("StringProp", "org.test.dynamic_object.Interface1");
    EXPECT_EQ(value1.as_string(), "initial_value");

    // Get property from Interface4
    auto value4 = test_object->get_property("CommonProp", "org.test.dynamic_object.Interface4");
    EXPECT_EQ(value4.as_string(), "interface4_value");
}

// Test array property
TEST_F(DynamicObjectTest, test_array_property)
{
    auto value = test_object->get_property("ArrayProp", "org.test.dynamic_object.Interface3");
    ASSERT_TRUE(value.is_array());
    auto arr = value.as_array();
    EXPECT_EQ(arr.size(), 4u);
    EXPECT_EQ(arr[0].as_int32(), 1);
    EXPECT_EQ(arr[1].as_int32(), 2);
    EXPECT_EQ(arr[2].as_int32(), 3);
    EXPECT_EQ(arr[3].as_int32(), 4);

    // Set array property
    mc::variants new_array{10, 20, 30};
    bool ret = test_object->set_property("ArrayProp", mc::variant(new_array), "org.test.dynamic_object.Interface3");
    EXPECT_TRUE(ret);

    auto value2 = test_object->get_property("ArrayProp", "org.test.dynamic_object.Interface3");
    ASSERT_TRUE(value2.is_array());
    auto arr2 = value2.as_array();
    EXPECT_EQ(arr2.size(), 3u);
    EXPECT_EQ(arr2[0].as_int32(), 10);
    EXPECT_EQ(arr2[1].as_int32(), 20);
    EXPECT_EQ(arr2[2].as_int32(), 30);
}

// Test has_property
TEST_F(DynamicObjectTest, test_has_property)
{
    EXPECT_TRUE(test_object->has_property("StringProp", "org.test.dynamic_object.Interface1"));
    EXPECT_TRUE(test_object->has_property("IntProp", "org.test.dynamic_object.Interface1"));
    EXPECT_FALSE(test_object->has_property("NonExistentProp", "org.test.dynamic_object.Interface1"));
    EXPECT_FALSE(test_object->has_property("StringProp", "org.test.dynamic_object.NonExistentInterface"));
}

// Test get_all_properties
TEST_F(DynamicObjectTest, test_get_all_properties)
{
    auto all_props = test_object->get_all_properties("org.test.dynamic_object.Interface1");
    EXPECT_EQ(all_props.size(), 3u);
    EXPECT_TRUE(all_props.contains("StringProp"));
    EXPECT_TRUE(all_props.contains("IntProp"));
    EXPECT_TRUE(all_props.contains("BoolProp"));

    auto empty_props = test_object->get_all_properties("org.test.dynamic_object.NonExistentInterface");
    EXPECT_EQ(empty_props.size(), 0u);
}

#if defined(ENABLE_CONAN_COMPILE) && ENABLE_CONAN_COMPILE == 1
// Test update_shm_prop
TEST_F(DynamicObjectTest, test_update_shm_prop)
{
    test_object->update_shm_prop("StringProp", mc::variant("test_update_shm_prop"),
                                 "org.test.dynamic_object.Interface1");
    auto value_shm = shm_tree::get_property("org.test.dynamic_object", "/org/test/dynamic_object/TestObject",
                                            "org.test.dynamic_object.Interface1", "StringProp");
    EXPECT_EQ(value_shm.as_string(), "test_update_shm_prop");

    test_object->update_shm_prop("IntProp", mc::variant(69), "org.test.dynamic_object.Interface1");
    auto value_shm2 = shm_tree::get_property("org.test.dynamic_object", "/org/test/dynamic_object/TestObject",
                                             "org.test.dynamic_object.Interface1", "IntProp");
    EXPECT_EQ(value_shm2.as_int32(), 69);

    test_object->update_shm_prop("BoolProp", mc::variant(false), "org.test.dynamic_object.Interface1");
    auto value_shm3 = shm_tree::get_property("org.test.dynamic_object", "/org/test/dynamic_object/TestObject",
                                             "org.test.dynamic_object.Interface1", "BoolProp");
    EXPECT_EQ(value_shm3.as_bool(), false);

    test_object->update_shm_prop("ArrayProp", mc::variant(mc::variants{5, 6, 7, 8}),
                                 "org.test.dynamic_object.Interface3");
    auto value_shm4 = shm_tree::get_property("org.test.dynamic_object", "/org/test/dynamic_object/TestObject",
                                             "org.test.dynamic_object.Interface3", "ArrayProp");
    EXPECT_EQ(value_shm4.as_array().size(), 4u);
    EXPECT_EQ(value_shm4.as_array()[0].as_int32(), 5);
    EXPECT_EQ(value_shm4.as_array()[1].as_int32(), 6);
    EXPECT_EQ(value_shm4.as_array()[2].as_int32(), 7);
    EXPECT_EQ(value_shm4.as_array()[3].as_int32(), 8);
}
#endif