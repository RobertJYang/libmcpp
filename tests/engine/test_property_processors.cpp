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
#include <mc/engine/base.h>
#include <mc/engine/metadata.h>
#include <mc/engine/object.h>
#include <mc/engine/property/helper.h>
#include <mc/engine/property/processor.h>
#include <mc/engine/property/processors.h>
#include <mc/engine/property/processors/function_call_processor.h>
#include <mc/engine/property/processors/ref_object_processor.h>
#include <mc/engine/property/processors/ref_property_processor.h>
#include <mc/engine/property/processors/sync_property_processor.h>
#include <mc/engine/property/types.h>
#include <mc/result.h>

// 简单的Mock对象，提供基本功能避免空指针访问（必须在测试用例之前定义）
class MockObject : public mc::engine::abstract_object {
public:
    MockObject()
    {
        // 提供默认值，避免纯虚函数访问问题
    }

    // 只重写必要的方法，其他方法抛出异常提示未实现
    std::string_view get_position() const override
    {
        static std::string pos = "mock_position";
        return pos;
    }

    std::string_view get_object_name() const override
    {
        static std::string name = "MockObject";
        return name;
    }

    // 其他必要的方法，提供默认实现
    void set_service(mc::engine::service* s) override
    {
    }
    mc::engine::service* get_service() const override
    {
        return nullptr;
    }
    mc::engine::abstract_object* get_owner() const override
    {
        return nullptr;
    }
    void set_owner(mc::engine::abstract_object* owner) override
    {
    }
    const mc::engine::abstract_object::managed_objects& get_managed_objects() const override
    {
        static mc::engine::abstract_object::managed_objects empty;
        return empty;
    }
    void set_object_name(std::string_view name) override
    {
    }
    std::string_view get_object_path() const override
    {
        static std::string path = "/mock/object";
        return path;
    }
    void set_object_path(std::string_view path) override
    {
    }
    void set_position(std::string_view position) override
    {
    }
    std::string_view get_class_name() const override
    {
        static std::string cls = "MockObject";
        return cls;
    }
    mc::engine::object_identifier_t get_object_identifier() const override
    {
        return mc::engine::object_identifier_t{};
    }
    void set_object_identifier(const mc::engine::object_identifier_t& identifier) override
    {
    }

    bool has_interface(std::string_view interface_name) const override
    {
        return false;
    }
    mc::engine::abstract_interface* get_interface(std::string_view interface_name) const override
    {
        return nullptr;
    }

    mc::variant get_property(std::string_view property_name, std::string_view interface_name = {}, int options = 0) const override
    {
        return mc::variant("mock_value");
    }
    mc::engine::property_base* get_property_base(std::string_view property_name, std::string_view interface_name = {}) const
    {
        return nullptr;
    }
    bool has_property(std::string_view property_name, std::string_view interface_name = {}) const override
    {
        return false;
    }
    mc::dict get_all_properties(std::string_view interface_name, int options = 0) const override
    {
        return mc::dict();
    }
    bool set_property(std::string_view property_name, const mc::variant& value, std::string_view interface_name = {}) override
    {
        return true;
    }

    mc::connection_type connect(std::string_view signal_name, mc::engine::slot_type slot, std::string_view interface_name = {}) override
    {
        return mc::connection_type();
    }
    mc::variant emit(std::string_view signal_name, const mc::variants& args, std::string_view interface_name = {}) override
    {
        return mc::variant();
    }

    void visit(mc::engine::metadata_visitor& v) const
    {
    }

    bool has_method(std::string_view method_name, std::string_view interface_name = {}) const override
    {
        return false;
    }
    mc::engine::invoke_result invoke(std::string_view method_name, const mc::variants& args = {}, std::string_view interface_name = {}) override
    {
        return mc::engine::invoke_result();
    }
    mc::result<mc::variant> async_invoke(std::string_view method_name, const mc::variants& args = {}, std::string_view interface_name = {}) override
    {
        return mc::result<mc::variant>();
    }

    void notify_property_changed(const mc::variant& value, const mc::engine::property_base& prop) override
    {
    }
    mc::engine::property_changed_signal& property_changed() override
    {
        static mc::engine::property_changed_signal signal;
        return signal;
    }

    void notify_property_update_shm(const mc::variant& value, const mc::engine::property_base& prop) override
    {
    }

    mc::engine::property_changed_signal& property_update_shm() override
    {
        static mc::engine::property_changed_signal signal;
        return signal;
    }

    const mc::engine::object_metadata& get_metadata() const override
    {
        static mc::reflect::struct_metadata mock_struct_metadata = {"MockObject", {}};
        static mc::engine::object_metadata  metadata("MockObject", mock_struct_metadata);
        return metadata;
    }

