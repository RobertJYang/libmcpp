#include <gtest/gtest.h>

#include <mc/dict/dict.h>

#include <string>
#include <unordered_set>
#include <vector>

namespace {

TEST(DictPublicSubheaderTest, StdFindBridgeWorksViaSubheaderOnly)
{
    mc::dict d{{"alpha", 1}, {"beta", 2}};

    std::string key = "beta";
    auto        it  = d.find(key);

    ASSERT_NE(it, d.end());
    EXPECT_EQ(it->value.as<int>(), 2);
}

TEST(DictPublicSubheaderTest, TemplateMembersWorkViaSubheaderOnly)
{
    std::vector<std::pair<const char*, int>> entries{{"alpha", 1}, {"beta", 2}};

    mc::dict d(entries.begin(), entries.end());

    auto [it, inserted] = d.try_emplace("gamma", 3);

    EXPECT_TRUE(inserted);
    ASSERT_NE(it, d.end());
    EXPECT_EQ(it->value.as<int>(), 3);
}

TEST(DictPublicSubheaderTest, HashWorksViaSubheaderOnly)
{
    std::unordered_set<mc::dict> values;

    values.insert(mc::dict{{"alpha", 1}});

    EXPECT_EQ(values.size(), 1u);
}

} // namespace
