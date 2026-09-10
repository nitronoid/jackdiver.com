#include <gtest/gtest.h>

#include <cstddef>
#include <cstdlib>
#include <expected>
#include <filesystem>
#include <optional>
#include <ostream>
#include <string>
#include <string_view>
#include <utility>
#include <variant>

#include <blog/error.hxx>
#include <blog/render.hxx>
#include <highlight/hljs.hxx>
#include <jimd/ast.hxx>
#include <jimd/parse.hxx>
#include <jimd/result.hxx>
#include <math/temml.hxx>
#include <support/result_printers.hxx>

using namespace std::literals;

namespace
{
auto engine() -> math::temml&
{
	static auto instance = math::temml::create().value();
	return instance;
}

auto highlighter() -> highlight::hljs&
{
	static auto instance = highlight::hljs::create().value();
	return instance;
}

auto testdata() -> std::filesystem::path
{
	// NOLINTBEGIN(concurrency-mt-unsafe)
	char const* const srcdir = std::getenv("TEST_SRCDIR");
	char const* const workspace = std::getenv("TEST_WORKSPACE");
	// NOLINTEND(concurrency-mt-unsafe)
	if (srcdir == nullptr || workspace == nullptr) {
		return {};
	}
	return std::filesystem::path{srcdir} / workspace / "tests" / "blog" / "testdata";
}

auto context(std::filesystem::path root = testdata()) -> blog::render_context
{
	return blog::render_context{
		.engine = engine(), .highlighter = highlighter(), .static_root = std::move(root)};
}

auto fragment_of(std::string_view source) -> std::expected<std::string, blog::error>
{
	auto const tree = jimd::parse(source);
	EXPECT_TRUE(tree.has_value()) << source;
	if (!tree) {
		return std::unexpected(tree.error());
	}
	return blog::render_fragment(*tree, context());
}

struct failure {
	std::size_t line{};
	std::size_t column{};
	std::string detail;

	auto operator==(failure const&) const -> bool = default;
};

void PrintTo(failure const& value, std::ostream* stream)
{
	*stream << value.line << ':' << value.column << ": " << value.detail;
}

auto fragment_failure(std::string_view source) -> std::optional<failure>
{
	auto const tree = jimd::parse(source);
	EXPECT_TRUE(tree.has_value()) << source;
	if (!tree) {
		return std::nullopt;
	}
	auto const result = blog::render_fragment(*tree, context());
	if (result.has_value()) {
		return std::nullopt;
	}
	EXPECT_EQ(result.error().code, (std::variant<jimd::errc, blog::errc>{blog::errc::invalid_math}))
	    << source;
	return failure{.line = result.error().line,
	               .column = result.error().column,
	               .detail = result.error().detail};
}

constexpr auto undefined_foo = "Unsupported function name: \\foo"sv;
}

TEST(render_fragment, renders_inline_maths)
{
	auto const tree = jimd::parse("Say $x^2$ and $y$.\n");
	ASSERT_TRUE(tree.has_value());

	auto const fragment = blog::render_fragment(*tree, context());

	ASSERT_TRUE(fragment.has_value());
	EXPECT_TRUE(fragment->starts_with("<p>Say <math><semantics><msup>")) << *fragment;
	EXPECT_NE(fragment->find("<annotation encoding=\"application/x-tex\">x^2</annotation>"),
	          std::string::npos);
	EXPECT_NE(fragment->find("</math> and <math>"), std::string::npos);
	EXPECT_EQ(fragment->find("<math display"), std::string::npos);
}

TEST(render_fragment, renders_display_and_aligned_maths_as_blocks)
{
	auto const tree = jimd::parse("$$\nx^2\n$$\n\n$$$\na &= b\nc &= d\n$$$\n");
	ASSERT_TRUE(tree.has_value());

	auto const fragment = blog::render_fragment(*tree, context());

	ASSERT_TRUE(fragment.has_value());
	EXPECT_TRUE(fragment->starts_with("<math display=\"block\"")) << *fragment;
	EXPECT_NE(fragment->find("</math>\n<math display=\"block\""), std::string::npos);
	EXPECT_NE(fragment->find("<mtable"), std::string::npos);
	EXPECT_TRUE(fragment->ends_with("</math>\n"));
}

