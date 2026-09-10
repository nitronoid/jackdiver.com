#include <array>
#include <expected>
#include <string>
#include <string_view>
#include <variant>
#include <gtest/gtest.h>

#include <blog/error.hxx>
#include <jimd/position.hxx>
#include <jimd/result.hxx>
#include <support/result_printers.hxx>

namespace
{
constexpr int jimd_value = 42;

auto helper_jimd(bool fail) -> jimd::result<int>
{
	if (fail) {
		return std::unexpected(jimd::error{.code = jimd::errc::syntax, .line = 4, .column = 2});
	}
	return jimd_value;
}

auto helper_site(bool fail) -> std::expected<int, blog::error>
{
	auto const val = TRY(helper_jimd(fail));
	return val * 2;
}

auto helper_void_jimd(bool fail) -> jimd::result<>
{
	if (fail) {
		return std::unexpected(
			jimd::error{.code = jimd::errc::duplicate_definition, .line = 1, .column = 3});
	}
	return {};
}

auto helper_void_site(bool fail) -> std::expected<void, blog::error>
{
	TRY(helper_void_jimd(fail));
	return {};
}
}

TEST(blog_error, try_converts_jimd_result)
{
	auto const success = helper_site(false);
	ASSERT_TRUE(success.has_value());
	EXPECT_EQ(*success, jimd_value * 2);

	auto const failure = helper_site(true);
	ASSERT_FALSE(failure.has_value());
	EXPECT_EQ(failure.error(), (jimd::error{.code = jimd::errc::syntax, .line = 4, .column = 2}));
	EXPECT_EQ(failure.error().code, (std::variant<jimd::errc, blog::errc>{jimd::errc::syntax}));
	EXPECT_EQ(failure.error().line, 4U);
	EXPECT_EQ(failure.error().column, 2U);
	EXPECT_TRUE(failure.error().detail.empty());
}

TEST(blog_error, try_converts_jimd_void_result)
{
	auto const success = helper_void_site(false);
	EXPECT_TRUE(success.has_value());

	auto const failure = helper_void_site(true);
	ASSERT_FALSE(failure.has_value());
	EXPECT_EQ(failure.error(),
	          (jimd::error{.code = jimd::errc::duplicate_definition, .line = 1, .column = 3}));
}

TEST(blog_error, a_jimd_error_keeps_its_place_and_has_no_detail)
{
	jimd::error const j_err{.code = jimd::errc::unclosed_quote, .line = 2, .column = 1};
	blog::error const s_from_j = j_err;
	ASSERT_TRUE(std::holds_alternative<jimd::errc>(s_from_j.code));
	EXPECT_EQ(std::get<jimd::errc>(s_from_j.code), jimd::errc::unclosed_quote);
	EXPECT_EQ(s_from_j.line, 2U);
	EXPECT_EQ(s_from_j.column, 1U);
	EXPECT_TRUE(s_from_j.detail.empty());

	blog::error const s_unplaced = jimd::error{.code = jimd::errc::syntax};
	EXPECT_EQ(s_unplaced.line, 0U);
	EXPECT_EQ(s_unplaced.column, 0U);
}

TEST(blog_error, a_site_code_has_no_place_and_no_detail)
{
	blog::error const s_from_c = blog::errc::file_not_found;
	ASSERT_TRUE(std::holds_alternative<blog::errc>(s_from_c.code));
	EXPECT_EQ(std::get<blog::errc>(s_from_c.code), blog::errc::file_not_found);
	EXPECT_EQ(s_from_c.line, 0U);
	EXPECT_EQ(s_from_c.column, 0U);
	EXPECT_TRUE(s_from_c.detail.empty());
}

