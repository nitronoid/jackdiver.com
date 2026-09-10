#include <array>
#include <expected>
#include <memory>
#include <string>
#include <gtest/gtest.h>

#include <jimd/result.hxx>
#include <support/result_printers.hxx>

namespace
{
constexpr int int_value = 42;
constexpr int unique_value = 123;
constexpr int inner_value = 10;
constexpr int middle_offset = 5;
constexpr int helper_value = 100;

constexpr jimd::error int_error{.code = jimd::errc::syntax, .line = 1, .column = 2};
constexpr jimd::error unique_error{.code = jimd::errc::unclosed_quote, .line = 3, .column = 1};
constexpr jimd::error void_error{.code = jimd::errc::duplicate_definition, .line = 5, .column = 10};
constexpr jimd::error inner_call_error{.code = jimd::errc::undefined_reference,
                                       .line = 7,
                                       .column = 3};
constexpr jimd::error middle_call_error{.code = jimd::errc::syntax, .line = 8, .column = 4};
constexpr jimd::error helper_error{.code = jimd::errc::syntax, .line = 12, .column = 1};

auto get_int(bool fail) -> jimd::result<int>
{
	if (fail) {
		return std::unexpected(int_error);
	}
	return int_value;
}

auto get_long(bool fail) -> jimd::result<long>
{
	auto const val = TRY(get_int(fail));
	return static_cast<long>(val * 2);
}

auto get_unique(bool fail) -> jimd::result<std::unique_ptr<int>>
{
	if (fail) {
		return std::unexpected(unique_error);
	}
	return std::make_unique<int>(unique_value);
}

auto forward_unique(bool fail) -> jimd::result<std::unique_ptr<int>>
{
	auto ptr = TRY(get_unique(fail));
	*ptr += 1;
	return ptr;
}

auto step_void(bool fail) -> jimd::result<>
{
	if (fail) {
		return std::unexpected(void_error);
	}
	return {};
}

auto execute_steps(bool fail) -> jimd::result<>
{
	TRY(step_void(fail));
	return {};
}

auto inner(bool fail) -> jimd::result<int>
{
	if (fail) {
		return std::unexpected(inner_call_error);
	}
	return inner_value;
}

auto middle(int val, bool fail) -> jimd::result<int>
{
	if (fail) {
		return std::unexpected(middle_call_error);
	}
	return val + middle_offset;
}

auto nested_call(bool fail_inner, bool fail_middle) -> jimd::result<int>
{
	auto const result = TRY(middle(TRY(inner(fail_inner)), fail_middle));
	return result * 2;
}

struct custom_error {
	jimd::error inner_error{};

	// NOLINTNEXTLINE(cppcoreguidelines-explicit-constructor,google-explicit-constructor)
	constexpr custom_error(jimd::error err) noexcept
		: inner_error(err)
	{
	}

	auto operator==(custom_error const&) const -> bool = default;
};

auto helper_func(bool fail) -> jimd::result<int>
{
	if (fail) {
		return std::unexpected(helper_error);
	}
	return helper_value;
}

auto convert_error(bool fail) -> std::expected<int, custom_error>
{
	auto const val = TRY(helper_func(fail));
	return val + 1;
}
}

TEST(result, try_with_different_t)
{
	auto const success = get_long(false);
	ASSERT_TRUE(success.has_value());
	EXPECT_EQ(*success, int_value * 2L);

	auto const failure = get_long(true);
	ASSERT_FALSE(failure.has_value());
	EXPECT_EQ(failure.error(), int_error);
}

TEST(result, try_with_move_only_value)
{
	auto const success = forward_unique(false);
	ASSERT_TRUE(success.has_value());
	ASSERT_NE(*success, nullptr);
	EXPECT_EQ(**success, unique_value + 1);

	auto const failure = forward_unique(true);
	ASSERT_FALSE(failure.has_value());
	EXPECT_EQ(failure.error(), unique_error);
}

TEST(result, try_with_void)
{
	auto const success = execute_steps(false);
	EXPECT_TRUE(success.has_value());

	auto const failure = execute_steps(true);
	ASSERT_FALSE(failure.has_value());
	EXPECT_EQ(failure.error(), void_error);
}

TEST(result, try_nesting)
{
	auto const success = nested_call(false, false);
	ASSERT_TRUE(success.has_value());
	EXPECT_EQ(*success, (inner_value + middle_offset) * 2);

	auto const failure_inner = nested_call(true, false);
	ASSERT_FALSE(failure_inner.has_value());
	EXPECT_EQ(failure_inner.error(), inner_call_error);

	auto const failure_middle = nested_call(false, true);
	ASSERT_FALSE(failure_middle.has_value());
	EXPECT_EQ(failure_middle.error(), middle_call_error);
}