TEST(render_fragment, the_same_tex_in_both_modes_is_rendered_for_each)
{
	auto const tree = jimd::parse("$x$\n\n$$\nx\n$$\n\n$x$\n");
	ASSERT_TRUE(tree.has_value());

	auto const fragment = blog::render_fragment(*tree, context());

	ASSERT_TRUE(fragment.has_value());
	EXPECT_NE(fragment->find("<p><math><semantics>"), std::string::npos) << *fragment;
	EXPECT_NE(fragment->find("<math display=\"block\""), std::string::npos);
	EXPECT_TRUE(fragment->ends_with("</math></p>\n"));
}

TEST(render_fragment, a_document_without_maths_needs_no_engine_work)
{
	auto const tree = jimd::parse("# Title\n\nJust _text_.\n");
	ASSERT_TRUE(tree.has_value());

	auto const fragment = blog::render_fragment(*tree, context());

	ASSERT_TRUE(fragment.has_value());
	EXPECT_EQ(fragment->find("<math"), std::string::npos);
}

TEST(render_fragment, an_unknown_macro_inline_is_reported_where_it_is)
{
	EXPECT_EQ(fragment_failure("Text $x + \\foo$.\n"),
	          (failure{1, 11, std::string{undefined_foo}}));
	EXPECT_EQ(fragment_failure("one\n\ntwo $\\foo$ and\n"),
	          (failure{3, 6, std::string{undefined_foo}}));
}

TEST(render_fragment, an_unknown_macro_in_a_display_block)
{
	EXPECT_EQ(fragment_failure("$$\na +\n\\foo\n$$\n"),
	          (failure{3, 1, std::string{undefined_foo}}));
	EXPECT_EQ(fragment_failure("Words:\n$$\n  a + \\foo\n$$\n"),
	          (failure{3, 7, std::string{undefined_foo}}));
}

TEST(render_fragment, an_unknown_macro_in_an_aligned_row)
{
	EXPECT_EQ(fragment_failure("$$$\na &= b\nc &= \\foo\n$$$\n"),
	          (failure{3, 6, std::string{undefined_foo}}));
	EXPECT_EQ(fragment_failure("$$$\na &= b\n\nc &= \\foo\nd &= e\n$$$\n"),
	          (failure{4, 6, std::string{undefined_foo}}));
	EXPECT_EQ(fragment_failure("$$$\n\\foo &= b\nc &= d\n$$$\n"),
	          (failure{2, 1, std::string{undefined_foo}}));
}

TEST(render_fragment, a_position_after_a_multi_byte_character_counts_bytes)
{
	EXPECT_EQ(fragment_failure("$30\xC2\xB0"
	                           "C + \\foo$\n"),
	          (failure{1, 10, std::string{undefined_foo}}));
	EXPECT_EQ(fragment_failure("$\xF0\x9F\x98\x80 + \\foo$\n"),
	          (failure{1, 9, std::string{undefined_foo}}));
}

TEST(render_fragment, a_position_after_a_multi_byte_character_in_a_block_counts_bytes)
{
	EXPECT_EQ(fragment_failure("$$\n30\xC2\xB0"
	                           "C + \\foo\n$$\n"),
	          (failure{2, 9, std::string{undefined_foo}}));
	EXPECT_EQ(fragment_failure("$$$\na &= b\n30\xC2\xB0"
	                           "C &= \\foo\n$$$\n"),
	          (failure{3, 10, std::string{undefined_foo}}));
}

TEST(render_fragment, a_blank_row_with_no_ampersand_in_a_one_column_block_renders_cleanly)
{
	auto const tree = jimd::parse("$$$\na = b\n\nc = d\n$$$\n");
	ASSERT_TRUE(tree.has_value());

	auto const fragment = blog::render_fragment(*tree, context());

	ASSERT_TRUE(fragment.has_value());
	EXPECT_NE(fragment->find("<mrow></mrow></mtd></mtr>"), std::string::npos) << *fragment;
	EXPECT_EQ(fragment->find("undefined"), std::string::npos) << *fragment;
}

TEST(render_fragment, a_row_of_one_ampersand_between_rows_renders_cleanly)
{
	auto const tree = jimd::parse("$$$\na &= b\n&\nc &= d\n$$$\n");
	ASSERT_TRUE(tree.has_value());

	auto const fragment = blog::render_fragment(*tree, context());

	ASSERT_TRUE(fragment.has_value());
	EXPECT_EQ(fragment->find("undefined"), std::string::npos) << *fragment;
}

TEST(render_fragment, a_crlf_display_body_is_reported_in_the_source)
{
	EXPECT_EQ(fragment_failure("$$\r\na +\r\n\\foo\r\n$$\r\n"),
	          (failure{3, 1, std::string{undefined_foo}}));
	EXPECT_EQ(fragment_failure("$$\r\na + \\foo\r\n$$\r\n"),
	          (failure{2, 5, std::string{undefined_foo}}));
}

