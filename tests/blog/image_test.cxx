#include <cstdlib>
#include <expected>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <gtest/gtest.h>

#include <blog/error.hxx>
#include <blog/image.hxx>
#include <support/result_printers.hxx>

using namespace std::literals;

namespace
{
auto testdata(std::string_view name) -> std::filesystem::path
{
	// NOLINTBEGIN(concurrency-mt-unsafe)
	char const* const srcdir = std::getenv("TEST_SRCDIR");
	char const* const workspace = std::getenv("TEST_WORKSPACE");
	// NOLINTEND(concurrency-mt-unsafe)
	if (srcdir == nullptr || workspace == nullptr) {
		return {};
	}
	return std::filesystem::path{srcdir} / workspace / "tests" / "blog" / "testdata" / name;
}

auto ratio_of_tag(std::string_view attrs) -> std::expected<std::string, blog::error>
{
	return blog::view_box_ratio("<svg " + std::string{attrs} + "></svg>");
}

auto view_box(std::string_view value) -> std::expected<std::string, blog::error>
{
	return ratio_of_tag("viewBox=\"" + std::string{value} + "\"");
}

void expect_size_error(std::expected<std::string, blog::error> const& result,
                       std::string_view detail)
{
	ASSERT_FALSE(result.has_value()) << result.value();
	EXPECT_EQ(result.error(),
	          (blog::error{blog::errc::image_size, jimd::position{}, std::string{detail}}));
}
}

TEST(svg_ratio, the_five_real_headers)
{
	struct expected {
		std::string_view file;
		std::string_view ratio;
	};
	for (auto const& [file, ratio] : {
			 expected{.file = "boiler.svg", .ratio = "600/740"},
			 expected{.file = "boiler_tau_graph_1.svg", .ratio = "1200/952"},
			 expected{.file = "boiler_tau_graph_2.svg", .ratio = "1200/828"},
			 expected{.file = "mystery-tau.svg", .ratio = "600/688"},
			 expected{.file = "flame.svg", .ratio = "421/600"},
		 }) {
		auto const result = blog::svg_ratio(testdata(file));
		ASSERT_TRUE(result.has_value()) << file;
		EXPECT_EQ(result.value(), ratio) << file;
	}
}

TEST(svg_ratio, a_file_that_is_not_there)
{
	auto const result = blog::svg_ratio(testdata("nothing_here.svg"));
	ASSERT_FALSE(result.has_value());
	EXPECT_EQ(result.error(), (blog::error{blog::errc::file_not_found, jimd::position{}}));
}

TEST(svg_ratio, a_file_that_cannot_be_read)
{
	auto const result = blog::svg_ratio(testdata(""));
	ASSERT_FALSE(result.has_value());
	EXPECT_EQ(result.error(), (blog::error{blog::errc::read_failed, jimd::position{}}));
}

TEST(svg_ratio, an_unusable_size_has_the_reason_and_not_the_file)
{
	auto const missing = blog::svg_ratio(testdata("no_view_box.svg"));
	ASSERT_FALSE(missing.has_value());
	EXPECT_EQ(missing.error(), (blog::error{blog::errc::image_size, jimd::position{},
	                                        "no viewBox in the <svg> tag"}));

	auto const short_list = blog::svg_ratio(testdata("bad_view_box.svg"));
	ASSERT_FALSE(short_list.has_value());
	EXPECT_EQ(short_list.error(), (blog::error{blog::errc::image_size, jimd::position{},
	                                           "viewBox is not four numbers"}));
	EXPECT_EQ(blog::format_error(short_list.error(), "post.jimd"),
	          "post.jimd: no usable image size: viewBox is not four numbers");
}

TEST(view_box_ratio, four_numbers_with_blanks)
{
	EXPECT_EQ(view_box("0 0 600 740").value(), "600/740");
	EXPECT_EQ(view_box("0 0 1200.000000 952.000000").value(), "1200/952");
	EXPECT_EQ(view_box("  0\t0\n600 \r\n 740  ").value(), "600/740");
	EXPECT_EQ(view_box("0 0 600 740\f").value(), "600/740");
}