TEST(result, try_error_conversion)
{
	auto const success = convert_error(false);
	ASSERT_TRUE(success.has_value());
	EXPECT_EQ(*success, helper_value + 1);

	auto const failure = convert_error(true);
	ASSERT_FALSE(failure.has_value());
	EXPECT_EQ(failure.error().inner_error, helper_error);
}

TEST(result, describe_non_empty)
{
	constexpr std::array codes = {
		jimd::errc::syntax,
		jimd::errc::unclosed_quote,
		jimd::errc::unclosed_code_block,
		jimd::errc::unclosed_emphasis,
		jimd::errc::empty_emphasis,
		jimd::errc::unclosed_resource,
		jimd::errc::invalid_escape,
		jimd::errc::duplicate_definition,
		jimd::errc::undefined_reference,
		jimd::errc::empty_key,
		jimd::errc::empty_link,
		jimd::errc::nested_footnote,
		jimd::errc::duplicate_metadata,
		jimd::errc::unclosed_math,
		jimd::errc::unclosed_math_block,
		jimd::errc::empty_math,
		jimd::errc::math_row_break,
		jimd::errc::math_environment_spans_rows,
		jimd::errc::invalid_width,
		jimd::errc::invalid_target,
		jimd::errc::unrendered_math,
		jimd::errc::unknown_image_size,
	};
	for (auto const code : codes) {
		auto const desc = jimd::describe(code);
		EXPECT_FALSE(desc.empty());
	}
	EXPECT_EQ(jimd::describe(jimd::errc::syntax), "syntax error");
	EXPECT_EQ(jimd::describe(jimd::errc::unclosed_quote), "unclosed quote");
	EXPECT_EQ(jimd::describe(jimd::errc::unclosed_code_block), "unclosed code block");
	EXPECT_EQ(jimd::describe(jimd::errc::unclosed_emphasis), "unclosed emphasis");
	EXPECT_EQ(jimd::describe(jimd::errc::empty_emphasis), "empty emphasis");
	EXPECT_EQ(jimd::describe(jimd::errc::unclosed_resource), "unclosed resource");
	EXPECT_EQ(jimd::describe(jimd::errc::invalid_escape), "invalid escape");
	EXPECT_EQ(jimd::describe(jimd::errc::duplicate_definition), "duplicate definition");
	EXPECT_EQ(jimd::describe(jimd::errc::undefined_reference), "undefined reference");
	EXPECT_EQ(jimd::describe(jimd::errc::empty_key), "empty key");
	EXPECT_EQ(jimd::describe(jimd::errc::empty_link), "empty link");
	EXPECT_EQ(jimd::describe(jimd::errc::nested_footnote), "nested footnote");
	EXPECT_EQ(jimd::describe(jimd::errc::duplicate_metadata), "duplicate metadata");
	EXPECT_EQ(jimd::describe(jimd::errc::unclosed_math), "unclosed maths");
	EXPECT_EQ(jimd::describe(jimd::errc::unclosed_math_block), "unclosed maths block");
	EXPECT_EQ(jimd::describe(jimd::errc::empty_math), "empty maths");
	EXPECT_EQ(jimd::describe(jimd::errc::math_row_break), "maths row ends in a row break");
	EXPECT_EQ(jimd::describe(jimd::errc::math_environment_spans_rows),
	          "maths environment spans rows");
	EXPECT_EQ(jimd::describe(jimd::errc::invalid_width), "invalid image width");
	EXPECT_EQ(jimd::describe(jimd::errc::invalid_target), "invalid figure target");
	EXPECT_EQ(jimd::describe(jimd::errc::unrendered_math), "unrendered maths");
	EXPECT_EQ(jimd::describe(jimd::errc::unknown_image_size), "unknown image size");
}

TEST(result_printers, error_prints_its_description_and_position)
{
	EXPECT_EQ(testing::PrintToString(inner_call_error), "undefined reference at 7:3");
	EXPECT_EQ(testing::PrintToString(std::unexpected(inner_call_error)),
	          "undefined reference at 7:3");
}

TEST(result_printers, result_prints_its_value_or_its_error)
{
	EXPECT_EQ(testing::PrintToString(jimd::result<std::string>{"a\n"}), R"("a\n")");
	EXPECT_EQ(testing::PrintToString(jimd::result<std::string>{std::unexpected(int_error)}),
	          "syntax error at 1:2");
	EXPECT_EQ(testing::PrintToString(jimd::result<>{}), "success");
}