TEST(blog_error, a_code_a_position_and_a_detail)
{
	blog::error const with_all{blog::errc::read_failed, jimd::position{.line = 3, .column = 9},
	                           "Unsupported function name: \\foo"};
	EXPECT_EQ(with_all.code, (std::variant<jimd::errc, blog::errc>{blog::errc::read_failed}));
	EXPECT_EQ(with_all.line, 3U);
	EXPECT_EQ(with_all.column, 9U);
	EXPECT_EQ(with_all.detail, "Unsupported function name: \\foo");

	blog::error const bare{jimd::errc::empty_math, jimd::position{}};
	EXPECT_EQ(bare, (jimd::error{.code = jimd::errc::empty_math}));
	blog::error const jimd_with_detail{jimd::errc::empty_math,
	                                   jimd::position{.line = 1, .column = 2}, "why"};
	EXPECT_EQ(jimd_with_detail.detail, "why");
}

TEST(blog_error, equality_compares_every_field)
{
	blog::error const err_a = blog::errc::file_not_found;
	blog::error const err_b = blog::errc::file_not_found;
	blog::error const err_c = blog::errc::read_failed;
	jimd::error const j_err{.code = jimd::errc::syntax, .line = 1, .column = 1};
	blog::error const err_d = j_err;

	EXPECT_EQ(err_a, err_b);
	EXPECT_NE(err_a, err_c);
	EXPECT_NE(err_a, err_d);
	EXPECT_EQ(err_d, err_d);
	EXPECT_EQ(err_d, j_err);

	constexpr jimd::position place{.line = 2, .column = 3};
	blog::error const base{blog::errc::write_failed, place, "msg"};
	EXPECT_EQ(base, (blog::error{blog::errc::write_failed, place, "msg"}));
	EXPECT_NE(base, (blog::error{blog::errc::read_failed, place, "msg"}));
	EXPECT_NE(base, (blog::error{jimd::errc::syntax, place, "msg"}));
	EXPECT_NE(base, (blog::error{blog::errc::write_failed, {.line = 9, .column = 3}, "msg"}));
	EXPECT_NE(base, (blog::error{blog::errc::write_failed, {.line = 2, .column = 9}, "msg"}));
	EXPECT_NE(base, (blog::error{blog::errc::write_failed, place, "other"}));
	EXPECT_NE(base, (blog::error{blog::errc::write_failed, place}));

	EXPECT_NE((blog::error{jimd::errc::syntax, {}}), (blog::error{blog::errc::usage}));
}

TEST(blog_error, describe_non_empty)
{
	constexpr std::array codes = {
		blog::errc::file_not_found,  blog::errc::read_failed,      blog::errc::write_failed,
		blog::errc::image_size,      blog::errc::invalid_math,     blog::errc::invalid_code,
		blog::errc::image_not_local, blog::errc::bad_post_name,    blog::errc::bad_page_name,
		blog::errc::missing_title,   blog::errc::unknown_metadata, blog::errc::page_collision,
		blog::errc::usage,           blog::errc::output_not_empty, blog::errc::static_collision,
	};
	for (auto const code : codes) {
		EXPECT_FALSE(blog::describe(code).empty());
	}
	EXPECT_EQ(blog::describe(blog::errc::file_not_found), "file not found");
	EXPECT_EQ(blog::describe(blog::errc::read_failed), "read failed");
	EXPECT_EQ(blog::describe(blog::errc::write_failed), "write failed");
	EXPECT_EQ(blog::describe(blog::errc::image_size), "no usable image size");
	EXPECT_EQ(blog::describe(blog::errc::invalid_math), "invalid maths");
	EXPECT_EQ(blog::describe(blog::errc::invalid_code), "cannot highlight code");
	EXPECT_EQ(blog::describe(blog::errc::image_not_local), "figure target is not a local path");
	EXPECT_EQ(blog::describe(blog::errc::bad_post_name), "bad post file name");
	EXPECT_EQ(blog::describe(blog::errc::bad_page_name), "bad page file name");
	EXPECT_EQ(blog::describe(blog::errc::missing_title), "missing title");
	EXPECT_EQ(blog::describe(blog::errc::unknown_metadata), "unknown metadata key");
	EXPECT_EQ(blog::describe(blog::errc::page_collision), "page collides with a generated path");
	EXPECT_EQ(blog::describe(blog::errc::usage),
	          "usage: blog --content <dir> --css <file> -o <dir>");
	EXPECT_EQ(blog::describe(blog::errc::output_not_empty), "output directory is not empty");
	EXPECT_EQ(blog::describe(blog::errc::static_collision),
	          "static file collides with a file the site writes");
}

