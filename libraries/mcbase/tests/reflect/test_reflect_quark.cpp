/*
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * openUBMC is licensed under Mulan PSL v2.
 */

/** @file test_reflect_quark.cpp */
#include <gtest/gtest.h>

#include <mc/quark.h>
#include <mc/reflect.h>
#include <mc/reflect/property.h>

using namespace mc;

namespace test_reflect_quark {

class point {
public:
    MC_REFLECTABLE("test_reflect_quark.point");

    int        m_x{0};
    int        m_y{0};
    mc::string m_label;

    int get_sum() const
    {
        return m_x + m_y;
    }
};

} // namespace test_reflect_quark

MC_REFLECT(test_reflect_quark::point,
           (m_x)(m_y)(m_label)(MC_COMPUTED_PROPERTY("sum", get_sum)))

namespace test_reflect_quark {

TEST(reflect_quark, runtime_property_carries_name_quark)
{
    auto& refl = mc::reflect::get_reflection<point>();
    auto* info = refl.get_property_info(mc::string_view("m_x"));
    ASSERT_NE(info, nullptr);
    EXPECT_TRUE(info->name_quark.valid());
    EXPECT_EQ(info->name_quark.view(), mc::string_view("m_x"));
}

TEST(reflect_quark, computed_property_quark_lookup_via_side_index)
{
    auto& refl = mc::reflect::get_reflection<point>();
    auto* by_str = refl.get_property_info(mc::string_view("sum"));
    ASSERT_NE(by_str, nullptr);

    auto* by_quark = refl.get_property_info("sum"_q);
    EXPECT_EQ(by_quark, by_str);
}

TEST(reflect_quark, lookup_by_quark_returns_same_info)
{
    auto& refl = mc::reflect::get_reflection<point>();

    const auto q_x   = "m_x"_q;
    const auto q_lab = "m_label"_q;
    const auto q_sum = "sum"_q;

    auto* by_string_x   = refl.get_property_info(mc::string_view("m_x"));
    auto* by_string_lab = refl.get_property_info(mc::string_view("m_label"));
    auto* by_string_sum = refl.get_property_info(mc::string_view("sum"));

    auto* by_quark_x   = refl.get_property_info(q_x);
    auto* by_quark_lab = refl.get_property_info(q_lab);
    auto* by_quark_sum = refl.get_property_info(q_sum);

    EXPECT_EQ(by_string_x, by_quark_x);
    EXPECT_EQ(by_string_lab, by_quark_lab);
    EXPECT_EQ(by_string_sum, by_quark_sum);
}

TEST(reflect_quark, get_set_by_quark_matches_string_path)
{
    point p;
    p.m_x = 7;
    p.m_y = 11;
    p.m_label = "hello";

    const auto q_x = "m_x"_q;
    const auto q_y = "m_y"_q;

    EXPECT_EQ(mc::reflect::get_property(p, q_x).as_int32(), 7);
    EXPECT_EQ(mc::reflect::get_property(p, q_y).as_int32(), 11);

    EXPECT_TRUE(mc::reflect::set_property(p, q_x, mc::variant(42)));
    EXPECT_EQ(p.m_x, 42);

    EXPECT_EQ(mc::reflect::get_property(p, mc::string_view("m_x")).as_int32(),
              mc::reflect::get_property(p, q_x).as_int32());
}

TEST(reflect_quark, non_field_quark_does_not_match)
{
    auto&      refl = mc::reflect::get_reflection<point>();
    const auto q_no = "__definitely_not_a_field__"_q;

    EXPECT_EQ(refl.get_property_info(q_no), nullptr);

    point p;
    EXPECT_FALSE(mc::reflect::set_property(p, q_no, mc::variant(0)));
}

TEST(reflect_quark, unknown_string_via_try_from_returns_nullptr)
{
    auto&      refl = mc::reflect::get_reflection<point>();
    const auto q_no =
        mc::quark::try_from(mc::string_view("__never_interned_at_all_xyz__"));
    EXPECT_FALSE(q_no.valid());
    EXPECT_EQ(refl.get_property_info(q_no), nullptr);
}

TEST(reflect_quark, invalid_quark_returns_nullptr)
{
    auto&         refl = mc::reflect::get_reflection<point>();
    mc::quark q_invalid;
    EXPECT_FALSE(q_invalid.valid());
    EXPECT_EQ(refl.get_property_info(q_invalid), nullptr);
}

} // namespace test_reflect_quark
