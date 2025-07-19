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

#include <gtest/gtest.h>
#include <mc/reflect/reflect.h>
#include <mc/reflect/reflection_factory.h>
#include <mc/singleton.h>
#include <test_utilities/test_base.h>

namespace test_reflection_namespace {

// 定义测试用的命名空间
struct mc_devices_namespace {
    constexpr static std::string_view factory_name = "mc.devices";
};

struct mc_devices_sensors_namespace {
    constexpr static std::string_view factory_name = "sensors"; // 不带命名空间前缀，这样可以注册到任何命名空间之下
};

struct mc_drivers_namespace {
    constexpr static std::string_view factory_name = "mc.drivers";
};

struct mc_duplicate_devices_namespace {
    constexpr static std::string_view factory_name = "mc.devices"; // 与 mc_devices_namespace 同名
};

// 测试类型
class device_sensor {
public:
    std::string m_name;
    double      m_value;
};

class temperature_sensor {
public:
    std::string m_name;
    double      m_temperature;
    std::string m_unit;
};

class driver_base {
public:
    std::string m_name;
    int         m_version;
};

// 测试枚举
enum class sensor_status {
    ACTIVE,
    INACTIVE,
    ERROR
};

// 没有指定命名空间的类型（应该注册失败）
class invalid_type_for_devices {
public:
    std::string m_name;
};

// 错误命名空间前缀的类型
class wrong_prefix_type {
public:
    std::string m_name;
};

} // namespace test_reflection_namespace

// 正确的命名空间注册
MC_REFLECT_WITH_NAMESPACE(
    test_reflection_namespace::mc_devices_namespace,
    (test_reflection_namespace::device_sensor, "mc.devices.Sensor"),
    ((m_name, "name"))((m_value, "value")))

MC_REFLECT_WITH_NAMESPACE(
    test_reflection_namespace::mc_devices_sensors_namespace,
    (test_reflection_namespace::temperature_sensor, "sensors::TemperatureSensor"),
    ((m_name, "name"))((m_temperature, "temperature"))((m_unit, "unit")))

MC_REFLECT_WITH_NAMESPACE(
    test_reflection_namespace::mc_drivers_namespace,
    (test_reflection_namespace::driver_base, "mc.drivers.DriverBase"),
    ((m_name, "name"))((m_version, "version")))

// 枚举注册到 mc.devices 命名空间下
MC_REFLECT_ENUM_WITH_NAMESPACE(
    test_reflection_namespace::mc_devices_namespace,
    (test_reflection_namespace::sensor_status, "mc.devices.SensorStatus"),
    (ACTIVE)(INACTIVE)(ERROR))

// 错误的命名空间注册（这些应该导致注册失败）
MC_REFLECT((test_reflection_namespace::invalid_type_for_devices, "mc.devices.InvalidType"),
           ((m_name, "name")))

MC_REFLECT((test_reflection_namespace::wrong_prefix_type, "wrong.prefix.Type"),
           ((m_name, "name")))

namespace test_reflection_namespace {

using test_types = std::tuple<
    device_sensor,
    temperature_sensor,
    driver_base,
    sensor_status,
    invalid_type_for_devices,
    wrong_prefix_type>;

class reflection_factory_advanced_test : public mc::test::TestBase {
protected:
    void SetUp() override {
        // 重置工厂单例，确保每个测试开始时都有干净的工厂状态
        mc::singleton<mc::reflect::factory_ptr, mc_devices_namespace>::reset_for_test();
        mc::singleton<mc::reflect::factory_ptr, mc_devices_sensors_namespace>::reset_for_test();
        mc::singleton<mc::reflect::factory_ptr, mc_drivers_namespace>::reset_for_test();

        // 先注销后注册再重新注册，确保所有类型都重新注册到对应的工厂
        mc::traits::tuple_element_for_each<test_types>([](auto&& type_wrap) {
            using type = typename std::decay_t<decltype(type_wrap)>::type;
            mc::reflect::reflector<type>::unregister_type();
        });
        mc::traits::tuple_element_for_each<test_types>([](auto&& type_wrap) {
            using type = std::tuple_element_t<std::decay_t<decltype(type_wrap)>::index, test_types>;
            mc::reflect::reflector<type>::register_type();
        });
        mc::log::default_logger().set_level(mc::log::level::error);
    }