TEST(render_fragment, a_crlf_aligned_block_is_reported_in_the_source)
{
	EXPECT_EQ(fragment_failure("$$$\r\na &= b\r\nc &= \\foo\r\n$$$\r\n"),
	          (failure{3, 6, std::string{undefined_foo}}));
}

TEST(render_fragment, an_unclosed_brace_in_the_last_aligned_row_is_the_end_of_that_row)
{
	EXPECT_EQ(fragment_failure("$$$\na &= b\nc &= {d\n$$$\n"),
	          (failure{3, 8, "Expected '}', got '\\end'"}));
	EXPECT_EQ(fragment_failure("$$$\r\na &= b\r\nc &= {d\r\n$$$\r\n"),
	          (failure{3, 8, "Expected '}', got '\\end'"}));
	EXPECT_EQ(fragment_failure("$$$\n{d\n$$$\n"), (failure{2, 3, "Expected '}', got '\\end'"}));
}

TEST(render_fragment, an_unclosed_brace_in_an_earlier_aligned_row)
{
	auto const result = fragment_failure("$$$\na &= {b\nc &= d\n$$$\n");
	ASSERT_TRUE(result.has_value());
	EXPECT_EQ(result.value().line, 3U);
	EXPECT_EQ(result.value().column, 3U) << result.value().detail;
	EXPECT_EQ(result.value().detail, "Expected '}', got '&'");
}

TEST(render_fragment, the_detail_is_temmls_message_as_it_is)
{
	auto const tree = jimd::parse("$\\color{x at position 1: }$\n");
	ASSERT_TRUE(tree.has_value());

	auto const result = blog::render_fragment(*tree, context());

	ASSERT_FALSE(result.has_value());
	EXPECT_EQ(result.error().detail, "Invalid color: 'x at position 1: '");
	EXPECT_EQ(blog::format_error(result.error(), "post.jimd"),
	          "post.jimd:1:" + std::to_string(result.error().column) +
	              ": invalid maths: Invalid color: 'x at position 1: '");
}

TEST(render_fragment, the_message_is_formatted_with_the_path_line_and_column)
{
	auto const result = fragment_of("one\n\ntwo $x + \\foo$\n");

	ASSERT_FALSE(result.has_value());
	EXPECT_EQ(blog::format_error(result.error(), "post.jimd"),
	          "post.jimd:3:10: invalid maths: Unsupported function name: \\foo");
}

TEST(render_fragment, an_error_with_no_offset_is_at_the_start_of_the_maths)
{
	auto const result = fragment_failure("a $x^$ b\n");
	ASSERT_TRUE(result.has_value());
	EXPECT_EQ(result.value().line, 1U);
	EXPECT_EQ(result.value().column, 4U);
	EXPECT_FALSE(result.value().detail.empty());

	auto const display = fragment_failure("$$\nx^\n$$\n");
	ASSERT_TRUE(display.has_value());
	EXPECT_EQ(display.value().line, 2U);
	EXPECT_EQ(display.value().column, 1U);

	auto const aligned = fragment_failure("$$$\n\na &= x^\n$$$\n");
	ASSERT_TRUE(aligned.has_value());
	EXPECT_EQ(aligned.value().line, 3U);
	EXPECT_EQ(aligned.value().column, 8U);
}

TEST(render_fragment, maths_in_a_footnote_definition)
{
	EXPECT_EQ(fragment_failure("A note[^n].\n\n[^n]: The $\\foo$ note.\n"),
	          (failure{3, 12, std::string{undefined_foo}}));
	EXPECT_EQ(fragment_failure("Text.\n\n[^n]: The $\\foo$ note.\n"),
	          (failure{3, 12, std::string{undefined_foo}}));
}

TEST(render_fragment, maths_in_an_inline_footnote)
{
	EXPECT_EQ(fragment_failure("A note[^n](see $\\foo$).\n"),
	          (failure{1, 17, std::string{undefined_foo}}));
}

TEST(render_fragment, maths_in_a_definition_renders)
{
	auto const result = fragment_of("A note[^n].\n\n[^n]: The $x$ note.\n");

	ASSERT_TRUE(result.has_value());
	EXPECT_NE(result->find("<aside id=\"note-1\""), std::string::npos);
	EXPECT_NE(result->find("The <math>"), std::string::npos);
}