TEST(view_box_ratio, commas)
{
	EXPECT_EQ(view_box("0,0,600,740").value(), "600/740");
	EXPECT_EQ(view_box("0, 0 , 600 ,740").value(), "600/740");
	EXPECT_EQ(view_box("0 0,600 740").value(), "600/740");
	EXPECT_EQ(view_box(",0 0 600 740").error().detail, "viewBox is not four numbers");
	EXPECT_EQ(view_box("0 0 600 740,").error().detail, "viewBox is not four numbers");
	EXPECT_EQ(view_box("0,,0 600 740").error().detail, "viewBox is not four numbers");
	EXPECT_EQ(view_box("0 , , 0 600 740").error().detail, "viewBox is not four numbers");
}

TEST(view_box_ratio, single_quotes)
{
	EXPECT_EQ(ratio_of_tag("viewBox='0 0 600 740'").value(), "600/740");
	EXPECT_EQ(ratio_of_tag("title=\"it's\" viewBox='0 0 3 4'").value(), "3/4");
	EXPECT_EQ(ratio_of_tag("title='say \"x\"' viewBox=\"0 0 3 4\"").value(), "3/4");
}

TEST(view_box_ratio, numbers_are_written_as_plain_decimals)
{
	EXPECT_EQ(view_box("0 0 600.000000 740.50").value(), "600/740.5");
	EXPECT_EQ(view_box("0 0 600. 740.").value(), "600/740");
	EXPECT_EQ(view_box("0 0 .5 .25").value(), "0.5/0.25");
	EXPECT_EQ(view_box("0 0 0.50 007").value(), "0.5/7");
	EXPECT_EQ(view_box("0 0 +600 +740").value(), "600/740");
	EXPECT_EQ(view_box("0 0 100 1000").value(), "100/1000");
	EXPECT_EQ(view_box("0 0 0.000123 123000.000").value(), "0.000123/123000");
	EXPECT_EQ(view_box("0 0 0.0100 100.0010").value(), "0.01/100.001");
	EXPECT_EQ(view_box("0 0 600.0000000000000000001 1").value(), "600.0000000000000000001/1");
	EXPECT_EQ(view_box("0 0 12345678901234567890123 1").value(), "12345678901234567890123/1");
}

TEST(view_box_ratio, an_exponent_is_not_taken)
{
	expect_size_error(view_box("0 0 1e3 2"), "viewBox is not four numbers");
	expect_size_error(view_box("0 0 1 2E2"), "viewBox is not four numbers");
	expect_size_error(view_box("0 0 1 1e-2"), "viewBox is not four numbers");
	expect_size_error(view_box("1e1 0 3 4"), "viewBox is not four numbers");
}

TEST(view_box_ratio, the_origin_is_checked_and_ignored)
{
	EXPECT_EQ(view_box("-10 -20 600 740").value(), "600/740");
	EXPECT_EQ(view_box("+1.5 .5 600 740").value(), "600/740");
	expect_size_error(view_box("x 0 600 740"), "viewBox is not four numbers");
	expect_size_error(view_box("0 - 600 740"), "viewBox is not four numbers");
	expect_size_error(view_box("0 0.0.0 600 740"), "viewBox is not four numbers");
}

TEST(view_box_ratio, not_four_numbers)
{
	for (auto const* const value : {"",
	                                " ",
	                                "0",
	                                "0 0",
	                                "0 0 600",
	                                "0 0 600 740 1",
	                                "0 0 600 740 x",
	                                "0 0 600 x",
	                                "0 0 600px 740",
	                                "0 0 600 740px",
	                                "0 0 6 0 0",
	                                "0 0 600, 740 ,",
	                                "0 0 600 74e",
	                                "00600 740",
	                                "0 0 600-740 1",
	                                "0 0 inf 740",
	                                "0 0 nan 740",
	                                "0 0 0x10 740",
	                                "0 0 1_0 740",
	                                "0 0 1,5 740",
	                                "0 0 1e3 740"}) {
		auto const result = view_box(value);
		ASSERT_FALSE(result.has_value()) << value;
		EXPECT_EQ(result.error().detail, "viewBox is not four numbers") << value;
	}
}