    void TearDown() override {
        // 清理测试单例
        mc::singleton<mc::reflect::factory_ptr, mc_devices_namespace>::reset_for_test();
        mc::singleton<mc::reflect::factory_ptr, mc_devices_sensors_namespace>::reset_for_test();
        mc::singleton<mc::reflect::factory_ptr, mc_drivers_namespace>::reset_for_test();

        // 清理所有的测试类型
        mc::traits::tuple_element_for_each<test_types>([](auto&& type_wrap) {
            using type = typename std::decay_t<decltype(type_wrap)>::type;
            mc::reflect::reflector<type>::unregister_type();
        });
    }
};

// 测试工厂名称验证
TEST_F(reflection_factory_advanced_test, FactoryNameValidation) {
    // 有效的工厂名称
    EXPECT_TRUE(mc::reflect::is_valid_namespace("devices"));
    EXPECT_TRUE(mc::reflect::is_valid_namespace("mc.devices"));
    EXPECT_TRUE(mc::reflect::is_valid_namespace("mc.devices.sensors"));
    EXPECT_TRUE(mc::reflect::is_valid_namespace("_private"));
    EXPECT_TRUE(mc::reflect::is_valid_namespace("module_1.sub_2"));

    // 空字符串不允许用户使用（仅保留给全局工厂）
    EXPECT_FALSE(mc::reflect::is_valid_namespace(""));

    // 无效的工厂名称
    EXPECT_FALSE(mc::reflect::is_valid_namespace(".invalid"));        // 以点开头
    EXPECT_FALSE(mc::reflect::is_valid_namespace("invalid."));        // 以点结尾
    EXPECT_FALSE(mc::reflect::is_valid_namespace("invalid..double")); // 连续点号
    EXPECT_FALSE(mc::reflect::is_valid_namespace("123invalid"));      // 数字开头
    EXPECT_FALSE(mc::reflect::is_valid_namespace("invalid.123sub"));  // 段以数字开头
    EXPECT_FALSE(mc::reflect::is_valid_namespace("invalid-name"));    // 包含非法字符

    // 超长名称
    std::string long_name(mc::reflect::max_type_name_length + 1, 'x');
    EXPECT_FALSE(mc::reflect::is_valid_namespace(long_name));
}

// 测试自定义命名空间工厂创建
TEST_F(reflection_factory_advanced_test, CustomNamespaceFactory) {
    // 获取自定义命名空间工厂
    auto& devices_factory = mc::reflect::reflection_factory::instance<mc_devices_namespace>();
    auto& sensors_factory = mc::reflect::reflection_factory::instance<mc_devices_sensors_namespace>();
    auto& drivers_factory = mc::reflect::reflection_factory::instance<mc_drivers_namespace>();

    // 验证工厂名称
    EXPECT_EQ(devices_factory.get_factory_name(), "mc.devices");
    EXPECT_EQ(sensors_factory.get_factory_name(), "sensors");
    EXPECT_EQ(drivers_factory.get_factory_name(), "mc.drivers");

    // 验证单例性
    auto& devices_factory2 = mc::reflect::reflection_factory::instance<mc_devices_namespace>();
    EXPECT_EQ(&devices_factory, &devices_factory2);
}

// 测试命名空间要求验证
TEST_F(reflection_factory_advanced_test, NamespaceRequirement) {
    auto& devices_factory = mc::reflect::reflection_factory::instance<mc_devices_namespace>();

    // 正确的命名空间注册应该成功
    auto device_obj = devices_factory.try_create_object("Sensor");
    EXPECT_NE(device_obj, nullptr);

    auto status_type_id = devices_factory.get_type_id("SensorStatus");
    EXPECT_NE(status_type_id, mc::reflect::INVALID_TYPE_ID);

    // 测试通过完整名称访问
    auto device_obj2 = devices_factory.try_create_object("mc.devices.Sensor");
    EXPECT_NE(device_obj2, nullptr);

    // 错误的命名空间前缀应该无法访问
    auto invalid_obj = devices_factory.try_create_object("wrong.prefix.Type");
    EXPECT_EQ(invalid_obj, nullptr);

    // 没有正确命名空间前缀的类型应该无法注册或访问
    auto invalid_type_id = devices_factory.get_type_id("InvalidType");
    EXPECT_EQ(invalid_type_id, mc::reflect::INVALID_TYPE_ID);
}

// 测试工厂注册到另一个工厂
TEST_F(reflection_factory_advanced_test, FactoryRegistration) {
    auto& global_factory  = mc::reflect::reflection_factory::global();
    auto& devices_factory = mc::reflect::reflection_factory::instance<mc_devices_namespace>();
    auto& sensors_factory = mc::reflect::reflection_factory::instance<mc_devices_sensors_namespace>();

    // 将 devices 工厂注册到全局工厂
    auto devices_id = global_factory.register_factory(
        mc::reflect::reflection_factory::instance_ptr<mc_devices_namespace>());
    EXPECT_NE(devices_id, mc::reflect::INVALID_FACTORY_ID);

    // 将 sensors 工厂注册到 devices 工厂
    auto sensors_id = devices_factory.register_factory(
        mc::reflect::reflection_factory::instance_ptr<mc_devices_sensors_namespace>());
    EXPECT_NE(sensors_id, mc::reflect::INVALID_FACTORY_ID);

    // 验证工厂可以被找到
    auto found_devices = global_factory.get_factory("mc.devices");
    EXPECT_NE(found_devices, nullptr);
    EXPECT_EQ(found_devices.get(), &devices_factory);

    auto found_sensors = devices_factory.get_factory("sensors");
    EXPECT_NE(found_sensors, nullptr);
    EXPECT_EQ(found_sensors.get(), &sensors_factory);

    auto found_global_sensors = global_factory.get_factory("mc.devices.sensors");
    ASSERT_NE(found_global_sensors, nullptr);
    EXPECT_EQ(found_global_sensors.get(), &sensors_factory);

    // 测试工厂名称列表
    auto factory_names      = global_factory.get_factory_names();
    bool found_devices_name = std::find(
                                  factory_names.begin(),
                                  factory_names.end(),
                                  "mc.devices") != factory_names.end();
    EXPECT_TRUE(found_devices_name);

    bool found_sensors_name = std::find(
                                  factory_names.begin(),
                                  factory_names.end(),
                                  "mc.devices.sensors") != factory_names.end();
    EXPECT_TRUE(found_sensors_name);

    auto factory_names2 = devices_factory.get_factory_names();
    ASSERT_EQ(factory_names2.size(), 1);
    EXPECT_EQ(factory_names2[0], "mc.devices.sensors");
}

// 测试工厂注册命名要求
TEST_F(reflection_factory_advanced_test, FactoryRegistrationNamingRequirement) {
    auto& global_factory  = mc::reflect::reflection_factory::global();
    auto& devices_factory = mc::reflect::reflection_factory::instance<mc_devices_namespace>();
    auto& drivers_factory = mc::reflect::reflection_factory::instance<mc_drivers_namespace>();

    // 正确的命名空间前缀注册应该成功
    auto devices_id = global_factory.register_factory(
        mc::reflect::reflection_factory::instance_ptr<mc_devices_namespace>());
    EXPECT_NE(devices_id, mc::reflect::INVALID_FACTORY_ID);

    // 尝试将 drivers 工厂注册到 devices 工厂应该失败（不匹配前缀）
    auto invalid_id = devices_factory.register_factory(
        mc::reflect::reflection_factory::instance_ptr<mc_drivers_namespace>());
    EXPECT_EQ(invalid_id, mc::reflect::INVALID_FACTORY_ID);

    // 同一个工厂重复注册可以成功
    auto duplicate_id = global_factory.register_factory(
        mc::reflect::reflection_factory::instance_ptr<mc_devices_namespace>());
    EXPECT_EQ(duplicate_id, devices_id);

    // 重复注册另一个同名的工厂应该失败
    auto duplicate_id2 = global_factory.register_factory(
        mc::reflect::reflection_factory::instance_ptr<mc_duplicate_devices_namespace>());
    EXPECT_EQ(duplicate_id2, mc::reflect::INVALID_FACTORY_ID);
}

// 测试跨工厂对象创建
TEST_F(reflection_factory_advanced_test, CrossFactoryObjectCreation) {
    auto& global_factory  = mc::reflect::reflection_factory::global();
    auto& devices_factory = mc::reflect::reflection_factory::instance<mc_devices_namespace>();
    auto& sensors_factory = mc::reflect::reflection_factory::instance<mc_devices_sensors_namespace>();

    // 注册工厂层次结构
    global_factory.register_factory(
        mc::reflect::reflection_factory::instance_ptr<mc_devices_namespace>());
    devices_factory.register_factory(
        mc::reflect::reflection_factory::instance_ptr<mc_devices_sensors_namespace>());

    // 通过全局工厂访问设备类型
    auto device_obj = global_factory.try_create_object("mc.devices.Sensor");
    EXPECT_NE(device_obj, nullptr);

    // 通过设备工厂访问传感器类型
    auto sensor_obj = devices_factory.try_create_object("sensors.TemperatureSensor");
    EXPECT_NE(sensor_obj, nullptr);

    // 通过传感器工厂直接访问
    auto sensor_obj2 = sensors_factory.try_create_object("TemperatureSensor");
    EXPECT_NE(sensor_obj2, nullptr);

    // 验证对象属性
    sensor_obj2->set_property("name", mc::variant("CPU温度传感器"));
    sensor_obj2->set_property("temperature", mc::variant(45.5));
    sensor_obj2->set_property("unit", mc::variant("°C"));

    EXPECT_EQ(sensor_obj2->get_property("name"), mc::variant("CPU温度传感器"));
    EXPECT_EQ(sensor_obj2->get_property("temperature"), mc::variant(45.5));
    EXPECT_EQ(sensor_obj2->get_property("unit"), mc::variant("°C"));
}

// 测试枚举元数据自动注销
TEST_F(reflection_factory_advanced_test, EnumMetadataAutoUnregister) {
    auto& devices_factory = mc::reflect::reflection_factory::instance<mc_devices_namespace>();

    // 验证枚举类型存在
    auto status_type_id = devices_factory.get_type_id("SensorStatus");
    EXPECT_NE(status_type_id, mc::reflect::INVALID_TYPE_ID);

    // 获取枚举元数据以确保单例存在
    auto metadata = devices_factory.get_metadata(status_type_id);
    EXPECT_NE(metadata, nullptr);

    // 销毁枚举元数据单例
    using meta_singleton_type = mc::singleton<
        mc::reflect::reflection_enum<sensor_status>::reflection_ptr>;
    auto* meta_ptr = meta_singleton_type::try_get();
    EXPECT_NE(meta_ptr, nullptr);
    mc::reflect::reflector<sensor_status>::unregister_type();
    meta_ptr = meta_singleton_type::try_get();
    EXPECT_EQ(meta_ptr, nullptr);

    // 验证类型已被自动注销
    auto invalid_type_id = devices_factory.get_type_id("SensorStatus");
    EXPECT_EQ(invalid_type_id, mc::reflect::INVALID_TYPE_ID);

    auto invalid_metadata = devices_factory.get_metadata(status_type_id);
    EXPECT_EQ(invalid_metadata, nullptr);
}

// 测试子工厂销毁时自动注销
TEST_F(reflection_factory_advanced_test, SubFactoryAutoUnregister) {
    auto& devices_factory = mc::reflect::reflection_factory::instance<mc_devices_namespace>();

    // 创建一个新的传感器工厂实例并注册
    {
        auto sensors_factory_ptr = mc::reflect::reflection_factory::instance_ptr<
            mc_devices_sensors_namespace>();
        auto sensors_id = devices_factory.register_factory(sensors_factory_ptr);
        EXPECT_NE(sensors_id, mc::reflect::INVALID_FACTORY_ID);

        // 验证工厂已注册
        auto found_sensors = devices_factory.get_factory("sensors");
        EXPECT_NE(found_sensors, nullptr);

        // 重置单例，模拟工厂销毁
        mc::singleton<mc::reflect::factory_ptr, mc_devices_sensors_namespace>::reset_for_test();
    }

    // 验证工厂已被自动注销
    auto not_found_sensors = devices_factory.get_factory("sensors");
    EXPECT_EQ(not_found_sensors, nullptr);
}

// 测试工厂层次结构和模块路径
TEST_F(reflection_factory_advanced_test, FactoryHierarchyAndModulePaths) {
    auto& global_factory  = mc::reflect::reflection_factory::global();
    auto& devices_factory = mc::reflect::reflection_factory::instance<mc_devices_namespace>();
    auto& sensors_factory = mc::reflect::reflection_factory::instance<mc_devices_sensors_namespace>();

    // 建立工厂层次结构
    global_factory.register_factory(
        mc::reflect::reflection_factory::instance_ptr<mc_devices_namespace>());
    devices_factory.register_factory(
        mc::reflect::reflection_factory::instance_ptr<mc_devices_sensors_namespace>());

    // 验证全局工厂的模块路径
    auto global_paths     = global_factory.get_module_paths();
    bool has_devices_path = std::find(
                                global_paths.begin(),
                                global_paths.end(),
                                "mc.devices") != global_paths.end();
    EXPECT_TRUE(has_devices_path);

    // 验证设备工厂的模块路径
    auto devices_paths    = devices_factory.get_module_paths();
    bool has_sensors_path = std::find(
                                devices_paths.begin(),
                                devices_paths.end(),
                                "mc.devices.sensors") != devices_paths.end();
    EXPECT_TRUE(has_sensors_path);

    // 验证模块存在性
    EXPECT_TRUE(global_factory.has_module("mc.devices"));
    EXPECT_TRUE(devices_factory.has_module("mc.devices.sensors"));
    EXPECT_FALSE(devices_factory.has_module("nonexistent"));

    // 验证跨层级类型访问
    auto registered_types  = global_factory.get_registered_types();
    bool has_device_sensor = std::find(registered_types.begin(),
                                       registered_types.end(),
                                       "mc.devices.Sensor") != registered_types.end();
    bool has_temp_sensor   = std::find(registered_types.begin(),
                                       registered_types.end(),
                                       "mc.devices.sensors.TemperatureSensor") != registered_types.end();

    EXPECT_TRUE(has_device_sensor);
    EXPECT_TRUE(has_temp_sensor);
}

// 测试工厂注销
TEST_F(reflection_factory_advanced_test, FactoryUnregistration) {
    auto& global_factory  = mc::reflect::reflection_factory::global();
    auto& devices_factory = mc::reflect::reflection_factory::instance<mc_devices_namespace>();

    // 注册工厂
    auto devices_id = global_factory.register_factory(
        mc::reflect::reflection_factory::instance_ptr<mc_devices_namespace>());
    EXPECT_NE(devices_id, mc::reflect::INVALID_FACTORY_ID);

    // 验证工厂存在
    auto found_factory = global_factory.get_factory("mc.devices");
    EXPECT_NE(found_factory, nullptr);

    // 注销工厂
    global_factory.unregister_factory("mc.devices");

    // 验证工厂已被注销
    auto not_found_factory = global_factory.get_factory("mc.devices");
    EXPECT_EQ(not_found_factory, nullptr);

    // 验证工厂名称列表中不再包含该工厂
    auto factory_names      = global_factory.get_factory_names();
    bool found_devices_name = std::find(
                                  factory_names.begin(),
                                  factory_names.end(),
                                  "mc.devices") != factory_names.end();
    EXPECT_FALSE(found_devices_name);
}

// 测试错误情况处理
TEST_F(reflection_factory_advanced_test, ErrorHandling) {
    auto& global_factory = mc::reflect::reflection_factory::global();

    // 测试注册空名称工厂
    EXPECT_EQ(global_factory.register_factory(nullptr), mc::reflect::INVALID_FACTORY_ID);

    // 测试获取不存在的工厂
    auto not_found = global_factory.get_factory("nonexistent.factory");
    EXPECT_EQ(not_found, nullptr);

    // 测试注销不存在的工厂（应该不会崩溃）
    global_factory.unregister_factory("nonexistent.factory");

    // 测试无效的类型名获取
    auto invalid_type_id = global_factory.get_type_id("");
    EXPECT_EQ(invalid_type_id, mc::reflect::INVALID_TYPE_ID);

    // 测试无效模块的类型获取
    auto invalid_module_types = global_factory.get_module_types("nonexistent.module");
    EXPECT_TRUE(invalid_module_types.empty());
}

// 测试统一的命名空间映射机制
TEST_F(reflection_factory_advanced_test, UnifiedNamespaceMapping) {
    // 验证枚举是否注册到了正确的命名空间工厂
    auto& devices_factory  = mc::reflect::reflection_factory::instance<mc_devices_namespace>();
    auto  registered_types = devices_factory.get_registered_types();

    // sensor_status 应该注册到 mc.devices 命名空间
    bool found_sensor_status = false;
    for (const auto& type_name : registered_types) {
        if (type_name.find("SensorStatus") != std::string::npos) {
            found_sensor_status = true;
            break;
        }
    }
    EXPECT_TRUE(found_sensor_status);

    // 验证通过 get_reflect_factory 能够正确获取到对应的工厂
    auto& factory_from_function = mc::reflect::get_reflect_factory<sensor_status>();
    EXPECT_EQ(&factory_from_function, &devices_factory);

    // 验证全局工厂中不包含该枚举类型
    auto& global_factory  = mc::reflect::reflection_factory::global();
    auto  global_types    = global_factory.get_registered_types();
    bool  found_in_global = false;
    for (const auto& type_name : global_types) {
        if (type_name.find("SensorStatus") != std::string::npos) {
            found_in_global = true;
            break;
        }
    }
    EXPECT_FALSE(found_in_global);

    // 验证类型的命名空间映射也正常工作
    auto& device_factory_from_class = mc::reflect::get_reflect_factory<device_sensor>();
    EXPECT_EQ(&device_factory_from_class, &devices_factory);
}

} // namespace test_reflection_namespace