TEST(render_fragment, two_errors_report_the_first_in_the_file)
{
	EXPECT_EQ(fragment_failure("$\\foo$ and $\\baz$\n"),
	          (failure{1, 2, std::string{undefined_foo}}));
	EXPECT_EQ(fragment_failure("$\\baz$ and $\\foo$\n"),
	          (failure{1, 2, "Unsupported function name: \\baz"}));
	EXPECT_EQ(fragment_failure("a $\\baz$\n\n$$\n\\foo\n$$\n"),
	          (failure{1, 4, "Unsupported function name: \\baz"}));
	EXPECT_EQ(fragment_failure("$$\n\\baz\n$$\n\na $\\foo$\n"),
	          (failure{2, 1, "Unsupported function name: \\baz"}));
}

TEST(render_fragment, a_definition_is_ordered_where_it_was_written)
{
	EXPECT_EQ(fragment_failure("[^n]: A $\\baz$ note.\n\nText $\\foo$[^n].\n"),
	          (failure{1, 10, "Unsupported function name: \\baz"}));
	EXPECT_EQ(fragment_failure("Text $\\foo$[^n].\n\n[^n]: A $\\baz$ note.\n"),
	          (failure{1, 7, std::string{undefined_foo}}));
}

TEST(render_fragment, equal_tex_that_fails_is_reported_at_its_first_use)
{
	EXPECT_EQ(fragment_failure("ok $x$ then $\\foo$ then $\\foo$\n"),
	          (failure{1, 14, std::string{undefined_foo}}));
}

TEST(render_fragment, the_engine_works_after_an_error)
{
	EXPECT_TRUE(fragment_failure("$\\foo$\n").has_value());

	auto const result = fragment_of("Fine: $x$.\n");
	EXPECT_TRUE(result.has_value());
}

TEST(render_fragment, a_tree_not_made_from_the_source_has_no_place)
{
	jimd::ast::document doc;
	doc.blocks.emplace_back(jimd::ast::paragraph{
		.children = {jimd::ast::inline_node{jimd::ast::math{.tex = "\\foo"sv}}}});

	auto const result = blog::render_fragment(doc, context());

	ASSERT_FALSE(result.has_value());
	EXPECT_EQ(result.error(), (blog::error{blog::errc::invalid_math, jimd::position{},
	                                       std::string{undefined_foo}}));
}

TEST(render_fragment, a_jimd_error_is_not_changed)
{
	jimd::ast::document doc;
	doc.blocks.emplace_back(jimd::ast::paragraph{
		.children = {jimd::ast::inline_node{
			jimd::ast::reference{.key = "nothing"sv, .content = std::nullopt}}}});

	auto const result = blog::render_fragment(doc, context());

	ASSERT_FALSE(result.has_value());
	EXPECT_EQ(result.error(), (jimd::error{.code = jimd::errc::undefined_reference}));
}

TEST(render_fragment, maths_in_a_heading_and_a_paragraph)
{
	auto const fragment = fragment_of("# Energy $E = mc^2$\n\nSo $E$ is it.\n");

	ASSERT_TRUE(fragment.has_value());
	EXPECT_NE(fragment->find("Energy <math>"), std::string::npos) << *fragment;
	EXPECT_NE(fragment->find("<p>So <math>"), std::string::npos);
}

TEST(render_fragment, a_maths_error_is_a_site_error_with_detail)
{
	auto const fragment = fragment_of("# Title\n\nSo $\\foo$ is it.\n");

	ASSERT_FALSE(fragment.has_value());
	EXPECT_EQ(fragment.error(),
	          (blog::error{blog::errc::invalid_math, jimd::position{.line = 3, .column = 5},
	                       std::string{undefined_foo}}));
}

namespace
{
auto figure_error(std::string_view source, std::filesystem::path root = testdata())
	-> std::optional<blog::error>
{
	auto const tree = jimd::parse(source);
	EXPECT_TRUE(tree.has_value()) << source;
	if (!tree) {
		return std::nullopt;
	}
	auto const result = blog::render_fragment(*tree, context(std::move(root)));
	if (result.has_value()) {
		return std::nullopt;
	}
	return result.error();
}

auto target_error(std::string_view target) -> std::optional<blog::error>
{
	return figure_error("Text.\n\n[!A](" + std::string{target} + ")\n");
}

auto at_the_image(blog::errc code, std::string detail) -> blog::error
{
	return blog::error{code, jimd::position{.line = 3, .column = 3}, std::move(detail)};
}
}

