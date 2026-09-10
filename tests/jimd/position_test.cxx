#include <cstddef>
#include <string>
#include <string_view>
#include <gtest/gtest.h>

#include <jimd/detail/position.hxx>
#include <jimd/position.hxx>
#include <jimd/result.hxx>
#include <support/result_printers.hxx>

using namespace std::literals;

TEST(position, start)
{
	constexpr auto source = "hello world"sv;
	auto const part = source.substr(0, 5);
	auto const err = jimd::detail::make_error(jimd::errc::syntax, source, part);

	EXPECT_EQ(err, (jimd::error{.code = jimd::errc::syntax, .line = 1, .column = 1}));
}

TEST(position, middle)
{
	constexpr auto source = "hello world"sv;
	auto const part = source.substr(6, 5);
	auto const err = jimd::detail::make_error(jimd::errc::syntax, source, part);

	EXPECT_EQ(err, (jimd::error{.code = jimd::errc::syntax, .line = 1, .column = 7}));

	constexpr auto abc = "abc"sv;
	auto const part_b = abc.substr(1, 1);
	auto const err_b = jimd::detail::make_error(jimd::errc::syntax, abc, part_b);

	EXPECT_EQ(err_b, (jimd::error{.code = jimd::errc::syntax, .line = 1, .column = 2}));
}

TEST(position, after_newline)
{
	constexpr auto source = "first\nsecond\nthird"sv;

	auto const second = source.substr(6, 6);
	auto const err_second = jimd::detail::make_error(jimd::errc::unclosed_quote, source, second);
	EXPECT_EQ(err_second,
	          (jimd::error{.code = jimd::errc::unclosed_quote, .line = 2, .column = 1}));

	auto const third = source.substr(13, 5);
	auto const err_third = jimd::detail::make_error(jimd::errc::undefined_reference, source, third);
	EXPECT_EQ(err_third,
	          (jimd::error{.code = jimd::errc::undefined_reference, .line = 3, .column = 1}));

	constexpr auto indented = "a\n   b"sv;
	auto const part_b = indented.substr(5, 1);
	auto const err_indented =
		jimd::detail::make_error(jimd::errc::duplicate_definition, indented, part_b);
	EXPECT_EQ(err_indented,
	          (jimd::error{.code = jimd::errc::duplicate_definition, .line = 2, .column = 4}));
}

TEST(position, crlf)
{
	constexpr auto source = "first\r\nsecond\r\nthird"sv;

	auto const second = source.substr(7, 6);
	auto const err_second = jimd::detail::make_error(jimd::errc::syntax, source, second);
	EXPECT_EQ(err_second, (jimd::error{.code = jimd::errc::syntax, .line = 2, .column = 1}));

	auto const third = source.substr(15, 5);
	auto const err_third = jimd::detail::make_error(jimd::errc::syntax, source, third);
	EXPECT_EQ(err_third, (jimd::error{.code = jimd::errc::syntax, .line = 3, .column = 1}));

	constexpr auto indented = "a\r\n  b"sv;
	auto const part_b = indented.substr(5, 1);
	auto const err_indented = jimd::detail::make_error(jimd::errc::syntax, indented, part_b);
	EXPECT_EQ(err_indented, (jimd::error{.code = jimd::errc::syntax, .line = 2, .column = 3}));

	constexpr auto multi_blank = "a\r\n\r\nb"sv;
	auto const blank_b = multi_blank.substr(5, 1);
	auto const err_blank = jimd::detail::make_error(jimd::errc::syntax, multi_blank, blank_b);
	EXPECT_EQ(err_blank, (jimd::error{.code = jimd::errc::syntax, .line = 3, .column = 1}));
}

TEST(position, lone_cr_is_not_a_line_break)
{
	constexpr auto source = "a\rb\nc"sv;

	auto const err_b = jimd::detail::make_error(jimd::errc::syntax, source, source.substr(2, 1));
	EXPECT_EQ(err_b, (jimd::error{.code = jimd::errc::syntax, .line = 1, .column = 3}));

	auto const err_c = jimd::detail::make_error(jimd::errc::syntax, source, source.substr(4, 1));
	EXPECT_EQ(err_c, (jimd::error{.code = jimd::errc::syntax, .line = 2, .column = 1}));
}

TEST(position, outside_source)
{
	constexpr auto source = "hello"sv;
	constexpr auto unrelated = "world"sv;
	auto const err_unrelated = jimd::detail::make_error(jimd::errc::syntax, source, unrelated);
	EXPECT_EQ(err_unrelated, (jimd::error{.code = jimd::errc::syntax, .line = 0, .column = 0}));

	auto const err_empty_part =
		jimd::detail::make_error(jimd::errc::syntax, source, std::string_view{});
	EXPECT_EQ(err_empty_part, (jimd::error{.code = jimd::errc::syntax, .line = 0, .column = 0}));

	auto const err_empty_source =
		jimd::detail::make_error(jimd::errc::syntax, std::string_view{}, "hello"sv);
	EXPECT_EQ(err_empty_source, (jimd::error{.code = jimd::errc::syntax, .line = 0, .column = 0}));

	// NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic)
	std::string_view const extends_past(source.data() + 3, 10);
	auto const err_extends = jimd::detail::make_error(jimd::errc::syntax, source, extends_past);
	EXPECT_EQ(err_extends, (jimd::error{.code = jimd::errc::syntax, .line = 0, .column = 0}));
}