TEST(view_box_ratio, a_size_must_be_positive)
{
	for (auto const* const value :
	     {"0 0 0 740", "0 0 600 0", "0 0 0 0", "0 0 0.000 740", "0 0 -600 740", "0 0 600 -740",
	      "0 0 -600 -740", "0 0 -0 740", "0 0 00 740", "0 0 600 .0", "0 0 600 0."}) {
		expect_size_error(view_box(value), "viewBox width and height must be positive");
	}
	EXPECT_EQ(view_box("-5 -5 0.001 0.002").value(), "0.001/0.002");
}

TEST(view_box_ratio, the_first_svg_tag_is_the_one)
{
	EXPECT_EQ(
		blog::view_box_ratio("<svg viewBox=\"0 0 3 4\"><svg viewBox=\"0 0 5 6\"/></svg>").value(),
		"3/4");
	EXPECT_EQ(ratio_of_tag("viewBox=\"0 0 3 4\" viewBox=\"0 0 5 6\"").value(), "3/4");
}

TEST(view_box_ratio, a_view_box_outside_the_svg_tag_does_not_count)
{
	expect_size_error(blog::view_box_ratio(R"(<svg width="1"><g viewBox="0 0 3 4"/></svg>)"),
	                  "no viewBox in the <svg> tag");
	expect_size_error(blog::view_box_ratio(R"(<svg width="1"/><g viewBox="0 0 3 4"/>)"),
	                  "no viewBox in the <svg> tag");
	expect_size_error(blog::view_box_ratio("<svg\n>viewBox=\"0 0 3 4\"</svg>"),
	                  "no viewBox in the <svg> tag");
	expect_size_error(blog::view_box_ratio("viewBox=\"0 0 3 4\" <svg>"),
	                  "no viewBox in the <svg> tag");
}

TEST(view_box_ratio, only_the_attribute_called_view_box_counts)
{
	expect_size_error(ratio_of_tag("viewbox=\"0 0 3 4\""), "no viewBox in the <svg> tag");
	expect_size_error(ratio_of_tag("xml:viewBox=\"0 0 3 4\""), "no viewBox in the <svg> tag");
	expect_size_error(ratio_of_tag("data-viewBox=\"0 0 3 4\""), "no viewBox in the <svg> tag");
	expect_size_error(ratio_of_tag("viewBoxx=\"0 0 3 4\""), "no viewBox in the <svg> tag");
	expect_size_error(ratio_of_tag("title=\"viewBox='0 0 3 4'\""), "no viewBox in the <svg> tag");
	EXPECT_EQ(ratio_of_tag("title=\"a > b\" viewBox=\"0 0 3 4\"").value(), "3/4");
	EXPECT_EQ(ratio_of_tag("title='/>' viewBox=\"0 0 3 4\"").value(), "3/4");
}

TEST(view_box_ratio, blanks_around_the_equals_sign)
{
	EXPECT_EQ(ratio_of_tag("viewBox = \"0 0 3 4\"").value(), "3/4");
	EXPECT_EQ(ratio_of_tag("viewBox\n=\n'0 0 3 4'").value(), "3/4");
	EXPECT_EQ(ratio_of_tag("a=\"1\"b=\"2\"viewBox=\"0 0 3 4\"").value(), "3/4");
}

TEST(view_box_ratio, the_tag_can_end_in_a_slash_or_span_lines)
{
	EXPECT_EQ(blog::view_box_ratio("<svg viewBox=\"0 0 3 4\"/>").value(), "3/4");
	EXPECT_EQ(blog::view_box_ratio("<svg\nwidth=\"1\"\r\nviewBox=\"0 0 3 4\"\n>").value(), "3/4");
	EXPECT_EQ(blog::view_box_ratio("<svg\tviewBox=\"0 0 3 4\">").value(), "3/4");
	EXPECT_EQ(blog::view_box_ratio("<svg viewBox=\"0 0 3 4\"").value_or("unclosed"), "3/4");
}

