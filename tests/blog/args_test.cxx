#include <array>
#include <cstddef>
#include <expected>
#include <string_view>
#include <gtest/gtest.h>

#include <blog/args.hxx>
#include <blog/error.hxx>

namespace
{
template <std::size_t N> auto parse(std::array<char const*, N> const& args)
{
	return blog::parse_args(args);
}

void expect_options(std::expected<blog::options, blog::error> const& result,
                    blog::options const& expected)
{
	ASSERT_TRUE(result.has_value());
	EXPECT_EQ(result->content, expected.content);
	EXPECT_EQ(result->css, expected.css);
	EXPECT_EQ(result->output, expected.output);
}

void expect_usage(std::expected<blog::options, blog::error> const& result)
{
	ASSERT_FALSE(result.has_value());
	EXPECT_TRUE(result.error() == blog::error{blog::errc::usage});
}
}

TEST(blog_args, the_three_options_in_any_order)
{
	blog::options const expected{.content = "content", .css = "main.css", .output = "out"};
	expect_options(parse(std::array{"--content", "content", "--css", "main.css", "-o", "out"}),
	               expected);
	expect_options(parse(std::array{"-o", "out", "--css", "main.css", "--content", "content"}),
	               expected);
	expect_options(parse(std::array{"--css", "main.css", "-o", "out", "--content", "content"}),
	               expected);
}

TEST(blog_args, a_value_is_whatever_follows_its_option)
{
	expect_options(parse(std::array{"--content", "-c", "--css", "--css", "-o", "-o"}),
	               blog::options{.content = "-c", .css = "--css", .output = "-o"});
}

TEST(blog_args, an_option_may_be_missing_from_none_of_them)
{
	expect_usage(blog::parse_args({}));
	expect_usage(parse(std::array{"--css", "main.css", "-o", "out"}));
	expect_usage(parse(std::array{"--content", "content", "-o", "out"}));
	expect_usage(parse(std::array{"--content", "content", "--css", "main.css"}));
}

TEST(blog_args, a_repeated_option_is_usage)
{
	expect_usage(parse(std::array{"--content", "a", "--content", "b", "--css", "c", "-o", "o"}));
	expect_usage(parse(std::array{"--content", "a", "--css", "c", "--css", "d", "-o", "o"}));
	expect_usage(parse(std::array{"--content", "a", "--css", "c", "-o", "o", "-o", "p"}));
}

TEST(blog_args, a_missing_or_empty_value_is_usage)
{
	expect_usage(parse(std::array{"--content", "a", "--css", "c", "-o"}));
	expect_usage(parse(std::array{"--content"}));
	expect_usage(parse(std::array{"--content", "", "--css", "c", "-o", "o"}));
	expect_usage(parse(std::array{"--content", "a", "--css", "", "-o", "o"}));
	expect_usage(parse(std::array{"--content", "a", "--css", "c", "-o", ""}));
}

TEST(blog_args, anything_else_is_usage)
{
	expect_usage(parse(std::array{"--content", "a", "--css", "c", "-o", "o", "extra"}));
	expect_usage(parse(std::array{"in.jimd", "--content", "a", "--css", "c", "-o", "o"}));
	expect_usage(parse(std::array{"--content", "a", "--css", "c", "-o", "o", "--stylesheet", "x"}));
	expect_usage(parse(std::array{"--content", "a", "--css", "c", "-o", "o", "--dump-ast"}));
	expect_usage(parse(std::array{"--content=a", "--css", "c", "-o", "o"}));
	expect_usage(parse(std::array{""}));
}
