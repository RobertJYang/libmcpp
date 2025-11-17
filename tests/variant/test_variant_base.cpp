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

#include <limits>
#include <optional>
#include <string>
#include <string_view>
#include <typeindex>
#include <utility>
#include <vector>

#include <mc/dict.h>
#include <mc/exception.h>
#include <mc/variant.h>
#include <mc/variant/variant_base.h>
#include <mc/variant/variant_common.h>
#include <mc/variant/variant_extension.h>
#include <mc/variant/variant_reference.h>
#include <mc/variant/variants.h>

namespace {
class simple_extension : public mc::variant_extension<simple_extension> {
public:
    explicit simple_extension(int64_t value = 0, bool allow_reference = false)
        : m_value(value),
          m_allow_reference(allow_reference),
          m_items({mc::variant(value)}),
          m_attrs({{"value", mc::variant(value)}}) {
    }

    simple_extension(const simple_extension&)            = default;
    simple_extension& operator=(const simple_extension&) = default;

    mc::shared_ptr<mc::variant_extension_base> copy() const override {
        return mc::make_shared<simple_extension>(*this);
    }

    mc::shared_ptr<mc::variant_extension_base>
    deep_copy(mc::detail::copy_context* /*ctx*/) const override {
        return copy();
    }

    bool equals(const mc::variant_extension_base& other) const override {
        auto* ptr = dynamic_cast<const simple_extension*>(&other);
        if (ptr == nullptr) {
            return false;
        }
        return current_value() == ptr->current_value() && m_allow_reference == ptr->m_allow_reference;
    }

    bool supports_reference_access() const override {
        return m_allow_reference;
    }

    mc::variant* get_ptr(std::size_t index) override {
        if (!m_allow_reference || index >= m_items.size()) {
            return nullptr;
        }
        return &m_items[index];
    }

    const mc::variant* get_ptr(std::size_t index) const override {
        if (!m_allow_reference || index >= m_items.size()) {
            return nullptr;
        }
        return &m_items[index];
    }

    mc::variant* get_ptr(std::string_view key) override {
        if (!m_allow_reference) {
            return nullptr;
        }
        if (key == "value") {
            ensure_primary_value();
            if (!m_items.empty()) {
                return &m_items[0];
            }
        }
        for (auto& entry : m_attrs) {
            if (entry.first == key) {
                return &entry.second;
            }
        }
        return nullptr;
    }

    const mc::variant* get_ptr(std::string_view key) const override {
        if (!m_allow_reference) {
            return nullptr;
        }
        if (key == "value" && !m_items.empty()) {
            return &m_items[0];
        }
        for (auto& entry : m_attrs) {
            if (entry.first == key) {
                return &entry.second;
            }
        }
        return nullptr;
    }

    mc::variant get(std::size_t index) const override {
        return m_items.at(index);
    }

    void set(std::size_t index, const mc::variant& value) override {
        if (index >= m_items.size()) {
            return;
        }
        m_items[index] = value;
        if (index == 0) {
            update_cached_value(value);
        }
    }

    mc::variant get(std::string_view key) const override {
        if (key == "value") {
            return current_variant();
        }
        for (const auto& entry : m_attrs) {
            if (entry.first == key) {
                return entry.second;
            }
        }
        return {};
    }

    void set(std::string_view key, const mc::variant& value) override {
        if (key == "value") {
            update_cached_value(value);
            return;
        }
        for (auto& entry : m_attrs) {
            if (entry.first == key) {
                entry.second = value;
                return;
            }
        }
        m_attrs.emplace_back(std::string(key), value);
    }

    int64_t as_int64() const override {
        return current_value();
    }

    uint64_t as_uint64() const override {
        return static_cast<uint64_t>(current_value());
    }

    double as_double() const override {
        return static_cast<double>(current_value());
    }

    bool as_bool() const override {
        return current_value() != 0;
    }

    std::string as_string() const override {
        return std::to_string(current_value());
    }

    size_t hash() const override {
        return static_cast<size_t>(current_value()) ^ 0x9e3779b97f4a7c15ULL;
    }

private:
    int64_t                                          m_value;
    bool                                             m_allow_reference;
    std::vector<mc::variant>                         m_items;
    std::vector<std::pair<std::string, mc::variant>> m_attrs;