TEST(render_fragment, an_svg_figure_gets_the_ratio_of_its_file)
{
	auto const fragment = fragment_of("[!Boiler](/boiler.svg 75%)\n\n[!Flame](/flame.svg)\n");

	ASSERT_TRUE(fragment.has_value());
	EXPECT_NE(fragment->find(
				  "<div class=\"ink-mask\" role=\"img\" aria-label=\"Boiler\" style=\"--src: "
				  "url('/boiler.svg'); --ratio: 600/740; --width: 75%\"></div>\n"),
	          std::string::npos)
	    << *fragment;
	EXPECT_NE(fragment->find("url('/flame.svg'); --ratio: 421/600\"></div>\n"), std::string::npos);
}

TEST(render_fragment, a_target_under_a_directory)
{
	auto const root = std::filesystem::path{::testing::TempDir()} / "figure_dirs";
	std::filesystem::remove_all(root);
	std::filesystem::create_directories(root / "image" / "deep");
	std::filesystem::copy_file(testdata() / "mystery-tau.svg", root / "image" / "deep" / "m.svg");

	auto const tree = jimd::parse("[!M](/image/deep/m.svg)\n");
	ASSERT_TRUE(tree.has_value());
	auto const fragment = blog::render_fragment(*tree, context(root));

	ASSERT_TRUE(fragment.has_value());
	EXPECT_NE(fragment->find("--ratio: 600/688"), std::string::npos);
}

TEST(render_fragment, the_same_file_in_several_figures_and_in_a_quote)
{
	auto const fragment = fragment_of(
		"[!A](/boiler.svg)\n\n[!B](/boiler.svg 50%)\n\n>\n[!C](/boiler.svg)\n<\n\n[!D]\n\n[!D]: /flame.svg\n");

	ASSERT_TRUE(fragment.has_value());
	std::size_t boiler = 0;
	for (auto at = fragment->find("--ratio: 600/740"); at != std::string::npos;
	     at = fragment->find("--ratio: 600/740", at + 1)) {
		++boiler;
	}
	EXPECT_EQ(boiler, 3U);
	EXPECT_NE(fragment->find("aria-label=\"C\""), std::string::npos);
	EXPECT_NE(fragment->find("aria-label=\"D\" style=\"--src: url('/flame.svg'); --ratio: 421/600"),
	          std::string::npos)
	    << *fragment;
}

TEST(render_fragment, figures_that_are_not_svg_figures_read_nothing)
{
	auto const fragment = fragment_of(
		"[!P](/nothing.png)\n\nText with [!an image](/nothing.svg) in it.\n\n[!U](https://h/x.svg?v=1)\n\n- [!L](/nothing.svg)\n");

	ASSERT_TRUE(fragment.has_value());
	EXPECT_EQ(fragment->find("ink-mask"), std::string::npos);
	EXPECT_NE(fragment->find("<img src=\"/nothing.png\""), std::string::npos);
}

TEST(render_fragment, a_target_that_is_not_a_local_path_is_an_error_at_the_image)
{
	EXPECT_EQ(target_error("x.svg"),
	          at_the_image(blog::errc::image_not_local, "x.svg: does not start with /"));
	EXPECT_EQ(target_error("//host/x.svg"),
	          at_the_image(blog::errc::image_not_local, "//host/x.svg: starts with //"));
	EXPECT_EQ(target_error("https://host/x.svg"),
	          at_the_image(blog::errc::image_not_local,
	                       "https://host/x.svg: does not start with /"));
	EXPECT_EQ(target_error("/a/../x.svg"),
	          at_the_image(blog::errc::image_not_local, "/a/../x.svg: has a . or .. segment"));
	EXPECT_EQ(target_error("/./x.svg"),
	          at_the_image(blog::errc::image_not_local, "/./x.svg: has a . or .. segment"));
	EXPECT_EQ(target_error("/a//x.svg"),
	          at_the_image(blog::errc::image_not_local, "/a//x.svg: has an empty segment"));
	EXPECT_EQ(target_error("/a\\..\\x.svg"),
	          at_the_image(blog::errc::image_not_local,
	                       "/a\\..\\x.svg: has a \\ or a control character"));
	EXPECT_EQ(target_error("/a?b/x.svg"),
	          at_the_image(blog::errc::image_not_local, "/a?b/x.svg: has a ?, # or %"));
	EXPECT_EQ(target_error("/a%2e%2e/x.svg"),
	          at_the_image(blog::errc::image_not_local, "/a%2e%2e/x.svg: has a ?, # or %"));
}

