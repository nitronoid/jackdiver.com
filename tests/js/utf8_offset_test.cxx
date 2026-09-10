#include <cstddef>
#include <optional>
#include <string>
#include <gtest/gtest.h>

#include <js/engine.hxx>

TEST(utf8_offset, counts_each_width_of_character)
{
	std::string const tex = "A\xC3\xA9\xE2\x82\xAC\xF0\x9F\x98\x80"
							"B";
	EXPECT_EQ(js::detail::utf8_offset(tex, 0), std::optional<std::size_t>{0});
	EXPECT_EQ(js::detail::utf8_offset(tex, 1), std::optional<std::size_t>{1});
	EXPECT_EQ(js::detail::utf8_offset(tex, 2), std::optional<std::size_t>{3});
	EXPECT_EQ(js::detail::utf8_offset(tex, 3), std::optional<std::size_t>{6});
	EXPECT_EQ(js::detail::utf8_offset(tex, 5), std::optional<std::size_t>{10});
	EXPECT_EQ(js::detail::utf8_offset(tex, 6), std::optional<std::size_t>{11});
}

TEST(utf8_offset, rounds_the_middle_of_a_surrogate_pair_down_to_its_character)
{
	std::string const tex = "A\xF0\x9F\x98\x80"
							"B";
	EXPECT_EQ(js::detail::utf8_offset(tex, 1), std::optional<std::size_t>{1});
	EXPECT_EQ(js::detail::utf8_offset(tex, 2), std::optional<std::size_t>{1});
	EXPECT_EQ(js::detail::utf8_offset(tex, 3), std::optional<std::size_t>{5});
}

TEST(utf8_offset, puts_the_end_of_the_string_at_its_size)
{
	EXPECT_EQ(js::detail::utf8_offset("", 0), std::optional<std::size_t>{0});
	EXPECT_EQ(js::detail::utf8_offset("ab", 2), std::optional<std::size_t>{2});
	EXPECT_EQ(js::detail::utf8_offset("\xF0\x9F\x98\x80", 2), std::optional<std::size_t>{4});
}

TEST(utf8_offset, has_none_past_the_end)
{
	EXPECT_EQ(js::detail::utf8_offset("", 1), std::nullopt);
	EXPECT_EQ(js::detail::utf8_offset("ab", 3), std::nullopt);
	EXPECT_EQ(js::detail::utf8_offset("\xF0\x9F\x98\x80", 3), std::nullopt);
}

TEST(utf8_offset, counts_a_byte_that_is_not_utf8_as_one_unit)
{
	EXPECT_EQ(js::detail::utf8_offset("\x80z", 1), std::optional<std::size_t>{1});
	EXPECT_EQ(js::detail::utf8_offset("\xE2\x82", 1), std::optional<std::size_t>{1});
	EXPECT_EQ(js::detail::utf8_offset("\xE2z", 1), std::optional<std::size_t>{1});
	EXPECT_EQ(js::detail::utf8_offset("\xFFz", 1), std::optional<std::size_t>{1});
}

TEST(utf8_offset, reads_sequences_as_quickjs_does)
{
	EXPECT_EQ(js::detail::utf8_offset("\xC0\x80z", 1), std::optional<std::size_t>{1});
	EXPECT_EQ(js::detail::utf8_offset("\xC1\xBFz", 2), std::optional<std::size_t>{2});
	EXPECT_EQ(js::detail::utf8_offset("\xE0\x80\x80z", 3), std::optional<std::size_t>{3});
	EXPECT_EQ(js::detail::utf8_offset("\xF0\x80\x80\x80z", 4), std::optional<std::size_t>{4});
	EXPECT_EQ(js::detail::utf8_offset("\xF4\x90\x80\x80z", 4), std::optional<std::size_t>{4});
	EXPECT_EQ(js::detail::utf8_offset("\xF5\x80\x80\x80z", 4), std::optional<std::size_t>{4});
	EXPECT_EQ(js::detail::utf8_offset("\xED\xA0\x80z", 1), std::optional<std::size_t>{3});
	EXPECT_EQ(js::detail::utf8_offset("\xF4\x8F\xBF\xBFz", 2), std::optional<std::size_t>{4});
}