    void update_cached_value(const mc::variant& value) {
        m_value = value.as_int64();
        ensure_primary_value();
        if (!m_items.empty()) {
            m_items[0] = value;
        }
        bool found = false;
        for (auto& entry : m_attrs) {
            if (entry.first == "value") {
                entry.second = value;
                found        = true;
            }
        }
        if (!found) {
            m_attrs.emplace_back("value", value);
        }
    }

    void ensure_primary_value() {
        if (m_items.empty()) {
            m_items.emplace_back(mc::variant(m_value));
        }
        bool has_key = false;
        for (auto& entry : m_attrs) {
            if (entry.first == "value") {
                has_key = true;
                break;
            }
        }
        if (!has_key) {
            m_attrs.emplace_back("value", mc::variant(m_value));
        }
    }

    int64_t current_value() const {
        if (!m_items.empty()) {
            return m_items[0].as_int64();
        }
        for (const auto& entry : m_attrs) {
            if (entry.first == "value") {
                return entry.second.as_int64();
            }
        }
        return m_value;
    }

    mc::variant current_variant() const {
        if (!m_items.empty()) {
            return m_items[0];
        }
        for (const auto& entry : m_attrs) {
            if (entry.first == "value") {
                return entry.second;
            }
        }
        return mc::variant(m_value);
    }
};

class type_record_visitor : public mc::variant_base::visitor {
public:
    explicit type_record_visitor() : m_last_type("none") {
    }

    void handle() const override {
        m_last_type = "null";
    }
    void handle(const int64_t&) const override {
        m_last_type = "int64";
    }
    void handle(const uint64_t&) const override {
        m_last_type = "uint64";
    }
    void handle(const double&) const override {
        m_last_type = "double";
    }
    void handle(const bool&) const override {
        m_last_type = "bool";
    }
    void handle(const std::string&) const override {
        m_last_type = "string";
    }
    void handle(const mc::variant_base::object_type&) const override {
        m_last_type = "object";
    }
    void handle(const mc::variant_base::array_type&) const override {
        m_last_type = "array";
    }
    void handle(const mc::variant_base::blob_type&) const override {
        m_last_type = "blob";
    }
    void handle(const mc::variant_extension_base&) const override {
        m_last_type = "extension";
    }

    std::string get_last_type() const {
        return m_last_type;
    }

private:
    mutable std::string m_last_type;
};
} // namespace