TEST(render_fragment, a_missing_file_is_an_error_at_the_image_naming_the_target)
{
	EXPECT_EQ(target_error("/nothing.svg"),
	          at_the_image(blog::errc::file_not_found, "/nothing.svg"));
	EXPECT_EQ(target_error("/image/nothing.svg"),
	          at_the_image(blog::errc::file_not_found, "/image/nothing.svg"));
	EXPECT_EQ(blog::format_error(target_error("/nothing.svg").value(), "post.jimd"),
	          "post.jimd:3:3: file not found: /nothing.svg");
}

TEST(render_fragment, a_file_that_cannot_be_read_is_an_error_at_the_image)
{
	auto const root = std::filesystem::path{::testing::TempDir()} / "figure_directory";
	std::filesystem::remove_all(root);
	std::filesystem::create_directories(root / "dir.svg");

	EXPECT_EQ(figure_error("Text.\n\n[!A](/dir.svg)\n", root),
	          at_the_image(blog::errc::read_failed, "/dir.svg"));
}

TEST(render_fragment, a_file_with_no_usable_view_box_is_an_error_at_the_image)
{
	EXPECT_EQ(target_error("/no_view_box.svg"),
	          at_the_image(blog::errc::image_size,
	                       "/no_view_box.svg: no viewBox in the <svg> tag"));
	EXPECT_EQ(target_error("/bad_view_box.svg"),
	          at_the_image(blog::errc::image_size,
	                       "/bad_view_box.svg: viewBox is not four numbers"));
	EXPECT_EQ(blog::format_error(target_error("/no_view_box.svg").value(), "post.jimd"),
	          "post.jimd:3:3: no usable image size: /no_view_box.svg: no viewBox in the <svg> tag");
}

TEST(render_fragment, the_error_of_a_defined_image_is_at_the_image_and_not_the_definition)
{
	EXPECT_EQ(figure_error("Text.\n\n[!A]\n\n[!A]: /nothing.svg\n"),
	          at_the_image(blog::errc::file_not_found, "/nothing.svg"));
	EXPECT_EQ(figure_error("Text.\n\n[!A]\n\n[!A]: //host/x.svg\n"),
	          at_the_image(blog::errc::image_not_local, "//host/x.svg: starts with //"));
}

TEST(render_fragment, the_error_is_in_a_quote_too)
{
	EXPECT_EQ(figure_error(">\n[!A](/nothing.svg)\n<\n"),
	          (blog::error{blog::errc::file_not_found, jimd::position{.line = 2, .column = 3},
	                       "/nothing.svg"}));
}

TEST(render_fragment, two_figure_errors_report_the_first_in_the_file)
{
	EXPECT_EQ(figure_error("[!A](/boiler.svg)\n\n[!B](/nothing.svg)\n\n[!C](//host/x.svg)\n"),
	          (blog::error{blog::errc::file_not_found, jimd::position{.line = 3, .column = 3},
	                       "/nothing.svg"}));
	EXPECT_EQ(figure_error("[!A](//host/x.svg)\n\n[!B](/nothing.svg)\n"),
	          (blog::error{blog::errc::image_not_local, jimd::position{.line = 1, .column = 3},
	                       "//host/x.svg: starts with //"}));
	EXPECT_EQ(figure_error("[!A](/nothing.svg)\n\n[!B](/nothing.svg)\n"),
	          (blog::error{blog::errc::file_not_found, jimd::position{.line = 1, .column = 3},
	                       "/nothing.svg"}));
}

TEST(render_fragment, maths_is_checked_before_figures_whatever_their_places)
{
	auto const error = figure_error("[!A](/nothing.svg)\n\nThen $\\foo$.\n");
	ASSERT_TRUE(error.has_value());
	EXPECT_EQ(error.value().code, (std::variant<jimd::errc, blog::errc>{blog::errc::invalid_math}));
	EXPECT_EQ(error.value().line, 3U);
}

TEST(render_fragment, a_figure_error_comes_before_the_renderers_own)
{
	EXPECT_EQ(target_error("/it's.svg"), at_the_image(blog::errc::file_not_found, "/it's.svg"));

	auto const root = std::filesystem::path{::testing::TempDir()} / "figure_quote";
	std::filesystem::remove_all(root);
	std::filesystem::create_directories(root);
	std::filesystem::copy_file(testdata() / "flame.svg", root / "it's.svg");
	auto const error = figure_error("Text.\n\n[!A](/it's.svg)\n", root);
	ASSERT_TRUE(error.has_value());
	EXPECT_EQ(
		error.value(),
		(blog::error{jimd::error{.code = jimd::errc::invalid_target, .line = 3, .column = 9}}));
}