    void add_managed_object(mc::engine::abstract_object* obj) override
    {
    }
    void remove_managed_object(mc::engine::abstract_object* obj) override
    {
    }

    void set_property_ref_info(std::string_view property_name, const std::string& info,
                               std::string_view interface_name) override
    {
    }

    std::string get_property_ref_info(std::string_view property_name,
                                      std::string_view interface_name) const override
    {
        return "";
    }

    void set_property_sync_info(std::string_view property_name, mc::engine::property_sync_info_ptr info,
                                std::string_view interface_name) override
    {
    }

    mc::engine::property_sync_info_ptr get_property_sync_info(std::string_view property_name,
                                                              std::string_view interface_name) const override
    {
        return {};
    }
};

// 创建一个模拟的property_helper用于测试（必须在测试用例之前定义）
class MockPropertyHelper : public mc::engine::property_helper {
public:
    MockPropertyHelper() = default;

    // property_helper虚函数
    void set_property_type(mc::engine::p_type type) override
    {
        m_type = type;
    }
    mc::engine::p_type get_property_type_enum() const override
    {
        return m_type;
    }
    void set_variant_value(const mc::variant& value) override
    {
    }
    mc::variant get_variant_value() const override
    {
        return mc::variant();
    }
    void set_internal_value(const mc::variant& value) override
    {
    }
    bool has_extension_data() const override
    {
        return false;
    }
    void ensure_extension_data() override
    {
    }
    void set_ref_object_cache(std::unique_ptr<mc::variant> cache) override
    {
        m_cache = std::move(cache);
    }
    mc::variant* get_ref_object_cache() const override
    {
        return m_cache.get();
    }
    void set_getter_function(std::function<mc::variant()> getter) override
    {
        m_getter = std::move(getter);
    }
    void set_setter_function(std::function<void(const mc::variant&)> setter) override
    {
        m_setter = std::move(setter);
    }
    void add_connection_slot(mc::connection_type slot) override
    {
    }
    void clear_connection_slots() override
    {
    }
    void set_function_data(std::unique_ptr<mc::engine::detail::func_data> data) override
    {
        m_func_data = std::move(data);
    }
    mc::engine::detail::func_data* get_function_data() const override
    {
        return m_func_data.get();
    }

    // property_base纯虚函数 - 使用成员变量避免悬空指针
    std::string_view get_name() const override
    {
        return m_name;
    }
    std::string_view get_signature() const override
    {
        return m_signature;
    }
    uint32_t get_access() const override
    {
        return 0;
    }
    uint64_t get_flags() const override
    {
        return 0;
    }
    mc::engine::abstract_interface* get_interface() const override
    {
        return nullptr;
    }
    void set_interface(mc::engine::abstract_interface* interface) override
    { /* 测试中不需要实现 */
    }
    mc::engine::abstract_object* get_object() const override
    {
        // 创建一个临时的mock对象，避免空指针访问
        static auto mock_obj = create_mock_object();
        return mock_obj.get();
    }
    mc::variant get_value(int options = 0) const override
    {
        if (m_getter) {
            try {
                return m_getter();
            } catch (...) {
                return mc::variant();
            }
        }
        return mc::variant();
    }
    mc::engine::property_changed_signal& property_changed() override
    {
        return m_signal;
    }
    void set_variant(const mc::variant& value) override
    {
        if (m_setter) {
            try {
                m_setter(value);
            } catch (...) {
                // 忽略setter中的异常，这是测试环境
            }
        }
    }

    // 创建mock对象的辅助方法
    std::shared_ptr<mc::engine::abstract_object> create_mock_object() const
    {
        // 返回一个可以提供基本信息的对象，避免空指针访问
        return std::shared_ptr<mc::engine::abstract_object>(new MockObject());
    }

    // 重写基类方法以避免空指针访问
    mc::engine::abstract_object* find_related_object(const std::string& object_name)
    {
        return nullptr; // 在测试中安全地返回nullptr
    }

    mc::variant get_relate_property(const mc::expr::relate_property& relate_property)
    {
        return mc::variant("mock_relate_value"); // 返回模拟值避免空值
    }

    void set_relate_property(const mc::expr::relate_property& relate_property, const mc::variant& value)
    {
        // 空实现，测试中不需要真正设置
    }

    mc::variant call_function_with_result(const mc::engine::detail::func_data* function_data)
    {
        return mc::variant("mock_func_result"); // 返回模拟结果
    }

    mc::variant get_override_value() const override
    {
        return {};
    }

    void set_override_value(const mc::variant& value) override
    {
    }