namespace mc {
namespace test {

TEST(VariantBaseTest, ObjectAccessAndContains) {
    dict object_data{{"foo", 10}, {"bar", 20}};
    variant_base object_value(object_data);
    EXPECT_EQ(object_value.size(), 2U);
    EXPECT_TRUE(object_value.contains("foo"));
    EXPECT_FALSE(object_value.contains("baz"));

    variant_base default_value(999);
    const variant_base& found = object_value.get("foo", default_value);
    EXPECT_EQ(found.as_int64(), 10);

    variant_base number_value(123);
    const variant_base& fallback = number_value.get("unused", default_value);
    EXPECT_EQ(fallback.as_int64(), 999);

    variant_reference ref = object_value["foo"];
    ref = variant(77);
    EXPECT_EQ(object_value["foo"].get().as_int64(), 77);

    object_value.clear();
    EXPECT_TRUE(object_value.is_null());
}

TEST(VariantBaseTest, DictEqualityAndAccessorHelpers) {
    dict object_data{{"alpha", 1}, {"beta", 2}};
    variant_base object_value(object_data);
    mc::dict copied = object_value.as_dict();
    EXPECT_EQ(copied.size(), 2U);
    EXPECT_TRUE(object_value == copied);

    dict other{{"alpha", 1}};
    EXPECT_FALSE(object_value == other);

    variant_base non_object(7);
    EXPECT_THROW(static_cast<void>(non_object.as_dict()), mc::exception);
}

TEST(VariantBaseTest, SizeAndGetters) {
    variant_base string_value("abc");
    EXPECT_EQ(string_value.size(), 3U);
    EXPECT_EQ(string_value.get_string(), "abc");

    blob blob_data;
    blob_data.data = {'x', 'y'};
    variant_base blob_value(blob_data);
    EXPECT_EQ(blob_value.size(), 2U);
    EXPECT_EQ(blob_value.get_blob().data[0], 'x');

    variants array_data{variant(1), variant(2)};
    variant_base array_value(array_data);
    EXPECT_EQ(array_value.size(), 2U);
    const auto& internal_array = array_value.get_array();
    EXPECT_EQ(internal_array[0].as_int32(), 1);

    dict dict_data{{"key", 42}};
    variant_base dict_value(dict_data);
    EXPECT_EQ(dict_value.size(), 1U);
    EXPECT_EQ(dict_value.get_object().at("key").as_int32(), 42);

    variant_base non_string(5);
    EXPECT_THROW(static_cast<void>(non_string.get_string()), mc::exception);
}

TEST(VariantBaseTest, ExtensionIndexAccess) {
    auto extension_ptr = mc::make_shared<simple_extension>(5, true);
    variant_base extension_value(extension_ptr);

    variant_reference index_ref = extension_value[0];
    EXPECT_EQ(index_ref.get().as_int64(), 5);
    index_ref = variant(88);
    EXPECT_EQ(extension_value[0].get().as_int64(), 88);

    variant_reference key_ref = extension_value["value"];
    EXPECT_EQ(key_ref.get().as_int64(), 88);
    key_ref = variant(99);
    EXPECT_EQ(extension_value["value"].get().as_int64(), 99);

    const variant_base& const_extension = extension_value;
    auto               const_idx_ref    = const_extension[0];
    EXPECT_EQ(const_idx_ref.get().as_int64(), 99);
}

TEST(VariantBaseTest, ExtensionNullHandling) {
    variant_base empty_ext(type_id::extension_type);
    EXPECT_THROW(static_cast<void>(empty_ext["missing"]), mc::exception);

    const variant_base& const_ext = empty_ext;
    EXPECT_THROW(static_cast<void>(const_ext["missing"]), mc::exception);
}

TEST(VariantBaseTest, SetValueForDifferentTypes) {
    variant_base string_holder("hello");
    variant_base other_string("world");
    string_holder.set_value(other_string);
    EXPECT_EQ(string_holder.get_string(), "world");

    variants array_one{variant(1), variant(2)};
    variants array_two{variant(3), variant(4)};
    variant_base array_holder(array_one);
    variant_base other_array(array_two);
    array_holder.set_value(other_array);
    EXPECT_EQ(array_holder.get_array()[0].as_int32(), 3);

    dict object_one{{"k", 1}};
    dict object_two{{"k", 2}};
    variant_base object_holder(object_one);
    variant_base other_object(object_two);
    object_holder.set_value(other_object);
    EXPECT_EQ(object_holder.get_object().at("k").as_int32(), 2);

    blob blob_one;
    blob_one.data = {'a'};
    blob blob_two;
    blob_two.data = {'b'};
    variant_base blob_holder(blob_one);
    variant_base other_blob(blob_two);
    blob_holder.set_value(other_blob);
    EXPECT_EQ(blob_holder.get_blob().data[0], 'b');

    auto extension_first  = mc::make_shared<simple_extension>(10);
    auto extension_second = mc::make_shared<simple_extension>(20);
    variant_base extension_holder(extension_first);
    variant_base other_extension(extension_second);
    extension_holder.set_value(other_extension);
    EXPECT_EQ(extension_holder.as_extension()->as_int64(), 20);

    variant_base nullable_string("abc");
    nullable_string = static_cast<const char*>(nullptr);
    EXPECT_TRUE(nullable_string.is_null());

    variant_base text("base");
    text = std::string_view("updated");
    EXPECT_TRUE(text.is_string());
    EXPECT_EQ(text.get_string(), "updated");

    variant_base another_blob_holder(mc::blob{{'x'}});
    blob new_blob;
    new_blob.data = {'d', 'a', 't', 'a'};
    another_blob_holder = new_blob;
    EXPECT_EQ(another_blob_holder.get_blob().data.size(), 4U);

    variant_base fixed_number(0);
    fixed_number.set_fixed_type(true);
    variant_base from_string("100");
    fixed_number = from_string;
    EXPECT_EQ(fixed_number.as_int64(), 100);

    variant_base move_target;
    variant_base move_source("move-from");
    move_target = std::move(move_source);
    EXPECT_TRUE(move_target.is_string());
    EXPECT_TRUE(move_source.is_null());

    variant_base same_type_left(1);
    variant_base same_type_right(2);
    same_type_left.set_value(std::move(same_type_right));
    EXPECT_EQ(same_type_left.as_int64(), 2);
    EXPECT_EQ(same_type_right.as_int64(), 1);
}

TEST(VariantBaseTest, VisitorDispatch) {
    type_record_visitor visitor;

    variant_base null_value;
    null_value.visit(visitor);
    EXPECT_EQ(visitor.get_last_type(), "null");

    variant_base int_value(42);
    int_value.visit(visitor);
    EXPECT_EQ(visitor.get_last_type(), "int64");

    variant_base double_value(3.14);
    double_value.visit(visitor);
    EXPECT_EQ(visitor.get_last_type(), "double");

    variant_base string_value("mc");
    string_value.visit(visitor);
    EXPECT_EQ(visitor.get_last_type(), "string");

    auto extension_ptr = mc::make_shared<simple_extension>(7);
    variant_base extension_value(extension_ptr);
    extension_value.visit(visitor);
    EXPECT_EQ(visitor.get_last_type(), "extension");
}

TEST(VariantBaseTest, OperatorIndexOnArray) {
    variants array_data{variant(1), variant(2)};
    variant_base array_value(array_data);

    variant_reference ref = array_value[1];
    EXPECT_EQ(ref.get().as_int32(), 2);
    ref = variant(5);
    EXPECT_EQ(array_value[1].get().as_int32(), 5);

    const variant_base& const_array = array_value;
    auto                const_ref   = const_array[0];
    EXPECT_EQ(const_ref.get().as_int32(), 1);

    EXPECT_THROW(static_cast<void>(array_value[5]), mc::exception);
    EXPECT_THROW(static_cast<void>(const_array[5]), mc::exception);
}

TEST(VariantBaseTest, CopyDeepCopyAndHash) {
    variant_base move_source("move-me");
    variant_base move_target(std::move(move_source));
    EXPECT_TRUE(move_target.is_string());
    EXPECT_TRUE(move_source.is_null());

    variants array_data{variant(1), variant(mc::dict{{"k", 2}})};
    variant_base array_value(array_data);

    variant_base copied = array_value.copy();
    EXPECT_TRUE(copied.is_array());
    copied[0] = variant(10);
    EXPECT_EQ(array_value[0].get().as_int32(), 1);

    variant_base deep_copied = array_value.deep_copy(nullptr);
    deep_copied[1]["k"]       = variant(5);
    EXPECT_EQ(array_value[1]["k"].get().as_int32(), 2);

    variant_base int_value(5);
    variant_base bool_value(true);
    EXPECT_NE(int_value.hash(), bool_value.hash());
}

TEST(VariantBaseTest, SwapAndTypeName) {
    variant_base v1(1);
    variant_base v2("text");
    v1.swap(v2);
    EXPECT_TRUE(v1.is_string());
    EXPECT_TRUE(v2.is_integer());
    EXPECT_EQ(v2.as_int64(), 1);

    auto extension_ptr = mc::make_shared<simple_extension>(8);
    variant_base extension_value(extension_ptr);
    EXPECT_STREQ(extension_value.get_type_name(), extension_ptr->get_type_name().data());
}

TEST(VariantBaseTest, AsArrayAndExtension) {
    variants array_data{variant(1)};
    variant_base array_value(array_data);
    EXPECT_NO_THROW(array_value.as_array());

    variant_base int_value(1);
    EXPECT_THROW(int_value.as_array(), mc::exception);

    auto extension_ptr = mc::make_shared<simple_extension>(1);
    variant_base extension_value(extension_ptr);
    EXPECT_NO_THROW(extension_value.as_extension());
    EXPECT_EQ(extension_value.as_extension()->as_int64(), 1);
    EXPECT_EQ(extension_value.as_string(), "1");

    EXPECT_THROW(int_value.as_extension(), mc::exception);
}

TEST(VariantBaseTest, VariantsComparisonOperators) {
    variants array_data{variant(1)};
    variant_base array_value(array_data);

    variants same{variant(1)};
    EXPECT_TRUE(array_value == same);

    variants bigger{variant(1), variant(2)};
    EXPECT_TRUE(array_value < bigger);
    EXPECT_TRUE(bigger > same);

    variant_base non_array(5);
    EXPECT_THROW(non_array < same, mc::exception);
}

TEST(VariantBaseTest, NumericOperatorsWithDetailNumeric) {
    mc::detail::numeric_t rhs(5);
    variant_base          int_value(10);

    EXPECT_TRUE(int_value.is_numeric());
    EXPECT_TRUE(int_value.is_signed_integer());

    variant_base bool_value(true);
    EXPECT_TRUE(bool_value.is_bool());
    EXPECT_TRUE(bool_value.is_integer());

    variant_base double_value(4.5);
    EXPECT_TRUE(double_value.is_numeric());
    EXPECT_TRUE(double_value.is_double());

    EXPECT_EQ((int_value + rhs).as_int64(), 15);
    EXPECT_EQ((int_value - rhs).as_int64(), 5);
    EXPECT_EQ((int_value * rhs).as_int64(), 50);
    EXPECT_EQ((int_value / rhs).as_int64(), 2);
    EXPECT_EQ((int_value % rhs).as_int64(), 0);
    EXPECT_EQ((int_value << rhs).as_int64(), 10 << 5);
    EXPECT_EQ((int_value >> rhs).as_int64(), 10 >> 5);
    EXPECT_EQ((int_value & rhs).as_int64(), 10 & 5);
    EXPECT_EQ((int_value | rhs).as_int64(), 10 | 5);
    EXPECT_EQ((int_value ^ rhs).as_int64(), 10 ^ 5);

    variant_base string_value("3");
    auto         string_sum = string_value + rhs;
    EXPECT_TRUE(string_sum.is_string());
    EXPECT_EQ(string_sum.as_string(), "35");

    variant_base negative(-8);
    EXPECT_EQ((negative >> mc::detail::numeric_t(64)).as_int64(), -1);
}

TEST(VariantBaseTest, NumericComparisonsWithDetailNumeric) {
    mc::detail::numeric_t rhs(3);
    variant_base          int_value(4);

    variant_base strict_string("true");
    EXPECT_TRUE(strict_string.as_bool(true));
    variant_base invalid_bool("invalid");
    EXPECT_FALSE(invalid_bool.as_bool(true));

    blob blob_data;
    blob_data.data = {'1', '2', '3'};
    variant_base blob_value(blob_data);
    EXPECT_EQ(blob_value.as_int64(), 123);

    EXPECT_TRUE(int_value > rhs);
    EXPECT_TRUE(int_value >= rhs);
    EXPECT_FALSE(int_value < rhs);
    EXPECT_FALSE(int_value <= rhs);
    EXPECT_FALSE(int_value == mc::detail::numeric_t(10));

    variant_base string_value("5");
    EXPECT_TRUE(string_value == mc::detail::numeric_t(5));
}

TEST(VariantBaseTest, ToVariantAndFromVariantHelpers) {
    variant_base value_holder;
    dict         object_data{{"alpha", 1}};
    to_variant(object_data, value_holder);
    EXPECT_TRUE(value_holder.is_object());

    dict restored;
    from_variant(value_holder, restored);
    EXPECT_TRUE(restored.contains("alpha"));

    variants array_data{variant(1), variant(2)};
    to_variant(array_data, value_holder);
    EXPECT_TRUE(value_holder.is_array());

    variants restored_array;
    from_variant(value_holder, restored_array);
    EXPECT_EQ(restored_array.size(), 2U);
}

TEST(VariantBaseTest, VisitWithHelper) {
    variant_base string_value("visit");
    auto result = string_value.visit_with([](const std::string& v) -> std::string {
        return v + "_suffix";
    });
    EXPECT_EQ(result, "visit_suffix");
}

} // namespace test
} // namespace mc