TEST(render_fragment, a_symlink_in_the_static_root_is_followed)
{
	auto const root = std::filesystem::path{::testing::TempDir()} / "figure_link";
	std::filesystem::remove_all(root);
	std::filesystem::create_directories(root);
	std::filesystem::create_symlink(testdata() / "flame.svg", root / "link.svg");

	auto const tree = jimd::parse("[!A](/link.svg)\n");
	ASSERT_TRUE(tree.has_value());
	auto const fragment = blog::render_fragment(*tree, context(root));

	ASSERT_TRUE(fragment.has_value());
	EXPECT_NE(fragment->find("--ratio: 421/600"), std::string::npos);
}

TEST(render_fragment, a_tree_not_made_from_the_source_has_its_figure_error_without_a_place)
{
	jimd::ast::document doc;
	doc.blocks.emplace_back(jimd::ast::paragraph{
		.children = {jimd::ast::inline_node{jimd::ast::image{
			.key = "A", .alt = "A", .target = "/nothing.svg", .width = std::nullopt}}}});

	auto const result = blog::render_fragment(doc, context());

	ASSERT_FALSE(result.has_value());
	EXPECT_EQ(result.error(),
	          (blog::error{blog::errc::file_not_found, jimd::position{}, "/nothing.svg"}));
}

namespace
{
auto code_error(std::string_view source) -> std::optional<blog::error>
{
	auto const tree = jimd::parse(source);
	EXPECT_TRUE(tree.has_value()) << source;
	if (!tree) {
		return std::nullopt;
	}
	auto const result = blog::render_fragment(*tree, context());
	if (result.has_value()) {
		return std::nullopt;
	}
	return result.error();
}
}

TEST(render_fragment, a_code_block_with_a_language_is_highlighted)
{
	auto const fragment = fragment_of("```cpp\nreturn \"x\" < 1;\n```\n");

	ASSERT_TRUE(fragment.has_value());
	EXPECT_EQ(*fragment, "<pre class=\"highlight\"><code class=\"language-cpp\">"
	                     "<span class=\"hljs-keyword\">return</span> "
	                     "<span class=\"hljs-string\">&quot;x&quot;</span> &lt; "
	                     "<span class=\"hljs-number\">1</span>;\n</code></pre>\n");
}

TEST(render_fragment, a_language_alias_is_highlighted_and_keeps_its_own_class)
{
	auto const fragment = fragment_of("```c++\nint x;\n```\n");

	ASSERT_TRUE(fragment.has_value());
	EXPECT_TRUE(fragment->starts_with("<pre class=\"highlight\"><code class=\"language-c++\">"))
	    << *fragment;
	EXPECT_NE(fragment->find("<span class=\"hljs-type\">int</span>"), std::string::npos)
	    << *fragment;
}

TEST(render_fragment, a_code_block_in_a_quote_is_highlighted)
{
	auto const fragment = fragment_of(">\n```cpp\nreturn;\n```\n<\n");

	ASSERT_TRUE(fragment.has_value());
	EXPECT_NE(fragment->find("<span class=\"hljs-keyword\">return</span>"), std::string::npos)
	    << *fragment;
}

TEST(render_fragment, a_code_block_with_no_info_string_stays_plain)
{
	auto const fragment = fragment_of("```\nreturn \"x\" < 1;\n```\n");

	ASSERT_TRUE(fragment.has_value());
	EXPECT_EQ(*fragment, "<pre><code>return \"x\" &lt; 1;\n</code></pre>\n");
}

TEST(render_fragment, a_text_block_is_accepted_and_has_no_spans)
{
	auto const fragment = fragment_of("```text\nreturn \"x\" < 1;\n```\n");

	ASSERT_TRUE(fragment.has_value());
	EXPECT_EQ(*fragment, "<pre class=\"highlight\"><code class=\"language-text\">"
	                     "return &quot;x&quot; &lt; 1;\n</code></pre>\n");
}

TEST(render_fragment, the_same_code_in_two_languages_is_highlighted_for_each)
{
	auto const fragment = fragment_of("```cpp\nint x;\n```\n\n```text\nint x;\n```\n");

	ASSERT_TRUE(fragment.has_value());
	EXPECT_NE(fragment->find("<span class=\"hljs-type\">int</span> x;"), std::string::npos)
	    << *fragment;
	EXPECT_NE(fragment->find("<code class=\"language-text\">int x;\n"), std::string::npos)
	    << *fragment;
}