    // 公开成员用于测试
    mc::engine::p_type m_type = mc::engine::p_type::normal;

private:
    mc::engine::property_changed_signal            m_signal;
    std::string                                    m_name      = "mock_property";
    std::string                                    m_signature = "s";
    std::function<mc::variant()>                   m_getter;
    std::function<void(const mc::variant&)>        m_setter;
    std::unique_ptr<mc::variant>                   m_cache;
    std::unique_ptr<mc::engine::detail::func_data> m_func_data;
};

// 扩展的MockPropertyHelper用于更详细的测试（必须在MockPropertyHelper之后，测试用例之前定义）
class DetailedMockPropertyHelper : public MockPropertyHelper {
public:
    void ensure_extension_data() override
    {
        has_extension = true;
    }
    bool has_extension_data() const override
    {
        return has_extension;
    }
    void set_getter_function(std::function<mc::variant()> getter) override
    {
        m_getter = getter;
    }
    void set_setter_function(std::function<void(const mc::variant&)> setter) override
    {
        m_setter = setter;
    }
    void add_connection_slot(mc::connection_type slot) override
    {
        connection_count++;
    }
    void clear_connection_slots() override
    {
        connection_count = 0;
    }

    bool                                    has_extension    = false;
    int                                     connection_count = 0;
    std::function<mc::variant()>            m_getter;
    std::function<void(const mc::variant&)> m_setter;
};

class PropertyProcessorTest : public ::testing::Test {
protected:
    void SetUp() override
    {
        // 注册所有处理器
        mc::engine::register_property_processors();
        factory = &mc::engine::property_processor_factory::get_instance();
    }

    mc::engine::property_processor_factory* factory;
};

// 测试同步属性处理器
TEST_F(PropertyProcessorTest, SyncPropertyProcessor)
{
    mc::engine::sync_property_processor processor;
    DetailedMockPropertyHelper          helper;

    // 测试匹配功能
    EXPECT_TRUE(processor.matches("<=/CPU.Temperature"));
    EXPECT_TRUE(processor.matches("<=/GPU.Load"));
    EXPECT_FALSE(processor.matches("=/CPU.Temperature"));
    EXPECT_FALSE(processor.matches("normal_value"));

    // 测试属性类型
    EXPECT_EQ(processor.get_property_type(), mc::engine::p_type::sync);

    // 测试处理功能（合并了 SyncPropertyProcessorDetails）
    EXPECT_NO_THROW({
        processor.process(&helper, "<=/CPU.Temperature");
    });
    EXPECT_EQ(helper.m_type, mc::engine::p_type::sync);
    EXPECT_TRUE(helper.has_extension);
    EXPECT_TRUE(helper.m_setter);
}

// 测试引用属性处理器
TEST_F(PropertyProcessorTest, RefPropertyProcessor)
{
    mc::engine::ref_property_processor processor;
    DetailedMockPropertyHelper         helper;

    // 测试匹配功能
    EXPECT_TRUE(processor.matches("#/CPU.Temperature"));
    EXPECT_TRUE(processor.matches("#/GPU.Load"));
    EXPECT_FALSE(processor.matches("<=/CPU.Temperature"));
    EXPECT_FALSE(processor.matches("normal_value"));

    // 测试属性类型
    EXPECT_EQ(processor.get_property_type(), mc::engine::p_type::reference);

    // 测试处理功能（合并了 RefPropertyProcessorDetails）
    EXPECT_NO_THROW({
        processor.process(&helper, "#/CPU.Temperature");
    });
    EXPECT_EQ(helper.m_type, mc::engine::p_type::reference);
    EXPECT_TRUE(helper.has_extension);
    EXPECT_TRUE(helper.m_getter);
    EXPECT_TRUE(helper.m_setter);
}

// 测试引用对象处理器
TEST_F(PropertyProcessorTest, RefObjectProcessor)
{
    mc::engine::ref_object_processor processor;
    DetailedMockPropertyHelper       helper;

    // 测试匹配功能
    EXPECT_TRUE(processor.matches("#/CPU"));
    EXPECT_TRUE(processor.matches("#/GPU"));
    EXPECT_FALSE(processor.matches("#/CPU.Temperature"));
    EXPECT_FALSE(processor.matches("normal_value"));

    // 测试属性类型
    EXPECT_EQ(processor.get_property_type(), mc::engine::p_type::ref_object);

    // 测试处理功能（合并了 RefObjectProcessorDetails）
    EXPECT_NO_THROW({
        processor.process(&helper, "#/CPU");
    });
    EXPECT_EQ(helper.m_type, mc::engine::p_type::ref_object);
    EXPECT_TRUE(helper.has_extension);
}