TEST(blog_error, format_error)
{
	blog::error const j_err = jimd::error{.code = jimd::errc::syntax, .line = 2, .column = 5};
	EXPECT_EQ(blog::format_error(j_err, "doc.jimd"), "doc.jimd:2:5: syntax error");

	blog::error const usage_err = blog::errc::usage;
	EXPECT_EQ(blog::format_error(usage_err, "doc.jimd"),
	          "usage: blog --content <dir> --css <file> -o <dir>");

	blog::error const fnf_err = blog::errc::file_not_found;
	EXPECT_EQ(blog::format_error(fnf_err, "missing.jimd"), "missing.jimd: file not found");
	EXPECT_EQ(blog::format_error(fnf_err, ""), "file not found");

	blog::error const read_err = blog::errc::read_failed;
	EXPECT_EQ(blog::format_error(read_err, "bad.jimd"), "bad.jimd: read failed");
	EXPECT_EQ(blog::format_error(read_err, ""), "read failed");

	blog::error const write_err = blog::errc::write_failed;
	EXPECT_EQ(blog::format_error(write_err, "out.html"), "out.html: write failed");
	EXPECT_EQ(blog::format_error(write_err, ""), "write failed");
}

TEST(blog_error, format_error_without_a_line)
{
	blog::error const unplaced = jimd::error{.code = jimd::errc::unrendered_math};
	EXPECT_EQ(blog::format_error(unplaced, "doc.jimd"), "doc.jimd: unrendered maths");
	EXPECT_EQ(blog::format_error(unplaced, ""), "unrendered maths");
	EXPECT_EQ(blog::format_error(unplaced), "unrendered maths");
}

TEST(blog_error, format_error_with_a_detail)
{
	constexpr jimd::position place{.line = 12, .column = 9};
	blog::error const placed{jimd::errc::empty_math, place, "no tex"};
	EXPECT_EQ(blog::format_error(placed, "post.jimd"), "post.jimd:12:9: empty maths: no tex");

	blog::error const unplaced{blog::errc::read_failed, jimd::position{}, "permission denied"};
	EXPECT_EQ(blog::format_error(unplaced, "post.jimd"),
	          "post.jimd: read failed: permission denied");
	EXPECT_EQ(blog::format_error(unplaced, ""), "read failed: permission denied");

	blog::error const site_placed{blog::errc::read_failed, place, "x"};
	EXPECT_EQ(blog::format_error(site_placed, "a.jimd"), "a.jimd:12:9: read failed: x");
}

TEST(blog_error, format_error_keeps_the_detail_as_it_is)
{
	blog::error const err{jimd::errc::empty_math, jimd::position{.line = 1, .column = 1},
	                      "Undefined control sequence: \\foo: at {x}\nsecond line"};
	EXPECT_EQ(blog::format_error(err, "p.jimd"),
	          "p.jimd:1:1: empty maths: Undefined control sequence: \\foo: at {x}\nsecond line");
}

TEST(blog_error, the_usage_error_is_only_its_description)
{
	constexpr auto usage = "usage: blog --content <dir> --css <file> -o <dir>";
	blog::error const with_place{blog::errc::usage, jimd::position{.line = 2, .column = 3}};
	EXPECT_EQ(blog::format_error(with_place, "out"), usage);
	EXPECT_EQ(blog::format_error(blog::errc::usage, ""), usage);
	blog::error const with_detail{blog::errc::usage, jimd::position{}, "extra"};
	EXPECT_EQ(blog::format_error(with_detail, "out"), std::string{usage} + ": extra");
}

TEST(blog_error, handle_error_smoke)
{
	blog::error const j_err = jimd::error{.code = jimd::errc::syntax, .line = 1, .column = 1};
	blog::handle_error(j_err, "test.jimd");

	blog::error const s_err = blog::errc::usage;
	blog::handle_error(s_err, "test.jimd");
}