TEST(view_box_ratio, what_comes_before_the_svg_tag)
{
	constexpr auto head =
		"<?xml version=\"1.0\"?>\n<!DOCTYPE svg PUBLIC \"-//W3C//DTD SVG 1.0//EN\"\n"
		" \"http://www.w3.org/TR/2001/REC-SVG-20010904/DTD/svg10.dtd\">\n"sv;
	EXPECT_EQ(blog::view_box_ratio(std::string{head} + "<svg viewBox=\"0 0 3 4\"/>").value(),
	          "3/4");
	EXPECT_EQ(blog::view_box_ratio("\xEF\xBB\xBF<svg viewBox=\"0 0 3 4\"/>").value(), "3/4");
	EXPECT_EQ(
		blog::view_box_ratio("<!-- <svg viewBox=\"0 0 9 9\"> --><svg viewBox=\"0 0 3 4\"/>").value(),
		"3/4");
	EXPECT_EQ(blog::view_box_ratio("<!-- a < b > c --><!----><svg viewBox=\"0 0 3 4\"/>").value(),
	          "3/4");
	EXPECT_EQ(blog::view_box_ratio("<svgx viewBox=\"0 0 9 9\"/><svg viewBox=\"0 0 3 4\"/>").value(),
	          "3/4");
	EXPECT_EQ(
		blog::view_box_ratio("<svg:svg viewBox=\"0 0 9 9\"/><svg viewBox=\"0 0 3 4\"/>").value(),
		"3/4");
}

TEST(view_box_ratio, no_svg_tag)
{
	for (auto const* const text : {"", " ", "hello", "<html></html>", "<g viewBox=\"0 0 3 4\"/>",
	                               "<SVG viewBox=\"0 0 3 4\"/>", "<svgx viewBox=\"0 0 3 4\"/>",
	                               "<!-- <svg viewBox=\"0 0 3 4\"> -->",
	                               "<!-- never closed <svg viewBox=\"0 0 3 4\">", "<", "svg"}) {
		expect_size_error(blog::view_box_ratio(text), "no <svg> tag");
	}
}

TEST(view_box_ratio, a_tag_that_is_not_well_formed)
{
	expect_size_error(blog::view_box_ratio("<svg"), "the <svg> tag is not closed");
	expect_size_error(blog::view_box_ratio("<svg   "), "the <svg> tag is not closed");
	expect_size_error(blog::view_box_ratio("<svg width=\"1\""), "the <svg> tag is not closed");
	expect_size_error(blog::view_box_ratio("<svg viewBox=\"0 0 3 4"),
	                  "the <svg> tag is not closed");
	expect_size_error(blog::view_box_ratio("<svg viewBox='0 0 3 4\""),
	                  "the <svg> tag is not closed");
	constexpr auto bad = "an attribute of the <svg> tag is not name=\"value\""sv;
	expect_size_error(ratio_of_tag("viewBox"), bad);
	expect_size_error(ratio_of_tag("viewBox width=\"1\""), bad);
	expect_size_error(ratio_of_tag("viewBox=0 0 3 4"), bad);
	expect_size_error(ratio_of_tag("viewBox="), bad);
	expect_size_error(ratio_of_tag("=\"1\""), bad);
	expect_size_error(ratio_of_tag("/ viewBox=\"0 0 3 4\""), bad);
	expect_size_error(ratio_of_tag("a viewBox=\"0 0 3 4\""), bad);
	EXPECT_EQ(ratio_of_tag("viewBox=\"0 0 3 4\" junk").value(), "3/4");
}