// 测试函数调用处理器
TEST_F(PropertyProcessorTest, FunctionCallProcessor)
{
    mc::engine::function_call_processor processor;
    DetailedMockPropertyHelper          helper;

    // 测试匹配功能
    EXPECT_TRUE(processor.matches("$Func_compute_average({})"));
    EXPECT_TRUE(processor.matches("$Func_get_temperature({})"));
    EXPECT_FALSE(processor.matches("#/CPU.Temperature"));
    EXPECT_FALSE(processor.matches("normal_value"));

    // 测试属性类型
    EXPECT_EQ(processor.get_property_type(), mc::engine::p_type::normal);

    // 测试处理功能（合并了 FunctionCallProcessorDetails）
    EXPECT_NO_THROW({
        processor.process(&helper, "$Func_compute_average({})");
    });
    EXPECT_EQ(helper.m_type, mc::engine::p_type::normal);
    // 当函数不存在时，不应该设置扩展数据和getter
    EXPECT_FALSE(helper.has_extension);
    EXPECT_FALSE(helper.m_getter);
}

// 测试处理器工厂
TEST_F(PropertyProcessorTest, ProcessorFactory)
{
    // 验证工厂已正确注册所有处理器
    // 我们通过测试不同格式的字符串来验证

    MockPropertyHelper helper;

    // 测试同步属性处理
    EXPECT_TRUE(factory->process_property_value(&helper, "<=/CPU.Temperature"));
    EXPECT_EQ(helper.m_type, mc::engine::p_type::sync);

    // 测试引用属性处理
    helper.m_type = mc::engine::p_type::normal;
    EXPECT_TRUE(factory->process_property_value(&helper, "#/CPU.Temperature"));
    EXPECT_EQ(helper.m_type, mc::engine::p_type::reference);

    // 测试引用对象处理
    helper.m_type = mc::engine::p_type::normal;
    EXPECT_TRUE(factory->process_property_value(&helper, "#/CPU"));
    EXPECT_EQ(helper.m_type, mc::engine::p_type::ref_object);

    // 测试函数调用处理
    helper.m_type = mc::engine::p_type::normal;
    EXPECT_TRUE(factory->process_property_value(&helper, "$Func_compute_temp({})"));
    EXPECT_EQ(helper.m_type, mc::engine::p_type::normal);

    // 测试不匹配的字符串
    helper.m_type = mc::engine::p_type::normal;
    EXPECT_FALSE(factory->process_property_value(&helper, "normal_value"));
    EXPECT_EQ(helper.m_type, mc::engine::p_type::normal);
}

// 测试处理器注册
TEST_F(PropertyProcessorTest, ProcessorRegistration)
{
    // 验证register_property_processors函数工作正常
    // 通过检查工厂能否处理各种格式来验证

    MockPropertyHelper helper;

    // 所有支持的格式都应该能被处理
    std::vector<std::string> supported_formats = {
        "<=/CPU.Temperature",    // sync
        "#/CPU.Temperature",     // reference
        "#/CPU",                 // ref_object
        "$Func_compute_temp({})" // function_call
    };

    for (const auto& format : supported_formats) {
        EXPECT_TRUE(factory->process_property_value(&helper, format))
            << "格式 '" << format << "' 应该被支持";
    }

    // 不支持的格式
    std::vector<std::string> unsupported_formats = {
        "normal_value",
        "invalid_format",
        "",
        "xyz123"};

    for (const auto& format : unsupported_formats) {
        EXPECT_FALSE(factory->process_property_value(&helper, format))
            << "格式 '" << format << "' 不应该被支持";
    }
}

// 测试错误情况处理
TEST_F(PropertyProcessorTest, ErrorHandling)
{
    DetailedMockPropertyHelper helper;

    // 测试无效的同步属性格式
    mc::engine::sync_property_processor sync_processor;
    EXPECT_FALSE(sync_processor.matches("invalid_sync_format"));

    // 测试无效的引用属性格式
    mc::engine::ref_property_processor ref_processor;
    EXPECT_FALSE(ref_processor.matches("invalid_ref_format"));

    // 测试无效的引用对象格式
    mc::engine::ref_object_processor ref_obj_processor;
    EXPECT_FALSE(ref_obj_processor.matches("invalid_ref_obj_format"));

    // 测试无效的函数调用格式
    mc::engine::function_call_processor func_processor;
    EXPECT_FALSE(func_processor.matches("invalid_func_format"));
}