TEST(render_fragment, the_same_block_twice_is_highlighted_both_times)
{
	auto const fragment =
		fragment_of("```cpp\nreturn;\n```\n\nAnd again:\n\n```cpp\nreturn;\n```\n");

	ASSERT_TRUE(fragment.has_value());
	auto const first = fragment->find("<span class=\"hljs-keyword\">return</span>");
	ASSERT_NE(first, std::string::npos) << *fragment;
	EXPECT_NE(fragment->find("<span class=\"hljs-keyword\">return</span>", first + 1),
	          std::string::npos)
	    << *fragment;
}

TEST(render_fragment, an_unknown_language_is_reported_at_the_language)
{
	auto const result = code_error("Text.\n\n```haskell\nmain = pure ()\n```\n");

	ASSERT_TRUE(result.has_value());
	EXPECT_EQ(result.value(),
	          (blog::error{blog::errc::invalid_code, jimd::position{.line = 3, .column = 4},
	                       "unknown language"}));
	EXPECT_EQ(blog::format_error(result.value(), "post.jimd"),
	          "post.jimd:3:4: cannot highlight code: unknown language");
}

TEST(render_fragment, the_language_is_only_the_first_word_of_the_info_string)
{
	auto const result = code_error("```  haskell title=x\nmain\n```\n");

	ASSERT_TRUE(result.has_value());
	EXPECT_EQ(result.value().line, 1U);
	EXPECT_EQ(result.value().column, 6U);
	EXPECT_EQ(result.value().detail, "unknown language");

	EXPECT_TRUE(fragment_of("```cpp title=x\nint x;\n```\n").has_value());
}

TEST(render_fragment, an_unknown_language_in_a_quote_is_reported_at_the_language)
{
	auto const result = code_error(">\n```haskell\nmain\n```\n<\n");

	ASSERT_TRUE(result.has_value());
	EXPECT_EQ(result.value().code,
	          (std::variant<jimd::errc, blog::errc>{blog::errc::invalid_code}));
	EXPECT_EQ(result.value().line, 2U);
	EXPECT_EQ(result.value().column, 4U);
}

TEST(render_fragment, a_language_is_looked_up_with_any_case_like_highlight_js_does)
{
	EXPECT_TRUE(fragment_of("```CPP\nint x;\n```\n").has_value());
}

TEST(render_fragment, two_unknown_languages_report_the_first_in_the_file)
{
	auto const result = code_error("```lisp\n(a)\n```\n\n```haskell\nb\n```\n");

	ASSERT_TRUE(result.has_value());
	EXPECT_EQ(result.value().line, 1U);
	EXPECT_EQ(result.value().column, 4U);

	auto const later =
		code_error("```cpp\nint x;\n```\n\n```lisp\n(a)\n```\n\n```haskell\nb\n```\n");

	ASSERT_TRUE(later.has_value());
	EXPECT_EQ(later.value().line, 5U);
}

TEST(render_fragment, an_unknown_language_that_repeats_is_reported_at_its_first_use)
{
	auto const result = code_error("```cpp\nx\n```\n\n```lisp\n(a)\n```\n\n```lisp\n(a)\n```\n");

	ASSERT_TRUE(result.has_value());
	EXPECT_EQ(result.value().line, 5U);
}

TEST(render_fragment, the_highlighter_works_after_an_error)
{
	EXPECT_TRUE(code_error("```lisp\n(a)\n```\n").has_value());

	EXPECT_TRUE(fragment_of("```cpp\nint x;\n```\n").has_value());
}

TEST(render_fragment, code_in_a_crlf_document_is_highlighted_as_lf)
{
	auto const crlf = fragment_of("```cpp\r\nint x;\r\nint y;\r\n```\r\n");
	auto const unix = fragment_of("```cpp\nint x;\nint y;\n```\n");

	ASSERT_TRUE(crlf.has_value());
	EXPECT_EQ(crlf, unix);
}

TEST(render_fragment, maths_is_checked_before_code)
{
	auto const result = code_error("```lisp\n(a)\n```\n\n$\\foo$\n");

	ASSERT_TRUE(result.has_value());
	EXPECT_EQ(result.value().code,
	          (std::variant<jimd::errc, blog::errc>{blog::errc::invalid_math}));
}

TEST(render_fragment, a_tree_not_made_from_the_source_has_its_code_error_without_a_place)
{
	jimd::ast::document doc;
	doc.blocks.emplace_back(jimd::ast::code_block{.info = "lisp"sv, .body = "(a)\n"sv});

	auto const result = blog::render_fragment(doc, context());

	ASSERT_FALSE(result.has_value());
	EXPECT_EQ(result.error(),
	          (blog::error{blog::errc::invalid_code, jimd::position{}, "unknown language"}));
}