TEST(view_box_ratio, an_error_has_no_place)
{
	auto const result = view_box("0 0 0 0");
	ASSERT_FALSE(result.has_value());
	EXPECT_EQ(result.error().line, 0U);
	EXPECT_EQ(result.error().column, 0U);
	EXPECT_EQ(blog::format_error(result.error(), "x.svg"),
	          "x.svg: no usable image size: viewBox width and height must be positive");
}

TEST(local_path_problem, a_path_under_the_root_has_none)
{
	for (auto const* const target :
	     {"/x.svg", "/image/x.svg", "/a/b/c/d.svg", "/.svg", "/a/.svg", "/..svg", "/a./b.svg",
	      "/...svg", "/a/.../x.svg", "/a b.svg", "/caf\xC3\xA9.svg", "/a-b_c.d.svg", "/a'b.svg",
	      "/a(b).svg", "/UP.SVG.svg", "/\x80\xff.svg"}) {
		EXPECT_EQ(blog::local_path_problem(target), std::nullopt) << target;
	}
}

TEST(local_path_problem, must_start_with_a_single_slash)
{
	for (auto const* const target :
	     {"x.svg", "image/x.svg", "./x.svg", "../x.svg", "https://h/x.svg", "http:/x.svg",
	      "file:///x.svg", "data:image/svg+xml,x.svg", "C:/x.svg", "\\x.svg", " /x.svg", ""}) {
		EXPECT_EQ(blog::local_path_problem(target), "does not start with /"sv) << target;
	}
	for (auto const* const target : {"//host/x.svg", "//x.svg", "///x.svg", "////"}) {
		EXPECT_EQ(blog::local_path_problem(target), "starts with //"sv) << target;
	}
}

TEST(local_path_problem, no_empty_segment)
{
	for (auto const* const target : {"/a//x.svg", "/a///x.svg", "/a/b//", "/a/", "/"}) {
		EXPECT_EQ(blog::local_path_problem(target), "has an empty segment"sv) << target;
	}
}

TEST(local_path_problem, no_dot_or_dot_dot_segment)
{
	for (auto const* const target : {"/./x.svg", "/../x.svg", "/a/../x.svg", "/a/./x.svg",
	                                 "/a/b/..", "/a/.", "/..", "/.", "/a/b/../../x.svg"}) {
		EXPECT_EQ(blog::local_path_problem(target), "has a . or .. segment"sv) << target;
	}
}

TEST(local_path_problem, no_query_fragment_or_escape)
{
	for (auto const* const target :
	     {"/a?b/x.svg", "/x.svg?v=1", "/a#b/x.svg", "/x.svg#a", "/a%20b.svg", "/a%2e%2e/x.svg",
	      "/a%2Fb.svg", "/%.svg", "/a?"}) {
		EXPECT_EQ(blog::local_path_problem(target), "has a ?, # or %"sv) << target;
	}
}

TEST(local_path_problem, no_backslash_or_control_character)
{
	for (auto const target :
	     {"/a\\..\\x.svg"sv, "/a\\b.svg"sv, "/\\x.svg"sv, "/a/b\\"sv, "/a\tb.svg"sv, "/a\nb.svg"sv,
	      "/a\rb.svg"sv, "/a\x01.svg"sv, "/a\x1f.svg"sv, "/a\x7f.svg"sv, "/a\0b.svg"sv}) {
		EXPECT_EQ(blog::local_path_problem(target), "has a \\ or a control character"sv) << target;
	}
}

TEST(local_path_problem, the_first_reason_wins)
{
	EXPECT_EQ(blog::local_path_problem("//a/../x?.svg"), "starts with //"sv);
	EXPECT_EQ(blog::local_path_problem("/a//../x?.svg"), "has an empty segment"sv);
	EXPECT_EQ(blog::local_path_problem("/a/../x?.svg"), "has a . or .. segment"sv);
	EXPECT_EQ(blog::local_path_problem("/a/x?.svg"), "has a ?, # or %"sv);
	EXPECT_EQ(blog::local_path_problem("/a?\\.svg"), "has a ?, # or %"sv);
}