// 测试边界情况
TEST_F(PropertyProcessorTest, EdgeCases)
{
    DetailedMockPropertyHelper helper;

    // 测试空字符串 - 直接测试匹配器而不是处理器，避免解析异常
    mc::engine::sync_property_processor sync_processor;
    mc::engine::ref_property_processor  ref_processor;
    mc::engine::ref_object_processor    ref_obj_processor;
    mc::engine::function_call_processor func_processor;

    // 测试空字符串不匹配任何处理器
    EXPECT_FALSE(sync_processor.matches(""));
    EXPECT_FALSE(ref_processor.matches(""));
    EXPECT_FALSE(ref_obj_processor.matches(""));
    EXPECT_FALSE(func_processor.matches(""));

    // 测试只有前缀的字符串会匹配相应处理器（matches只检查前缀）
    EXPECT_TRUE(sync_processor.matches("<=/"));
    EXPECT_FALSE(ref_processor.matches("#/")); // ref_property需要有"."
    EXPECT_TRUE(ref_obj_processor.matches("#/"));
    EXPECT_TRUE(func_processor.matches("$Func_"));

    // 测试格式正确的情况
    EXPECT_TRUE(sync_processor.matches("<=/Object.Property"));
    EXPECT_TRUE(ref_processor.matches("#/Object.Property"));
    EXPECT_TRUE(ref_obj_processor.matches("#/Object"));
    EXPECT_TRUE(func_processor.matches("$Func_test({})"));

    // 测试工厂对于不匹配的字符串返回false（应该安全不抛异常）
    EXPECT_FALSE(factory->process_property_value(&helper, "invalid_format"));
}

// 扩展的 MockPropertyHelper，用于记录 set_variant_value 调用
class TrackingMockPropertyHelper : public DetailedMockPropertyHelper {
public:
    void set_variant_value(const mc::variant& value) override
    {
        set_variant_value_called = true;
        last_value               = value;
    }

    void set_setter_function(std::function<void(const mc::variant&)> setter) override
    {
        DetailedMockPropertyHelper::set_setter_function(
            [this, setter = std::move(setter)](const mc::variant& value) {
            set_relate_property_called = true;
            set_relate_property_value  = value;
            setter(value);
        });
    }

    void set_getter_function(std::function<mc::variant()> getter) override
    {
        DetailedMockPropertyHelper::set_getter_function(
            [this, getter = std::move(getter)]() -> mc::variant {
            get_relate_property_called = true;
            return getter();
        });
    }

    bool        set_variant_value_called = false;
    mc::variant last_value;

    bool get_relate_property_called = false;

    bool        set_relate_property_called = false;
    mc::variant set_relate_property_value;
};

// 测试 ref_object_processor 的 setter 抛出异常
TEST_F(PropertyProcessorTest, RefObjectProcessorSetterThrows)
{
    mc::engine::ref_object_processor processor;
    TrackingMockPropertyHelper       helper;

    // 处理引用对象字符串
    processor.process(&helper, "#/CPU");

    // 验证 setter 被设置
    EXPECT_TRUE(helper.m_setter);

    // 尝试调用 setter，应该抛出异常
    EXPECT_THROW(helper.m_setter(mc::variant(123)), mc::invalid_op_exception);
}

// 测试 ref_property_processor 的 setter 委托
TEST_F(PropertyProcessorTest, RefPropertyProcessorSetterDelegates)
{
    mc::engine::ref_property_processor processor;
    TrackingMockPropertyHelper         helper;

    // 处理引用属性字符串
    processor.process(&helper, "#/CPU.Temperature");

    // 验证 setter 被设置
    EXPECT_TRUE(helper.m_setter);

    // 调用 setter 并捕获异常（processor 会委托到 set_relate_property，缺失对象时会抛异常）
    mc::variant test_value(456);
    EXPECT_THROW(helper.m_setter(test_value), mc::invalid_op_exception);

    // 验证 setter 确实被调用，并记录了写入值
    EXPECT_TRUE(helper.set_relate_property_called);
    EXPECT_EQ(helper.set_relate_property_value, test_value);
}

// 测试 ref_property_processor 的 getter 在 get_relate_property 返回空值时抛出异常
TEST_F(PropertyProcessorTest, RefPropertyProcessorGetterFailsWhenNull)
{
    mc::engine::ref_property_processor processor;
    TrackingMockPropertyHelper         helper;

    // 处理引用属性字符串
    processor.process(&helper, "#/CPU.Temperature");

    // 验证 getter 被设置
    EXPECT_TRUE(helper.m_getter);

    // 调用 getter，应该抛出异常，并且记录调用
    EXPECT_THROW(helper.m_getter(), mc::invalid_op_exception);
    EXPECT_TRUE(helper.get_relate_property_called);
}
