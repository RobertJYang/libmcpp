#include <gtest/gtest.h>

#include <mc/fmt/format_context.h>
#include <mc/fmt/formatter_mc.h>

namespace {

TEST(FormatterSubheaderTest, McTimePointFormattingWorksWithoutFormatFacade)
{
    mc::string                        out;
    mc::fmt::detail::runtime_arg_store args;
    mc::fmt::format_context            ctx(out, args);
    mc::fmt::format_spec               spec;
    mc::fmt::formatter<mc::time_point> formatter;

    formatter.format(mc::time_point(mc::seconds(42)), ctx, spec);

    EXPECT_FALSE(out.empty());
}

} // namespace