TEST(position, zero_length_inside_source)
{
	constexpr auto source = "abc\ndef"sv;
	auto const part_zero = source.substr(4, 0);
	auto const err_zero = jimd::detail::make_error(jimd::errc::syntax, source, part_zero);
	EXPECT_EQ(err_zero, (jimd::error{.code = jimd::errc::syntax, .line = 2, .column = 1}));

	constexpr auto single_line = "abc"sv;
	auto const part_eof = single_line.substr(3, 0);
	auto const err_eof = jimd::detail::make_error(jimd::errc::syntax, single_line, part_eof);
	EXPECT_EQ(err_eof, (jimd::error{.code = jimd::errc::syntax, .line = 1, .column = 4}));
}

TEST(position, an_empty_key_or_link_text_is_at_the_closing_bracket)
{
	constexpr auto key = "a\n[@]: u"sv;
	auto const err_key = jimd::detail::make_error(jimd::errc::empty_key, key, key.substr(4, 0));
	EXPECT_EQ(err_key, (jimd::error{.code = jimd::errc::empty_key, .line = 2, .column = 3}));

	constexpr auto link = "[@ ](u)"sv;
	auto const err_link = jimd::detail::make_error(jimd::errc::empty_link, link, link.substr(3, 0));
	EXPECT_EQ(err_link, (jimd::error{.code = jimd::errc::empty_link, .line = 1, .column = 4}));
}

TEST(position_of, inside_the_source)
{
	constexpr auto source = "first\nsecond\n   third"sv;

	EXPECT_EQ(jimd::position_of(source, source.substr(0, 5)),
	          (jimd::position{.line = 1, .column = 1}));
	EXPECT_EQ(jimd::position_of(source, source.substr(3, 1)),
	          (jimd::position{.line = 1, .column = 4}));
	EXPECT_EQ(jimd::position_of(source, source.substr(6)),
	          (jimd::position{.line = 2, .column = 1}));
	EXPECT_EQ(jimd::position_of(source, source.substr(16)),
	          (jimd::position{.line = 3, .column = 4}));
}

TEST(position_of, only_where_the_part_starts_matters)
{
	constexpr auto source = "ab\ncd\nef"sv;

	EXPECT_EQ(jimd::position_of(source, source.substr(4, 0)),
	          (jimd::position{.line = 2, .column = 2}));
	EXPECT_EQ(jimd::position_of(source, source.substr(4, 1)),
	          (jimd::position{.line = 2, .column = 2}));
	EXPECT_EQ(jimd::position_of(source, source.substr(4)),
	          (jimd::position{.line = 2, .column = 2}));
}

TEST(position_of, the_end_of_the_source_is_a_place)
{
	constexpr auto source = "abc\ndef\n"sv;

	EXPECT_EQ(jimd::position_of(source, source.substr(source.size())),
	          (jimd::position{.line = 3, .column = 1}));
	EXPECT_EQ(jimd::position_of("abc"sv, "abc"sv.substr(3)),
	          (jimd::position{.line = 1, .column = 4}));
}

TEST(position_of, line_ends)
{
	constexpr auto crlf = "a\r\nbc\r\n\r\nd"sv;
	EXPECT_EQ(jimd::position_of(crlf, crlf.substr(3)), (jimd::position{.line = 2, .column = 1}));
	EXPECT_EQ(jimd::position_of(crlf, crlf.substr(4)), (jimd::position{.line = 2, .column = 2}));
	EXPECT_EQ(jimd::position_of(crlf, crlf.substr(9)), (jimd::position{.line = 4, .column = 1}));
	EXPECT_EQ(jimd::position_of(crlf, crlf.substr(2)), (jimd::position{.line = 1, .column = 3}));

	constexpr auto lone_cr = "a\rb\nc"sv;
	EXPECT_EQ(jimd::position_of(lone_cr, lone_cr.substr(2)),
	          (jimd::position{.line = 1, .column = 3}));
	EXPECT_EQ(jimd::position_of(lone_cr, lone_cr.substr(4)),
	          (jimd::position{.line = 2, .column = 1}));
}

TEST(position_of, columns_count_bytes)
{
	constexpr auto source = "30\xC2\xB0"
							"C"sv;
	EXPECT_EQ(jimd::position_of(source, source.substr(4)),
	          (jimd::position{.line = 1, .column = 5}));
}

TEST(position_of, outside_the_source_is_no_place)
{
	constexpr auto source = "hello\nworld"sv;
	constexpr jimd::position none{};

	EXPECT_EQ(jimd::position_of(source, "hello"sv), none);
	EXPECT_EQ(jimd::position_of(source, std::string_view{}), none);
	EXPECT_EQ(jimd::position_of(std::string_view{}, "hello"sv), none);
	EXPECT_EQ(jimd::position_of(std::string_view{}, std::string_view{}), none);

	// NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic)
	std::string_view const extends_past(source.data() + 8, 10);
	EXPECT_EQ(jimd::position_of(source, extends_past), none);

	std::string const buffer = "xhello\nworld";
	std::string_view const inner = std::string_view{buffer}.substr(1);
	EXPECT_EQ(jimd::position_of(inner, std::string_view{buffer}.substr(0, 3)), none);
	EXPECT_EQ(jimd::position_of(inner, std::string_view{buffer}.substr(1, 3)),
	          (jimd::position{.line = 1, .column = 1}));
}

TEST(position_of, make_error_gives_the_same_place)
{
	constexpr auto source = "one\ntwo\r\nthree"sv;
	for (std::size_t offset = 0; offset <= source.size(); ++offset) {
		auto const part = source.substr(offset);
		auto const pos = jimd::position_of(source, part);
		EXPECT_EQ(jimd::detail::make_error(jimd::errc::syntax, source, part),
		          (jimd::error{.code = jimd::errc::syntax, .line = pos.line, .column = pos.column}))
		    << offset;
	}
}
