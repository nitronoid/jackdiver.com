#include <gtest/gtest.h>

#include <optional>
#include <string>
#include <string_view>
#include <tuple>
#include <utility>
#include <vector>

#include <jimd/ast.hxx>
#include <support/ast_printers.hxx>
#include <support/result_printers.hxx>
#include <jimd/parse.hxx>

using namespace std::literals;

namespace
{
auto make_resources(std::flat_map<std::string_view, std::string_view> links = {},
                    std::flat_map<std::string_view, jimd::ast::image_def> images = {},
                    std::flat_map<std::string_view, jimd::ast::inlines> footnotes = {})
	-> jimd::ast::resources
{
	jimd::ast::resources res;
	res.links = std::move(links);
	res.images = std::move(images);
	res.footnotes = std::move(footnotes);
	return res;
}

auto text(std::string_view value) -> jimd::ast::text
{
	return jimd::ast::text{.value = value};
}

auto make_doc(std::string_view source, std::vector<jimd::ast::block> blocks,
              jimd::ast::resources defs = {}) -> jimd::ast::document
{
	jimd::ast::document doc;
	doc.source = source;
	doc.blocks = std::move(blocks);
	doc.defs = std::move(defs);
	return doc;
}

auto first_image(jimd::ast::inlines const& nodes) -> jimd::ast::image const*
{
	for (auto const& node : nodes) {
		if (auto const* const image = std::get_if<jimd::ast::image>(&node)) {
			return image;
		}
	}
	return nullptr;
}

auto first_image(std::vector<jimd::ast::block> const& blocks) -> jimd::ast::image const*
{
	for (auto const& block : blocks) {
		jimd::ast::image const* found = nullptr;
		if (auto const* const para = std::get_if<jimd::ast::paragraph>(&block)) {
			found = first_image(para->children);
		} else if (auto const* const head = std::get_if<jimd::ast::heading>(&block)) {
			found = first_image(head->children);
		} else if (auto const* const items = std::get_if<jimd::ast::list>(&block)) {
			for (auto const& item : items->items) {
				found = found != nullptr ? found : first_image(item);
			}
		} else if (auto const* const quote = std::get_if<jimd::ast::quote>(&block)) {
			found = first_image(quote->children);
		}
		if (found != nullptr) {
			return found;
		}
	}
	return nullptr;
}

auto given(jimd::ast::image const& image) -> jimd::ast::image_def
{
	return jimd::ast::image_def{.target = image.target.value_or(""), .width = image.width};
}

struct error_case {
	std::string_view input;
	std::size_t line;
	std::size_t column;
};
}

TEST(parse, hello)
{
	using namespace jimd;

	static constexpr auto input = "hello"sv;
	auto const result = parse(input);
	ASSERT_TRUE(result.has_value());

	auto const expected =
		make_doc(input, {ast::paragraph{.children = {ast::text{.value = "hello"sv}}}});
	EXPECT_EQ(*result, expected);
}

TEST(parse, empty_document)
{
	using namespace jimd;

	for (auto const input : {""sv, "\n\n"sv}) {
		auto const result = parse(input);
		ASSERT_TRUE(result.has_value());
		EXPECT_EQ(*result, make_doc(input, {}));
	}
}

TEST(parse, quote_simple)
{
	using namespace jimd;

	static constexpr auto input = ">\nhello\n<\n"sv;
	auto const result = parse(input);
	ASSERT_TRUE(result.has_value());

	auto const expected = make_doc(
		input,
		{ast::quote{.children = {ast::paragraph{.children = {ast::text{.value = "hello"sv}}}}}});
	EXPECT_EQ(*result, expected);
}

TEST(parse, quote_multiline_paragraph)
{
	using namespace jimd;

	static constexpr auto input = ">\nline 1\nline 2\n<\n"sv;
	auto const result = parse(input);
	ASSERT_TRUE(result.has_value());

	auto const expected =
		make_doc(input, {ast::quote{.children = {ast::paragraph{.children = {
																	ast::text{.value = "line 1"sv},
																	ast::soft_break{},
																	ast::text{.value = "line 2"sv},
																}}}}});
	EXPECT_EQ(*result, expected);
}

TEST(parse, quote_multiple_paragraphs)
{
	using namespace jimd;

	static constexpr auto input = ">\npara 1\n\npara 2\n<\n"sv;
	auto const result = parse(input);
	ASSERT_TRUE(result.has_value());

	auto const expected = make_doc(
		input, {ast::quote{.children = {
							   ast::paragraph{.children = {ast::text{.value = "para 1"sv}}},
							   ast::paragraph{.children = {ast::text{.value = "para 2"sv}}},
						   }}});
	EXPECT_EQ(*result, expected);
}

TEST(parse, quote_nested_hierarchical)
{
	using namespace jimd;

	static constexpr auto input = ">\nouter\n>\ninner\n<\nouter again\n<\n"sv;
	auto const result = parse(input);
	ASSERT_TRUE(result.has_value());

	auto const expected = make_doc(
		input,
		{ast::quote{
			.children = {
				ast::paragraph{.children = {ast::text{.value = "outer"sv}}},
				ast::quote{
					.children = {ast::paragraph{.children = {ast::text{.value = "inner"sv}}}},
				},
				ast::paragraph{.children = {ast::text{.value = "outer again"sv}}},
			}}});
	EXPECT_EQ(*result, expected);
}

TEST(parse, quote_nested_double)
{
	using namespace jimd;

	static constexpr auto input = ">\n>\nnested\n<\n<\n"sv;
	auto const result = parse(input);
	ASSERT_TRUE(result.has_value());

	auto const expected = make_doc(
		input, {ast::quote{.children = {ast::quote{.children = {ast::paragraph{
													   .children = {ast::text{.value = "nested"sv}},
												   }}}}}});
	EXPECT_EQ(*result, expected);
}

TEST(parse, quote_nested_triple)
{
	using namespace jimd;

	static constexpr auto input = ">\n>\n>\ndeeply nested\n<\n<\n<\n"sv;
	auto const result = parse(input);
	ASSERT_TRUE(result.has_value());

	auto const expected =
		make_doc(input, {ast::quote{.children = {ast::quote{
										.children = {ast::quote{.children = {ast::paragraph{
																	.children = {ast::text{
																		.value = "deeply nested"sv,
																	}},
																}}}}}}}});
	EXPECT_EQ(*result, expected);
}

TEST(parse, unclosed_quote_error)
{
	using namespace jimd;

	static constexpr auto input = ">\nhello\n"sv;
	auto const result = parse(input);
	ASSERT_FALSE(result.has_value());
	EXPECT_EQ(result.error(), (error{.code = errc::unclosed_quote, .line = 3, .column = 1}));
}

TEST(parse, a_syntax_error_in_a_closed_quote_is_not_an_unclosed_quote)
{
	using namespace jimd;

	for (auto const& [input, line, column] : {
			 error_case{.input = ">\na `\n<\n"sv, .line = 2, .column = 3},
			 error_case{.input = ">\n```a`b\n<\n"sv, .line = 2, .column = 1},
			 error_case{.input = ">\n>\nx\n#tag `\n<\n<\n"sv, .line = 4, .column = 6},
		 }) {
		auto const result = parse(input);
		ASSERT_FALSE(result.has_value()) << input;
		EXPECT_EQ(result.error(), (error{.code = errc::syntax, .line = line, .column = column}))
		    << input;
	}
}

TEST(parse, nested_quotes_with_paragraphs_parse_in_linear_time)
{
	using namespace jimd;

	constexpr auto depth = 40;
	std::string input;
	for (auto i = 0; i < depth; ++i) {
		input += ">\na\n";
	}
	for (auto i = 0; i < depth; ++i) {
		input += "<\n";
	}
	EXPECT_TRUE(parse(input).has_value());
}

TEST(parse, a_blank_before_a_lone_angle_bracket_makes_it_text)
{
	using namespace jimd;

	static constexpr auto input = "a\n >\n <\n"sv;
	auto const result = parse(input);
	ASSERT_TRUE(result.has_value());
	EXPECT_EQ(*result,
	          make_doc(input, {ast::paragraph{.children = {text("a"), ast::soft_break{}, text(" >"),
	                                                       ast::soft_break{}, text(" <")}}}));
}

TEST(parse, syntax_error_position)
{
	using namespace jimd;

	static constexpr auto input = "text\n<\n"sv;
	auto const result = parse(input);
	ASSERT_FALSE(result.has_value());
	EXPECT_EQ(result.error(), (error{.code = errc::syntax, .line = 2, .column = 1}));
}

TEST(parse, header)
{
	using namespace jimd;

	static constexpr auto input = "# Title\n"sv;
	auto const result = parse(input);
	ASSERT_TRUE(result.has_value());

	auto const expected =
		make_doc(input, {ast::heading{.level = 1, .children = {ast::text{.value = "Title"sv}}}});
	EXPECT_EQ(*result, expected);
}

TEST(parse, header_with_trailing_hash)
{
	using namespace jimd;

	static constexpr auto input = "# Title #\n"sv;
	auto const result = parse(input);
	ASSERT_TRUE(result.has_value());

	auto const expected =
		make_doc(input, {ast::heading{.level = 1, .children = {ast::text{.value = "Title #"sv}}}});
	EXPECT_EQ(*result, expected);
}

TEST(parse, header_multi_level)
{
	using namespace jimd;

	static constexpr auto input = "## Subtitle\n"sv;
	auto const result = parse(input);
	ASSERT_TRUE(result.has_value());

	auto const expected =
		make_doc(input, {ast::heading{.level = 2, .children = {ast::text{.value = "Subtitle"sv}}}});
	EXPECT_EQ(*result, expected);
}

TEST(parse, header_deep_level)
{
	using namespace jimd;

	static constexpr auto input = "####### Level 7 Header\n"sv;
	auto const result = parse(input);
	ASSERT_TRUE(result.has_value());

	auto const expected = make_doc(
		input, {ast::heading{.level = 7, .children = {ast::text{.value = "Level 7 Header"sv}}}});
	EXPECT_EQ(*result, expected);
}

TEST(parse, header_multiple)
{
	using namespace jimd;

	static constexpr auto input = "# Heading 1\n\n## Heading 2\n\n### Heading 3\n"sv;
	auto const result = parse(input);
	ASSERT_TRUE(result.has_value());

	auto const expected = make_doc(
		input, {
				   ast::heading{.level = 1, .children = {ast::text{.value = "Heading 1"sv}}},
				   ast::heading{.level = 2, .children = {ast::text{.value = "Heading 2"sv}}},
				   ast::heading{.level = 3, .children = {ast::text{.value = "Heading 3"sv}}},
			   });
	EXPECT_EQ(*result, expected);
}

TEST(parse, code_block)
{
	using namespace jimd;

	static constexpr auto input = "```\nint x = 0;\n```\n"sv;
	auto const result = parse(input);
	ASSERT_TRUE(result.has_value());

	auto const expected =
		make_doc(input, {ast::code_block{.info = ""sv, .body = "int x = 0;\n"sv}});
	EXPECT_EQ(*result, expected);
}

TEST(parse, code_block_with_tag)
{
	using namespace jimd;

	static constexpr auto input = "```c\nint x = 0;\n```\n"sv;
	auto const result = parse(input);
	ASSERT_TRUE(result.has_value());
	EXPECT_EQ(*result, make_doc(input, {ast::code_block{.info = "c"sv, .body = "int x = 0;\n"sv}}));
}

TEST(parse, code_block_info_keeps_inner_blanks_and_drops_outer_ones)
{
	using namespace jimd;

	static constexpr auto input = "``` \tcpp title=x \t\nbody\n```\n"sv;
	auto const result = parse(input);
	ASSERT_TRUE(result.has_value());
	EXPECT_EQ(*result,
	          make_doc(input, {ast::code_block{.info = "cpp title=x"sv, .body = "body\n"sv}}));
}

TEST(parse, code_block_closed_at_end_of_input_without_a_newline)
{
	using namespace jimd;

	static constexpr auto input = "```\nx\n```"sv;
	auto const result = parse(input);
	ASSERT_TRUE(result.has_value());
	EXPECT_EQ(*result, make_doc(input, {ast::code_block{.info = ""sv, .body = "x\n"sv}}));
}

TEST(parse, code_block_empty_body)
{
	using namespace jimd;

	for (auto const input : {"```\n```\n"sv, "```\n```"sv}) {
		auto const result = parse(input);
		ASSERT_TRUE(result.has_value()) << input;
		EXPECT_EQ(*result, make_doc(input, {ast::code_block{.info = ""sv, .body = ""sv}}));
	}
	static constexpr auto tagged = "```c\n```\n"sv;
	auto const result = parse(tagged);
	ASSERT_TRUE(result.has_value());
	EXPECT_EQ(*result, make_doc(tagged, {ast::code_block{.info = "c"sv, .body = ""sv}}));
}

TEST(parse, code_block_closing_fence_may_have_trailing_blanks)
{
	using namespace jimd;

	static constexpr auto input = "```\nx\n``` \t\nafter\n"sv;
	auto const result = parse(input);
	ASSERT_TRUE(result.has_value());
	EXPECT_EQ(*result,
	          make_doc(input, {ast::code_block{.info = ""sv, .body = "x\n"sv},
	                           ast::paragraph{.children = {ast::text{.value = "after"sv}}}}));
}

TEST(parse, code_block_crlf)
{
	using namespace jimd;

	static constexpr auto input = "```rust\r\nlet x;\r\n\r\n```\r\nafter\r\n"sv;
	auto const result = parse(input);
	ASSERT_TRUE(result.has_value());
	EXPECT_EQ(*result,
	          make_doc(input, {ast::code_block{.info = "rust"sv, .body = "let x;\r\n\r\n"sv},
	                           ast::paragraph{.children = {ast::text{.value = "after"sv}}}}));
}

TEST(parse, code_block_info_keeps_a_lone_cr)
{
	using namespace jimd;

	static constexpr auto input = "```a\rb\nx\n```\n"sv;
	auto const result = parse(input);
	ASSERT_TRUE(result.has_value());
	EXPECT_EQ(*result, make_doc(input, {ast::code_block{.info = "a\rb"sv, .body = "x\n"sv}}));
}

TEST(parse, code_block_body_is_not_markup_and_only_a_bare_fence_closes_it)
{
	using namespace jimd;

	static constexpr auto input = "```\n# not a heading\n_open\n```c\n  ```\n````\n```\n"sv;
	auto const result = parse(input);
	ASSERT_TRUE(result.has_value());
	EXPECT_EQ(*result,
	          make_doc(input,
	                   {ast::code_block{.info = ""sv,
	                                    .body = "# not a heading\n_open\n```c\n  ```\n````\n"sv}}));
}

TEST(parse, code_block_interrupts_a_paragraph)
{
	using namespace jimd;

	static constexpr auto input = "text\n```c\nx\n```\n"sv;
	auto const result = parse(input);
	ASSERT_TRUE(result.has_value());
	EXPECT_EQ(*result, make_doc(input, {ast::paragraph{.children = {ast::text{.value = "text"sv}}},
	                                    ast::code_block{.info = "c"sv, .body = "x\n"sv}}));
}

TEST(parse, code_block_inside_a_quote)
{
	using namespace jimd;

	static constexpr auto input = ">\n```c\nx\n```\n<\n"sv;
	auto const result = parse(input);
	ASSERT_TRUE(result.has_value());
	EXPECT_EQ(*result, make_doc(input, {ast::quote{.children = {ast::code_block{
													   .info = "c"sv, .body = "x\n"sv}}}}));
}

TEST(parse, unclosed_code_block_error)
{
	using namespace jimd;

	for (auto const input :
	     {"```\nx\n"sv, "```c\nx"sv, "```\n"sv, "```"sv, "```c "sv, "text\n\n```\n"sv}) {
		auto const result = parse(input);
		ASSERT_FALSE(result.has_value()) << input;
		EXPECT_EQ(result.error().code, errc::unclosed_code_block) << input;
	}
	auto const result = parse("```c\nx\n"sv);
	ASSERT_FALSE(result.has_value());
	EXPECT_EQ(result.error(), (error{.code = errc::unclosed_code_block, .line = 3, .column = 1}));
}

TEST(parse, unclosed_code_block_in_a_quote_is_not_an_unclosed_quote)
{
	using namespace jimd;

	auto const result = parse(">\n```\nx\n<\n"sv);
	ASSERT_FALSE(result.has_value());
	EXPECT_EQ(result.error().code, errc::unclosed_code_block);
}

TEST(parse, longer_and_indented_fences_are_not_supported)
{
	using namespace jimd;

	for (auto const input : {"````\nx\n````\n"sv, "  ```\nx\n  ```\n"sv}) {
		auto const result = parse(input);
		ASSERT_FALSE(result.has_value()) << input;
		EXPECT_EQ(result.error().code, errc::syntax) << input;
	}
}

TEST(parse, a_fence_with_a_backtick_in_its_line_is_not_a_code_block)
{
	using namespace jimd;

	auto const result = parse("```a`b\n"sv);
	ASSERT_FALSE(result.has_value());
	EXPECT_EQ(result.error().code, errc::syntax);
}

TEST(parse, list_items_are_inline_content)
{
	using namespace jimd;

	static constexpr auto input = "- one\n- _two_ and *three*\n- `four` \\* [@k]\n\n[@k]: u\n"sv;
	auto const result = parse(input);
	ASSERT_TRUE(result.has_value());
	EXPECT_EQ(
		*result,
		make_doc(input,
		         {ast::list{
					 .items = {{text("one")},
					           {ast::emphasis{.children = {text("two")}}, text(" and "),
					            ast::strong{.children = {text("three")}}},
					           {ast::code_span{.value = "four"sv}, text(" "), text("*"), text(" "),
					            ast::link{.key = "k"sv, .text = "k"sv, .target = std::nullopt}}}}},
		         make_resources({{"k"sv, "u"sv}})));
}

TEST(parse, list_item_blanks_after_the_dash_are_dropped)
{
	using namespace jimd;

	static constexpr auto input = "-\ta\n-   b  \n"sv;
	auto const result = parse(input);
	ASSERT_TRUE(result.has_value());
	EXPECT_EQ(*result, make_doc(input, {ast::list{.items = {{text("a")}, {text("b  ")}}}}));
}

TEST(parse, list_interrupts_a_paragraph)
{
	using namespace jimd;

	static constexpr auto input = "text:\n- a\n- b\n"sv;
	auto const result = parse(input);
	ASSERT_TRUE(result.has_value());
	EXPECT_EQ(*result, make_doc(input, {ast::paragraph{.children = {text("text:")}},
	                                    ast::list{.items = {{text("a")}, {text("b")}}}}));
}

TEST(parse, any_other_line_ends_a_list)
{
	using namespace jimd;

	static constexpr auto input = "- a\ntext\n- b\n\n- c\n# h\n- d\n```\nx\n```\n- e\n"sv;
	auto const result = parse(input);
	ASSERT_TRUE(result.has_value());
	EXPECT_EQ(
		*result,
		make_doc(input,
		         {ast::list{.items = {{text("a")}}}, ast::paragraph{.children = {text("text")}},
		          ast::list{.items = {{text("b")}}}, ast::list{.items = {{text("c")}}},
		          ast::heading{.level = 1, .children = {text("h")}},
		          ast::list{.items = {{text("d")}}}, ast::code_block{.info = ""sv, .body = "x\n"sv},
		          ast::list{.items = {{text("e")}}}}));
}

TEST(parse, list_inside_a_quote)
{
	using namespace jimd;

	static constexpr auto input = ">\ntext\n- a\n- b\n<\n- c\n"sv;
	auto const result = parse(input);
	ASSERT_TRUE(result.has_value());
	EXPECT_EQ(*result,
	          make_doc(input,
	                   {ast::quote{.children = {ast::paragraph{.children = {text("text")}},
	                                            ast::list{.items = {{text("a")}, {text("b")}}}}},
	                    ast::list{.items = {{text("c")}}}}));
}

TEST(parse, a_footnote_in_a_list_item)
{
	using namespace jimd;

	static constexpr auto input = "- a[^n] b[^m](inline)\n\n[^n]: defined\n"sv;
	auto const result = parse(input);
	ASSERT_TRUE(result.has_value());
	EXPECT_EQ(
		*result,
		make_doc(input,
		         {ast::list{
					 .items = {{text("a"), ast::reference{.key = "n"sv, .content = {}}, text(" b"),
					            ast::reference{.key = "m"sv,
					                           .content = ast::inlines{text("inline")}}}}}},
		         make_resources({}, {}, {{"n"sv, ast::inlines{text("defined")}}})));
}

TEST(parse, a_dash_without_content_is_text)
{
	using namespace jimd;

	for (auto const input : {"-5"sv, "-"sv, "--"sv, "---"sv, "-a"sv, "- "sv, "-\t"sv, "-  "sv}) {
		auto const result = parse(input);
		ASSERT_TRUE(result.has_value()) << input;
		EXPECT_EQ(*result, make_doc(input, {ast::paragraph{.children = {text(input)}}})) << input;
	}

	static constexpr auto input = "a\n-\n- \n-5\nb - c\n"sv;
	auto const result = parse(input);
	ASSERT_TRUE(result.has_value());
	EXPECT_EQ(*result,
	          make_doc(input, {ast::paragraph{.children = {text("a"), ast::soft_break{}, text("-"),
	                                                       ast::soft_break{}, text("- "),
	                                                       ast::soft_break{}, text("-5"),
	                                                       ast::soft_break{}, text("b - c")}}}));

	static constexpr auto before_list = "- \n- a\n"sv;
	auto const listed = parse(before_list);
	ASSERT_TRUE(listed.has_value());
	EXPECT_EQ(*listed, make_doc(before_list, {ast::paragraph{.children = {text("- ")}},
	                                          ast::list{.items = {{text("a")}}}}));
}

TEST(parse, an_escaped_dash_is_text)
{
	using namespace jimd;

	static constexpr auto input = "\\- a\nb\n\\- c\n"sv;
	auto const result = parse(input);
	ASSERT_TRUE(result.has_value());
	EXPECT_EQ(*result,
	          make_doc(input, {ast::paragraph{.children = {text("-"), text(" a"), ast::soft_break{},
	                                                       text("b"), ast::soft_break{}, text("-"),
	                                                       text(" c")}}}));

	static constexpr auto inside = "_a\\-b_ [^n](\\-)"sv;
	auto const nested = parse(inside);
	ASSERT_TRUE(nested.has_value());
	auto const expected = ast::paragraph{
		.children = {ast::emphasis{.children = {text("a"), text("-"), text("b")}}, text(" "),
		             ast::reference{.key = "n"sv, .content = ast::inlines{text("-")}}}};
	EXPECT_EQ(nested->blocks, (std::vector<ast::block>{expected}));
}

TEST(parse, list_item_errors_point_into_the_item)
{
	using namespace jimd;

	struct item_error {
		std::string_view input;
		errc code;
		std::size_t line;
		std::size_t column;
	};
	for (auto const& [input, code, line, column] : {
			 item_error{
				 .input = "- a \\q\n"sv, .code = errc::invalid_escape, .line = 1, .column = 5},
			 item_error{
				 .input = "- a\n- b \\\n"sv, .code = errc::invalid_escape, .line = 2, .column = 5},
			 item_error{
				 .input = "- a\n- _b\n"sv, .code = errc::unclosed_emphasis, .line = 2, .column = 5},
			 item_error{
				 .input = "- [@a\n"sv, .code = errc::unclosed_resource, .line = 1, .column = 6},
			 item_error{
				 .input = "- [@a]\n"sv, .code = errc::undefined_reference, .line = 1, .column = 5},
			 item_error{
				 .input = "- a\n- [@ ](u)\n"sv, .code = errc::empty_link, .line = 2, .column = 6},
			 item_error{.input = "- a\n- [^]\n"sv, .code = errc::empty_key, .line = 2, .column = 5},
			 item_error{.input = "- [^n](x [^m])\n"sv,
			            .code = errc::nested_footnote,
			            .line = 1,
			            .column = 12},
			 item_error{.input = "- `a\n"sv, .code = errc::syntax, .line = 1, .column = 3},
			 item_error{.input = "- a `b\n"sv, .code = errc::syntax, .line = 1, .column = 5},
			 item_error{.input = "text\n- `a\n"sv, .code = errc::syntax, .line = 2, .column = 3},
			 item_error{.input = "- a\n- b `c\n"sv, .code = errc::syntax, .line = 2, .column = 5},
		 }) {
		auto const result = parse(input);
		ASSERT_FALSE(result.has_value()) << input;
		EXPECT_EQ(result.error(), (error{.code = code, .line = line, .column = column})) << input;
	}
}

TEST(parse, inline_math_is_verbatim_tex)
{
	using namespace jimd;

	static constexpr auto input = "a $x_1^2 \\* [@k] _z_ `c`$ b"sv;
	auto const result = parse(input);
	ASSERT_TRUE(result.has_value());
	EXPECT_EQ(*result,
	          make_doc(input,
	                   {ast::paragraph{.children = {text("a "),
	                                                ast::math{.tex = "x_1^2 \\* [@k] _z_ `c`"sv},
	                                                text(" b")}}}));
	auto const& children = std::get<ast::paragraph>(result->blocks.front()).children;
	EXPECT_EQ(std::get<ast::math>(children.at(1)).tex.data(), input.substr(3, 1).data());
}

TEST(parse, inline_math_needs_no_blanks_around_it)
{
	using namespace jimd;

	static constexpr auto input = "a$x$b$y$$z$ $ w $"sv;
	auto const result = parse(input);
	ASSERT_TRUE(result.has_value());
	EXPECT_EQ(*result,
	          make_doc(input, {ast::paragraph{.children = {text("a"), ast::math{.tex = "x"sv},
	                                                       text("b"), ast::math{.tex = "y"sv},
	                                                       ast::math{.tex = "z"sv}, text(" "),
	                                                       ast::math{.tex = " w "sv}}}}));
}

TEST(parse, a_backslash_before_the_closing_dollar_does_not_escape_it)
{
	using namespace jimd;

	static constexpr auto input = "$a\\$b"sv;
	auto const result = parse(input);
	ASSERT_TRUE(result.has_value());
	EXPECT_EQ(*result,
	          make_doc(input,
	                   {ast::paragraph{.children = {ast::math{.tex = "a\\"sv}, text("b")}}}));
}

TEST(parse, math_inside_emphasis_and_strong)
{
	using namespace jimd;

	static constexpr auto input = "_a $x$ b_ *$y$* _$z_1$_"sv;
	auto const result = parse(input);
	ASSERT_TRUE(result.has_value());
	EXPECT_EQ(
		*result,
		make_doc(
			input,
			{ast::paragraph{
				.children = {
					ast::emphasis{.children = {text("a "), ast::math{.tex = "x"sv}, text(" b")}},
					text(" "), ast::strong{.children = {ast::math{.tex = "y"sv}}}, text(" "),
					ast::emphasis{.children = {ast::math{.tex = "z_1"sv}}}}}}));
}

TEST(parse, math_inside_a_footnote)
{
	using namespace jimd;

	static constexpr auto input = "a[^n](see $f(x)$) b[^m]\n\n[^m]: $y$ and _$z$_\n"sv;
	auto const result = parse(input);
	ASSERT_TRUE(result.has_value());
	EXPECT_EQ(
		*result,
		make_doc(
			input,
			{ast::paragraph{
				.children = {text("a"),
				             ast::reference{.key = "n"sv,
				                            .content = ast::inlines{text("see "),
				                                                    ast::math{.tex = "f(x)"sv}}},
				             text(" b"), ast::reference{.key = "m"sv, .content = {}}}}},
			make_resources(
				{}, {},
				{{"m"sv, ast::inlines{ast::math{.tex = "y"sv}, text(" and "),
				                      ast::emphasis{.children = {ast::math{.tex = "z"sv}}}}}})));
}

TEST(parse, math_in_a_heading_a_list_item_and_a_quote)
{
	using namespace jimd;

	static constexpr auto input = "# H $x$\n- i $y$\n>\nq $z$\n<\n"sv;
	auto const result = parse(input);
	ASSERT_TRUE(result.has_value());
	EXPECT_EQ(*result,
	          make_doc(input,
	                   {ast::heading{.level = 1, .children = {text("H "), ast::math{.tex = "x"sv}}},
	                    ast::list{.items = {{text("i "), ast::math{.tex = "y"sv}}}},
	                    ast::quote{.children = {ast::paragraph{
									   .children = {text("q "), ast::math{.tex = "z"sv}}}}}}));
}

TEST(parse, math_in_link_text_and_code_is_not_math)
{
	using namespace jimd;

	static constexpr auto input = "[@a $b](u) `$c` [!i $d](p)"sv;
	auto const result = parse(input);
	ASSERT_TRUE(result.has_value());
	EXPECT_EQ(
		*result,
		make_doc(input,
		         {ast::paragraph{
					 .children = {ast::link{.key = "a $b"sv, .text = "a $b"sv, .target = "u"sv},
					              text(" "), ast::code_span{.value = "$c"sv}, text(" "),
					              ast::image{.key = "i $d"sv,
					                         .alt = "i $d"sv,
					                         .target = "p"sv,
					                         .width = std::nullopt}}}}));
}

TEST(parse, an_escaped_dollar_is_text)
{
	using namespace jimd;

	static constexpr auto input = R"(costs \$5 and \$6, or $x$ \$)"sv;
	auto const result = parse(input);
	ASSERT_TRUE(result.has_value());
	EXPECT_EQ(*result,
	          make_doc(input, {ast::paragraph{.children = {text("costs "), text("$"),
	                                                       text("5 and "), text("$"),
	                                                       text("6, or "), ast::math{.tex = "x"sv},
	                                                       text(" "), text("$")}}}));
	auto const& children = std::get<ast::paragraph>(result->blocks.front()).children;
	EXPECT_EQ(std::get<ast::text>(children.at(1)).value.data(), input.substr(7, 1).data());
}

TEST(parse, an_escaped_dollar_in_a_heading_an_item_and_a_definition)
{
	using namespace jimd;

	static constexpr auto input = "# \\$1\n- \\$2\n\n[^n]: \\$3\n"sv;
	auto const result = parse(input);
	ASSERT_TRUE(result.has_value());
	EXPECT_EQ(*result,
	          make_doc(input,
	                   {ast::heading{.level = 1, .children = {text("$"), text("1")}},
	                    ast::list{.items = {{text("$"), text("2")}}}},
	                   make_resources({}, {}, {{"n"sv, ast::inlines{text("$"), text("3")}}})));
}

TEST(parse, a_dollar_in_a_metadata_value_a_target_or_a_definition_is_not_math)
{
	using namespace jimd;

	static constexpr auto input =
		"[%price]: $5 and $\n\n[@a](u$v) [@b] [!i]\n\n[@b]: w$x\n[!i]: p$q\n"sv;
	auto const result = parse(input);
	ASSERT_TRUE(result.has_value());
	EXPECT_EQ(result->metadata.at("price"), "$5 and $");
	auto const& children = std::get<ast::paragraph>(result->blocks.front()).children;
	EXPECT_EQ(std::get<ast::link>(children.front()).target, "u$v");
	EXPECT_EQ(result->defs.links.at("b"), "w$x");
	EXPECT_EQ(result->defs.images.at("i").target, "p$q");
}

TEST(parse, an_escaped_dollar_inside_emphasis_and_a_footnote)
{
	using namespace jimd;

	static constexpr auto input = "_a\\$b_ [^n](\\$)"sv;
	auto const result = parse(input);
	ASSERT_TRUE(result.has_value());
	EXPECT_EQ(
		*result,
		make_doc(input,
		         {ast::paragraph{
					 .children = {
						 ast::emphasis{.children = {text("a"), text("$"), text("b")}}, text(" "),
						 ast::reference{.key = "n"sv, .content = ast::inlines{text("$")}}}}}));
}

TEST(parse, unclosed_math_error)
{
	using namespace jimd;

	for (auto const& [input, line, column] : {
			 error_case{.input = "$a"sv, .line = 1, .column = 3},
			 error_case{.input = "$"sv, .line = 1, .column = 2},
			 error_case{.input = "a $"sv, .line = 1, .column = 4},
			 error_case{.input = "a $b\nc$"sv, .line = 1, .column = 5},
			 error_case{.input = "a $b\r\nc$"sv, .line = 1, .column = 5},
			 error_case{.input = "x\ny $z"sv, .line = 2, .column = 5},
			 error_case{.input = "_a $x_"sv, .line = 1, .column = 7},
			 error_case{.input = "# Cost $5\n"sv, .line = 1, .column = 10},
			 error_case{.input = "- Cost $5\n"sv, .line = 1, .column = 10},
			 error_case{.input = "[^n](a $b)"sv, .line = 1, .column = 11},
			 error_case{.input = "[^n]: a $b\n"sv, .line = 1, .column = 11},
			 error_case{.input = ">\nCost $5\n<\n"sv, .line = 2, .column = 8},
		 }) {
		auto const result = parse(input);
		ASSERT_FALSE(result.has_value()) << input;
		EXPECT_EQ(result.error(),
		          (error{.code = errc::unclosed_math, .line = line, .column = column}))
		    << input;
	}
}

TEST(parse, empty_math_error)
{
	using namespace jimd;

	for (auto const& [input, line, column] : {
			 error_case{.input = "a $$ b"sv, .line = 1, .column = 3},
			 error_case{.input = "x\n_y $$ z_"sv, .line = 2, .column = 4},
			 error_case{.input = "# a $$\n"sv, .line = 1, .column = 5},
			 error_case{.input = "[^n](a $$)"sv, .line = 1, .column = 8},
			 error_case{.input = "$ $"sv, .line = 1, .column = 1},
			 error_case{.input = "a $ \t $ b"sv, .line = 1, .column = 3},
			 error_case{.input = "_a $  $_"sv, .line = 1, .column = 4},
			 error_case{.input = "[^n](a $ $)"sv, .line = 1, .column = 8},
		 }) {
		auto const result = parse(input);
		ASSERT_FALSE(result.has_value()) << input;
		EXPECT_EQ(result.error(), (error{.code = errc::empty_math, .line = line, .column = column}))
		    << input;
	}
}

TEST(parse, display_math_body_is_verbatim_tex_with_its_line_ends)
{
	using namespace jimd;

	static constexpr auto input = "$$\nx_1 \\* [@k] _z_ `c`\n\n  \\frac{a}{b}\n$$\n"sv;
	auto const result = parse(input);
	ASSERT_TRUE(result.has_value());
	EXPECT_EQ(*result, make_doc(input, {ast::display_math{.tex = "x_1 \\* [@k] _z_ `c`\n\n  "
	                                                             "\\frac{a}{b}\n"sv}}));
	auto const& body = std::get<ast::display_math>(result->blocks.front()).tex;
	EXPECT_EQ(body.data(), input.substr(3).data());
}

TEST(parse, display_math_fences_may_have_trailing_blanks_and_end_the_input)
{
	using namespace jimd;

	for (auto const input : {"$$ \t\nx\n$$  \n"sv, "$$\nx\n$$"sv, "$$\nx\n$$\n"sv}) {
		auto const result = parse(input);
		ASSERT_TRUE(result.has_value()) << input;
		EXPECT_EQ(*result, make_doc(input, {ast::display_math{.tex = "x\n"sv}})) << input;
	}
}

TEST(parse, display_math_keeps_carriage_returns_in_its_body)
{
	using namespace jimd;

	static constexpr auto input = "$$ \r\na\r\nb\r\n$$\t\r\ntext\r\n"sv;
	auto const result = parse(input);
	ASSERT_TRUE(result.has_value());
	EXPECT_EQ(*result, make_doc(input, {ast::display_math{.tex = "a\r\nb\r\n"sv},
	                                    ast::paragraph{.children = {text("text")}}}));
}

TEST(parse, display_math_interrupts_a_paragraph_and_a_definition_may_follow_it)
{
	using namespace jimd;

	static constexpr auto input = "text:\n$$\nx\n$$\n[@k]: u\nafter [@k]\n"sv;
	auto const result = parse(input);
	ASSERT_TRUE(result.has_value());
	EXPECT_EQ(
		*result,
		make_doc(input,
		         {ast::paragraph{.children = {text("text:")}}, ast::display_math{.tex = "x\n"sv},
		          ast::paragraph{.children = {text("after "), ast::link{.key = "k"sv,
		                                                                .text = "k"sv,
		                                                                .target = std::nullopt}}}},
		         make_resources({{"k"sv, "u"sv}})));
}

TEST(parse, display_math_fences_work_back_to_back_and_after_a_heading_or_a_code_block)
{
	using namespace jimd;

	static constexpr auto input = "# h\n$$\nx\n$$\n$$\ny\n$$\n```\n$$\n```\n$$\nz\n$$"sv;
	auto const result = parse(input);
	ASSERT_TRUE(result.has_value());
	EXPECT_EQ(*result,
	          make_doc(input, {ast::heading{.level = 1, .children = {text("h")}},
	                           ast::display_math{.tex = "x\n"sv}, ast::display_math{.tex = "y\n"sv},
	                           ast::code_block{.info = ""sv, .body = "$$\n"sv},
	                           ast::display_math{.tex = "z\n"sv}}));
}

TEST(parse, display_math_inside_a_quote_and_after_a_list)
{
	using namespace jimd;

	static constexpr auto input = ">\n$$\nx\n$$\n<\n- a\n$$\ny\n$$\n"sv;
	auto const result = parse(input);
	ASSERT_TRUE(result.has_value());
	EXPECT_EQ(*result, make_doc(input, {ast::quote{.children = {ast::display_math{.tex = "x\n"sv}}},
	                                    ast::list{.items = {{text("a")}}},
	                                    ast::display_math{.tex = "y\n"sv}}));
}

TEST(parse, only_a_line_of_two_dollars_and_blanks_is_a_display_math_fence)
{
	using namespace jimd;

	static constexpr auto input = "$$\n$$$\n$$ x\n  $$\n$ $$\n$$\n"sv;
	auto const result = parse(input);
	ASSERT_TRUE(result.has_value());
	EXPECT_EQ(*result, make_doc(input, {ast::display_math{.tex = "$$$\n$$ x\n  $$\n$ $$\n"sv}}));

	for (auto const& [text_input, column] : {
			 std::pair{"$$ x"sv, 1U},
			 std::pair{"  $$"sv, 3U},
			 std::pair{"a $$"sv, 3U},
		 }) {
		auto const failed = parse(text_input);
		ASSERT_FALSE(failed.has_value()) << text_input;
		EXPECT_EQ(failed.error(), (error{.code = errc::empty_math, .line = 1, .column = column}))
		    << text_input;
	}
}

TEST(parse, a_dollar_quad_line_or_a_dollar_triple_closer_is_not_a_display_math_fence)
{
	using namespace jimd;

	for (auto const input : {"$$\nx\n$$$\n"sv, "$$\nx\n$$$$\n"sv, "$$\n$$$\n$$$$\n"sv}) {
		auto const result = parse(input);
		ASSERT_FALSE(result.has_value()) << input;
		EXPECT_EQ(result.error().code, errc::unclosed_math_block) << input;
	}
	for (auto const input : {"$$$$\nx\n$$\n"sv, "$$\rx\n$$\n"sv}) {
		auto const result = parse(input);
		ASSERT_FALSE(result.has_value()) << input;
		EXPECT_EQ(result.error(), (error{.code = errc::empty_math, .line = 1, .column = 1}))
		    << input;
	}
}

TEST(parse, unclosed_display_math_error)
{
	using namespace jimd;

	for (auto const& [input, line, column] : {
			 error_case{.input = "$$"sv, .line = 1, .column = 3},
			 error_case{.input = "$$\n"sv, .line = 2, .column = 1},
			 error_case{.input = "$$\nx\n"sv, .line = 3, .column = 1},
			 error_case{.input = "text\n\n$$\nx $$\n"sv, .line = 5, .column = 1},
			 error_case{.input = "$$\r\nx\r\n"sv, .line = 3, .column = 1},
			 error_case{.input = ">\n$$\nx\n<\n"sv, .line = 5, .column = 1},
			 error_case{.input = "$$\nx\n\n- a\n# h\n\nmore"sv, .line = 7, .column = 5},
			 error_case{.input = "$$\nx\n$$\\\n"sv, .line = 4, .column = 1},
		 }) {
		auto const result = parse(input);
		ASSERT_FALSE(result.has_value()) << input;
		EXPECT_EQ(result.error(),
		          (error{.code = errc::unclosed_math_block, .line = line, .column = column}))
		    << input;
	}
}

TEST(parse, empty_display_math_error_points_at_the_opening_fence)
{
	using namespace jimd;

	for (auto const& [input, line, column] : {
			 error_case{.input = "$$\n$$\n"sv, .line = 1, .column = 1},
			 error_case{.input = "$$\n$$"sv, .line = 1, .column = 1},
			 error_case{.input = "$$ \n \t\n\n  \n$$\n"sv, .line = 1, .column = 1},
			 error_case{.input = "$$\r\n\r\n$$\r\n"sv, .line = 1, .column = 1},
			 error_case{.input = "text\n$$\n$$\n"sv, .line = 2, .column = 1},
			 error_case{.input = ">\n$$\n\n$$\n<\n"sv, .line = 2, .column = 1},
		 }) {
		auto const result = parse(input);
		ASSERT_FALSE(result.has_value()) << input;
		EXPECT_EQ(result.error(), (error{.code = errc::empty_math, .line = line, .column = column}))
		    << input;
	}
}

TEST(parse, aligned_math_rows_are_the_lines_of_the_body)
{
	using namespace jimd;

	static constexpr auto input = "$$$\na &= b\n  c &= d  \n$$$\n"sv;
	auto const result = parse(input);
	ASSERT_TRUE(result.has_value());
	EXPECT_EQ(*result, make_doc(input, {ast::aligned_math{.rows = {"a &= b"sv, "  c &= d  "sv}}}));
	auto const& rows = std::get<ast::aligned_math>(result->blocks.front()).rows;
	EXPECT_EQ(rows.front().data(), input.substr(4).data());
	EXPECT_EQ(rows.back().data(), input.substr(11).data());
}

TEST(parse, aligned_math_fences_may_have_trailing_blanks_and_end_the_input)
{
	using namespace jimd;

	for (auto const input : {"$$$ \t\nx\n$$$  \n"sv, "$$$\nx\n$$$"sv, "$$$\nx\n$$$\n"sv}) {
		auto const result = parse(input);
		ASSERT_TRUE(result.has_value()) << input;
		EXPECT_EQ(*result, make_doc(input, {ast::aligned_math{.rows = {"x"sv}}})) << input;
	}
}

TEST(parse, aligned_math_rows_leave_out_a_crlf_but_keep_a_lone_cr)
{
	using namespace jimd;

	static constexpr auto input = "$$$\r\na\r\n\r\nb\rc\r\r\nd\r\n$$$\r\n"sv;
	auto const result = parse(input);
	ASSERT_TRUE(result.has_value());
	EXPECT_EQ(*result,
	          make_doc(input, {ast::aligned_math{.rows = {"a"sv, ""sv, "b\rc\r"sv, "d"sv}}}));
}

TEST(parse, aligned_math_drops_blank_lines_around_the_rows_and_keeps_those_between)
{
	using namespace jimd;

	static constexpr auto input = "$$$\n\n \t\na\n\n   \n\nb\n\n  \n$$$\n"sv;
	auto const result = parse(input);
	ASSERT_TRUE(result.has_value());
	EXPECT_EQ(*result,
	          make_doc(input, {ast::aligned_math{.rows = {"a"sv, ""sv, "   "sv, ""sv, "b"sv}}}));
}

TEST(parse, aligned_math_body_is_verbatim_tex)
{
	using namespace jimd;

	static constexpr auto input = "$$$\na % note \\\\\n_x_ [@k] `c` $\n$$\n$$$$\n$$$\n"sv;
	auto const result = parse(input);
	ASSERT_TRUE(result.has_value());
	EXPECT_EQ(*result,
	          make_doc(input, {ast::aligned_math{.rows = {"a % note \\\\"sv, "_x_ [@k] `c` $"sv,
	                                                      "$$"sv, "$$$$"sv}}}));
}

TEST(parse, aligned_math_interrupts_a_paragraph_and_a_definition_may_follow_it)
{
	using namespace jimd;

	static constexpr auto input = "text:\n$$$\nx\n$$$\n[@k]: u\nafter [@k]\n"sv;
	auto const result = parse(input);
	ASSERT_TRUE(result.has_value());
	EXPECT_EQ(
		*result,
		make_doc(input,
		         {ast::paragraph{.children = {text("text:")}}, ast::aligned_math{.rows = {"x"sv}},
		          ast::paragraph{.children = {text("after "), ast::link{.key = "k"sv,
		                                                                .text = "k"sv,
		                                                                .target = std::nullopt}}}},
		         make_resources({{"k"sv, "u"sv}})));
}

TEST(parse, aligned_math_inside_a_quote_after_a_list_and_beside_display_math)
{
	using namespace jimd;

	static constexpr auto input = ">\n$$$\nx\n$$$\n<\n- a\n$$$\ny\n$$$\n$$\nz\n$$\n"sv;
	auto const result = parse(input);
	ASSERT_TRUE(result.has_value());
	EXPECT_EQ(*result,
	          make_doc(input,
	                   {ast::quote{.children = {ast::aligned_math{.rows = {"x"sv}}}},
	                    ast::list{.items = {{text("a")}}}, ast::aligned_math{.rows = {"y"sv}},
	                    ast::display_math{.tex = "z\n"sv}}));
}

TEST(parse, only_a_line_of_three_dollars_and_blanks_is_an_aligned_math_fence)
{
	using namespace jimd;

	for (auto const input :
	     {"$$$\nx\n$$\n"sv, "$$$\nx\n$$$$\n"sv, "$$$\nx\n  $$$\n"sv, "$$$\nx\n$$$ y\n"sv}) {
		auto const result = parse(input);
		ASSERT_FALSE(result.has_value()) << input;
		EXPECT_EQ(result.error().code, errc::unclosed_math_block) << input;
	}
	for (auto const& [input, column] : {
			 std::pair{"$$$ x"sv, 1U},
			 std::pair{"  $$$"sv, 3U},
			 std::pair{"a $$$"sv, 3U},
			 std::pair{"$$$$"sv, 1U},
		 }) {
		auto const failed = parse(input);
		ASSERT_FALSE(failed.has_value()) << input;
		EXPECT_EQ(failed.error(), (error{.code = errc::empty_math, .line = 1, .column = column}))
		    << input;
	}
}

TEST(parse, unclosed_aligned_math_error)
{
	using namespace jimd;

	for (auto const& [input, line, column] : {
			 error_case{.input = "$$$"sv, .line = 1, .column = 4},
			 error_case{.input = "$$$\n"sv, .line = 2, .column = 1},
			 error_case{.input = "$$$\nx\n"sv, .line = 3, .column = 1},
			 error_case{.input = "$$$\r\nx\r\n"sv, .line = 3, .column = 1},
			 error_case{.input = ">\n$$$\nx\n<\n"sv, .line = 5, .column = 1},
		 }) {
		auto const result = parse(input);
		ASSERT_FALSE(result.has_value()) << input;
		EXPECT_EQ(result.error(),
		          (error{.code = errc::unclosed_math_block, .line = line, .column = column}))
		    << input;
	}
}

TEST(parse, empty_aligned_math_error_points_at_the_opening_fence)
{
	using namespace jimd;

	for (auto const& [input, line, column] : {
			 error_case{.input = "$$$\n$$$\n"sv, .line = 1, .column = 1},
			 error_case{.input = "$$$\n$$$"sv, .line = 1, .column = 1},
			 error_case{.input = "$$$ \n \t\n\n  \n$$$\n"sv, .line = 1, .column = 1},
			 error_case{.input = "$$$\r\n\r\n$$$\r\n"sv, .line = 1, .column = 1},
			 error_case{.input = "text\n$$$\n\n$$$\n"sv, .line = 2, .column = 1},
		 }) {
		auto const result = parse(input);
		ASSERT_FALSE(result.has_value()) << input;
		EXPECT_EQ(result.error(), (error{.code = errc::empty_math, .line = line, .column = column}))
		    << input;
	}
}

TEST(parse, a_row_that_ends_in_a_row_break_is_an_error_at_the_row)
{
	using namespace jimd;

	for (auto const& [input, line, column] : {
			 error_case{.input = "$$$\na \\\\\n$$$\n"sv, .line = 2, .column = 1},
			 error_case{.input = "$$$\na\nb \\\\ \t\n$$$\n"sv, .line = 3, .column = 1},
			 error_case{.input = "$$$\na\r\n  b\\\\\r\n$$$\r\n"sv, .line = 3, .column = 1},
			 error_case{.input = "$$$\na \\\\\\\\\n$$$\n"sv, .line = 2, .column = 1},
			 error_case{.input = "$$$\na \\\\\\\n$$$\n"sv, .line = 2, .column = 1},
			 error_case{.input = "$$$\na \\\\\\\\\\\n$$$\n"sv, .line = 2, .column = 1},
			 error_case{.input = "$$$\na \\\\ % c\n$$$\n"sv, .line = 2, .column = 1},
			 error_case{.input = "$$$\na \\% \\\\\n$$$\n"sv, .line = 2, .column = 1},
			 error_case{.input = "$$$\na \\\\% c\n$$$\n"sv, .line = 2, .column = 1},
			 error_case{.input = ">\n$$$\na\\\\\n$$$\n<\n"sv, .line = 3, .column = 1},
		 }) {
		auto const result = parse(input);
		ASSERT_FALSE(result.has_value()) << input;
		EXPECT_EQ(result.error(),
		          (error{.code = errc::math_row_break, .line = line, .column = column}))
		    << input;
	}
}

TEST(parse, a_row_that_does_not_end_in_a_row_break_is_fine)
{
	using namespace jimd;

	for (auto const input :
	     {"$$$\na % \\\\\n$$$\n"sv, "$$$\na \\\\ b\n$$$\n"sv, "$$$\na \\\n$$$\n"sv,
	      "$$$\n\\\\ a\n$$$\n"sv, "$$$\na \\\\ \\ b\n$$$\n"sv}) {
		auto const result = parse(input);
		EXPECT_TRUE(result.has_value()) << input;
	}
}

TEST(parse, a_row_whose_environment_spans_rows_is_an_error_at_the_row)
{
	using namespace jimd;

	for (auto const& [input, line, column] : {
			 error_case{.input = "$$$\n\\begin{aligned} a\n\\end{aligned}\n$$$\n"sv,
			            .line = 2,
			            .column = 1},
			 error_case{.input = "$$$\na\n  b \\end{aligned}\n$$$\n"sv, .line = 3, .column = 1},
			 error_case{
				 .input = "$$$\n\\begin {x} \\begin{y} \\end{y}\n$$$\n"sv, .line = 2, .column = 1},
			 error_case{
				 .input = "$$$\n\\begin{x} \\end{x} \\end{y}\n$$$\n"sv, .line = 2, .column = 1},
			 error_case{.input = "$$$\r\n\\begin{x}\r\n$$$\r\n"sv, .line = 2, .column = 1},
		 }) {
		auto const result = parse(input);
		ASSERT_FALSE(result.has_value()) << input;
		EXPECT_EQ(
			result.error(),
			(error{.code = errc::math_environment_spans_rows, .line = line, .column = column}))
		    << input;
	}
}

TEST(parse, rows_with_balanced_or_no_environments_are_fine)
{
	using namespace jimd;

	for (auto const input :
	     {"$$$\n\\begin{x} a \\end{x}\n$$$\n"sv, "$$$\na % \\begin{x}\nb % \\end{y}\n$$$\n"sv,
	      "$$$\na \\\\begin{x}\n$$$\n"sv, "$$$\n\\beginning{x} \\endx{y}\n$$$\n"sv,
	      "$$$\n\\begin x \\end\n$$$\n"sv, "$$$\na \\% \\begin{x} \\end{x}\n$$$\n"sv}) {
		auto const result = parse(input);
		EXPECT_TRUE(result.has_value()) << input;
	}
}

TEST(parse, the_row_break_check_comes_before_the_environment_check)
{
	using namespace jimd;

	auto const result = parse("$$$\n\\begin{x} \\\\\n$$$\n"sv);
	ASSERT_FALSE(result.has_value());
	EXPECT_EQ(result.error().code, errc::math_row_break);
}

TEST(parse, an_image_target_may_end_in_a_width)
{
	using namespace jimd;

	static constexpr auto input = "[!a](x.png 75%)"sv;
	auto const result = parse(input);
	ASSERT_TRUE(result.has_value());
	EXPECT_EQ(*result,
	          make_doc(input, {ast::paragraph{.children = {ast::image{.key = "a"sv,
	                                                                  .alt = "a"sv,
	                                                                  .target = "x.png"sv,
	                                                                  .width = "75%"sv}}}}));
	EXPECT_TRUE(result->defs.images.empty());
	auto const& first =
		std::get<ast::image>(std::get<ast::paragraph>(result->blocks.front()).children.front());
	ASSERT_TRUE(first.width.has_value());
	EXPECT_EQ(first.width.value().data(), input.substr(11).data());
}

TEST(parse, a_defined_image_may_end_in_a_width)
{
	using namespace jimd;

	static constexpr auto input = "[!a]\n\n[!a]: my pic.png \t 100%  \n"sv;
	auto const result = parse(input);
	ASSERT_TRUE(result.has_value());
	EXPECT_EQ(*result, make_doc(input,
	                            {ast::paragraph{.children = {ast::image{.key = "a"sv,
	                                                                    .alt = "a"sv,
	                                                                    .target = std::nullopt,
	                                                                    .width = std::nullopt}}}},
	                            make_resources(
									{}, {{"a"sv, {.target = "my pic.png"sv, .width = "100%"sv}}})));
}

TEST(parse, a_width_is_one_to_a_hundred_and_leading_zeros_stay_as_written)
{
	using namespace jimd;

	for (auto const width : {"1%"sv, "50%"sv, "99%"sv, "100%"sv, "075%"sv, "0100%"sv, "001%"sv}) {
		auto const input = std::string{"[!a](x "} + std::string{width} + ")";
		auto const result = parse(input);
		ASSERT_TRUE(result.has_value()) << input;
		EXPECT_EQ(given(*first_image(result->blocks)),
		          (ast::image_def{.target = "x"sv, .width = width}))
		    << input;
	}
}

TEST(parse, a_last_word_that_is_not_a_width_belongs_to_the_target)
{
	using namespace jimd;

	for (auto const target :
	     {"75%"sv, "x 7.5%"sv, "x 75 %"sv, "x +75%"sv, "x -5%"sv, "x 75"sv, "x 75%%"sv, "x 7a%"sv,
	      "x %"sv, "x 1e2%"sv, "75% x"sv, "x% 5"sv, "a%b"sv, "x "sv}) {
		auto const input = std::string{"[!a]("} + std::string{target} + ")";
		auto const result = parse(input);
		ASSERT_TRUE(result.has_value()) << input;
		EXPECT_EQ(given(*first_image(result->blocks)),
		          (ast::image_def{.target = target, .width = std::nullopt}))
		    << input;
	}
	static constexpr auto def = "[!a]: x 7.5%\n"sv;
	auto const result = parse(def);
	ASSERT_TRUE(result.has_value());
	EXPECT_EQ(result->defs.images.at("a"sv).target, "x 7.5%"sv);
}

TEST(parse, a_width_needs_a_target_before_it)
{
	using namespace jimd;

	for (auto const& [text_input, target, width] : {
			 std::tuple{"[!a](75%)"sv, "75%"sv, std::optional<std::string_view>{}},
			 std::tuple{"[!a]( 75%)"sv, " 75%"sv, std::optional<std::string_view>{}},
			 std::tuple{"[!a]( x\t75%)"sv, " x"sv, std::optional{"75%"sv}},
			 std::tuple{"[!a](x \t 5%\t)"sv, "x"sv, std::optional{"5%"sv}},
		 }) {
		auto const result = parse(text_input);
		ASSERT_TRUE(result.has_value()) << text_input;
		EXPECT_EQ(given(*first_image(result->blocks)),
		          (ast::image_def{.target = target, .width = width}))
		    << text_input;
	}
	auto const def = parse("[!a]: 75%\n"sv);
	ASSERT_TRUE(def.has_value());
	EXPECT_EQ(def->defs.images.at("a"sv),
	          (ast::image_def{.target = "75%"sv, .width = std::nullopt}));
}

TEST(parse, a_defined_width_ends_before_a_crlf_line_end)
{
	using namespace jimd;

	auto const result = parse("[!a]\r\n\r\n[!a]: my pic.png 50%\r\n"sv);
	ASSERT_TRUE(result.has_value());
	EXPECT_EQ(result->defs.images.at("a"sv),
	          (ast::image_def{.target = "my pic.png"sv, .width = "50%"sv}));
}

TEST(parse, a_width_in_a_heading_a_quote_or_a_list_item)
{
	using namespace jimd;

	for (auto const input :
	     {"# [!a](x 50%)\n"sv, "text\n>\n[!a](x 50%)\n<\n"sv, "- [!a](x 50%)\n"sv}) {
		auto const result = parse(input);
		ASSERT_TRUE(result.has_value()) << input;
		EXPECT_EQ(given(*first_image(result->blocks)),
		          (ast::image_def{.target = "x"sv, .width = "50%"sv}))
		    << input;
	}
}

TEST(parse, a_width_out_of_range_is_an_error_at_the_width)
{
	using namespace jimd;

	for (auto const& [input, line, column] : {
			 error_case{.input = "[!a](x 0%)"sv, .line = 1, .column = 8},
			 error_case{.input = "[!a](x 000%)"sv, .line = 1, .column = 8},
			 error_case{.input = "[!a](x 101%)"sv, .line = 1, .column = 8},
			 error_case{.input = "[!a](x 0101%)"sv, .line = 1, .column = 8},
			 error_case{.input = "[!a](x 99999999999999999999999%)"sv, .line = 1, .column = 8},
			 error_case{.input = "[!a]: x 0%\n"sv, .line = 1, .column = 9},
			 error_case{.input = "[!a]: my pic.png  200%  \n"sv, .line = 1, .column = 19},
			 error_case{.input = "text\n>\ny [!a](x   0%)\n<\n"sv, .line = 3, .column = 12},
			 error_case{.input = "- [!a](x 0%)\n"sv, .line = 1, .column = 10},
		 }) {
		auto const result = parse(input);
		ASSERT_FALSE(result.has_value()) << input;
		EXPECT_EQ(result.error(),
		          (error{.code = errc::invalid_width, .line = line, .column = column}))
		    << input;
	}
}

TEST(parse, a_duplicate_image_is_reported_at_its_key_before_its_width)
{
	using namespace jimd;

	for (auto const& [input, line, column] : {
			 error_case{.input = "[!a]: x 50%\n[!a]: x 50%\n"sv, .line = 2, .column = 3},
			 error_case{.input = "[!a]: x\n[!a]: y 0%\n"sv, .line = 2, .column = 3},
		 }) {
		auto const result = parse(input);
		ASSERT_FALSE(result.has_value()) << input;
		EXPECT_EQ(result.error(),
		          (error{.code = errc::duplicate_definition, .line = line, .column = column}))
		    << input;
	}
}

TEST(parse, a_width_is_not_split_off_a_link_or_a_footnote)
{
	using namespace jimd;

	static constexpr auto input = "[@l](u 50%) [^n](a 50%)\n\n[@m]: v 50%\n[@m]"sv;
	auto const result = parse(input);
	ASSERT_TRUE(result.has_value());
	auto const& children = std::get<ast::paragraph>(result->blocks.front()).children;
	EXPECT_EQ(std::get<ast::link>(children.front()).target, "u 50%"sv);
	EXPECT_EQ(result->defs.links.at("m"sv), "v 50%"sv);
}

TEST(parse, quote_with_formatting)
{
	using namespace jimd;

	static constexpr auto input = ">\n*bold* and _italic_\n<\n"sv;
	auto const result = parse(input);
	ASSERT_TRUE(result.has_value());

	auto const expected = make_doc(
		input, {ast::quote{.children = {ast::paragraph{
							   .children = {
								   ast::strong{.children = {ast::text{.value = "bold"sv}}},
								   ast::text{.value = " and "sv},
								   ast::emphasis{.children = {ast::text{.value = "italic"sv}}},
							   }}}}});
	EXPECT_EQ(*result, expected);
}

TEST(parse, paragraph_formatting)
{
	using namespace jimd;

	static constexpr auto input = "*bold*, _italic_, and `code`"sv;
	auto const result = parse(input);
	ASSERT_TRUE(result.has_value());

	auto const expected = make_doc(
		input, {ast::paragraph{.children = {
								   ast::strong{.children = {ast::text{.value = "bold"sv}}},
								   ast::text{.value = ", "sv},
								   ast::emphasis{.children = {ast::text{.value = "italic"sv}}},
								   ast::text{.value = ", and "sv},
								   ast::code_span{.value = "code"sv},
							   }}});
	EXPECT_EQ(*result, expected);
}

TEST(parse, code_span_in_paragraph)
{
	using namespace jimd;

	static constexpr auto input = "Here is `some code` inline"sv;
	auto const result = parse(input);
	ASSERT_TRUE(result.has_value());

	auto const expected =
		make_doc(input, {ast::paragraph{.children = {
											ast::text{.value = "Here is "sv},
											ast::code_span{.value = "some code"sv},
											ast::text{.value = " inline"sv},
										}}});
	EXPECT_EQ(*result, expected);
}

TEST(parse, multiline_quote_with_formatting)
{
	using namespace jimd;

	static constexpr auto input = ">\n`code line 1`\n*bold line 2*\n<\n"sv;
	auto const result = parse(input);
	ASSERT_TRUE(result.has_value());

	auto const expected = make_doc(
		input, {ast::quote{.children = {ast::paragraph{
							   .children = {
								   ast::code_span{.value = "code line 1"sv},
								   ast::soft_break{},
								   ast::strong{.children = {ast::text{.value = "bold line 2"sv}}},
							   }}}}});
	EXPECT_EQ(*result, expected);
}

TEST(parse, disconnected_link_def)
{
	using namespace jimd;

	static constexpr auto input = "[@site]\n\n[@site]: https://example.com\n"sv;
	auto const result = parse(input);
	ASSERT_TRUE(result.has_value());

	auto const expected = make_doc(
		input,
		{ast::paragraph{
			.children = {ast::link{.key = "site"sv, .text = "site"sv, .target = std::nullopt}}}},
		make_resources({{"site"sv, "https://example.com"sv}}));
	EXPECT_EQ(*result, expected);
}

TEST(parse, multi_reference_single_def)
{
	using namespace jimd;

	static constexpr auto input = "[@site] and [@site]\n\n[@site]: https://example.com\n"sv;
	auto const result = parse(input);
	ASSERT_TRUE(result.has_value());

	auto const expected =
		make_doc(input,
		         {ast::paragraph{
					 .children =
						 {
							 ast::link{.key = "site"sv, .text = "site"sv, .target = std::nullopt},
							 ast::text{.value = " and "sv},
							 ast::link{.key = "site"sv, .text = "site"sv, .target = std::nullopt},
						 }}},
		         make_resources({{"site"sv, "https://example.com"sv}}));
	EXPECT_EQ(*result, expected);
}

TEST(parse, disconnected_image_def)
{
	using namespace jimd;

	static constexpr auto input = "[!logo]\n\n[!logo]: /assets/logo.png\n"sv;
	auto const result = parse(input);
	ASSERT_TRUE(result.has_value());

	auto const expected = make_doc(
		input,
		{ast::paragraph{
			.children = {ast::image{
				.key = "logo"sv, .alt = "logo"sv, .target = std::nullopt, .width = std::nullopt}}}},
		make_resources({}, {{"logo"sv, {.target = "/assets/logo.png"sv, .width = std::nullopt}}}));
	EXPECT_EQ(*result, expected);
}

TEST(parse, disconnected_ref_def)
{
	using namespace jimd;

	static constexpr auto input = "[^1]\n\n[^1]: Footnote 1\n"sv;
	auto const result = parse(input);
	ASSERT_TRUE(result.has_value());

	auto const expected = make_doc(
		input,
		{ast::paragraph{.children = {ast::reference{.key = "1"sv, .content = std::nullopt}}}},
		make_resources({}, {}, {{"1"sv, ast::inlines{ast::text{.value = "Footnote 1"sv}}}}));
	EXPECT_EQ(*result, expected);
}

TEST(parse, inline_def_structure)
{
	using namespace jimd;

	static constexpr auto input = "[@site](https://example.com)\n"sv;
	auto const result = parse(input);
	ASSERT_TRUE(result.has_value());

	auto const expected = make_doc(
		input, {ast::paragraph{.children = {ast::link{.key = "site"sv,
		                                              .text = "site"sv,
		                                              .target = "https://example.com"sv}}}});
	EXPECT_EQ(*result, expected);
	EXPECT_TRUE(result->defs.links.empty());
}

TEST(parse, inline_image_and_reference_structure)
{
	using namespace jimd;

	static constexpr auto input = "[!logo](/logo.png) and [^1](Footnote note)\n"sv;
	auto const result = parse(input);
	ASSERT_TRUE(result.has_value());

	auto const expected =
		make_doc(input, {ast::paragraph{.children = {
											ast::image{.key = "logo"sv,
											           .alt = "logo"sv,
											           .target = "/logo.png"sv,
											           .width = std::nullopt},
											ast::text{.value = " and "sv},
											ast::reference{.key = "1"sv,
											               .content = ast::inlines{ast::text{
															   .value = "Footnote note"sv}}},
										}}});
	EXPECT_EQ(*result, expected);
	EXPECT_TRUE(result->defs.images.empty());
	EXPECT_TRUE(result->defs.footnotes.empty());
}

TEST(parse, an_inline_link_defines_nothing_to_refer_to)
{
	using namespace jimd;

	for (auto const& [input, line, column] : {
			 error_case{
				 .input = "[@site](https://example.com) and [@site]\n"sv, .line = 1, .column = 36},
			 error_case{
				 .input = "[@site] and [@site](https://example.com)\n"sv, .line = 1, .column = 3},
		 }) {
		auto const result = parse(input);
		ASSERT_FALSE(result.has_value()) << input;
		EXPECT_EQ(result.error(),
		          (error{.code = errc::undefined_reference, .line = line, .column = column}))
		    << input;
	}
}

TEST(parse, unreferenced_def_allowed)
{
	using namespace jimd;

	static constexpr auto input = "[@unused]: https://example.com\n"sv;
	auto const result = parse(input);
	ASSERT_TRUE(result.has_value());

	auto const expected =
		make_doc(input, {}, make_resources({{"unused"sv, "https://example.com"sv}}));
	EXPECT_EQ(*result, expected);
}

TEST(parse, duplicate_disconnected_def_error)
{
	using namespace jimd;

	static constexpr auto input = "[@site]: https://one.com\n[@site]: https://two.com\n"sv;
	auto const result = parse(input);
	ASSERT_FALSE(result.has_value());
	EXPECT_EQ(result.error(), (error{.code = errc::duplicate_definition, .line = 2, .column = 3}));
}

TEST(parse, equal_inline_links_may_have_different_targets)
{
	using namespace jimd;

	static constexpr auto input = "[@site](https://one.com) and [@site](https://two.com)\n"sv;
	auto const result = parse(input);
	ASSERT_TRUE(result.has_value());
	EXPECT_EQ(
		*result,
		make_doc(
			input,
			{ast::paragraph{
				.children = {
					ast::link{.key = "site"sv, .text = "site"sv, .target = "https://one.com"sv},
					text(" and "),
					ast::link{.key = "site"sv, .text = "site"sv, .target = "https://two.com"sv}}}}));
	EXPECT_TRUE(result->defs.links.empty());
}

TEST(parse, equal_inline_images_may_have_different_targets)
{
	using namespace jimd;

	static constexpr auto input = "[!logo](/one.png) [!logo](/two.png 50%)\n"sv;
	auto const result = parse(input);
	ASSERT_TRUE(result.has_value());
	EXPECT_EQ(*result,
	          make_doc(input, {ast::paragraph{.children = {ast::image{.key = "logo"sv,
	                                                                  .alt = "logo"sv,
	                                                                  .target = "/one.png"sv,
	                                                                  .width = std::nullopt},
	                                                       text(" "),
	                                                       ast::image{.key = "logo"sv,
	                                                                  .alt = "logo"sv,
	                                                                  .target = "/two.png"sv,
	                                                                  .width = "50%"sv}}}}));
	EXPECT_TRUE(result->defs.images.empty());
}

TEST(parse, an_inline_link_and_a_definition_of_one_key_are_separate)
{
	using namespace jimd;

	static constexpr auto input =
		"[@site](https://one.com) and [@site]\n\n[@site]: https://two.com\n"sv;
	auto const result = parse(input);
	ASSERT_TRUE(result.has_value());
	EXPECT_EQ(*result,
	          make_doc(input,
	                   {ast::paragraph{.children = {ast::link{.key = "site"sv,
	                                                          .text = "site"sv,
	                                                          .target = "https://one.com"sv},
	                                                text(" and "),
	                                                ast::link{.key = "site"sv,
	                                                          .text = "site"sv,
	                                                          .target = std::nullopt}}}},
	                   make_resources({{"site"sv, "https://two.com"sv}})));
}

TEST(parse, an_inline_image_and_a_definition_of_one_key_are_separate)
{
	using namespace jimd;

	static constexpr auto input = "[!a](/one.png) [!a]\n\n[!a]: /two.png 50%\n"sv;
	auto const result = parse(input);
	ASSERT_TRUE(result.has_value());
	EXPECT_EQ(*result,
	          make_doc(input,
	                   {ast::paragraph{.children = {ast::image{.key = "a"sv,
	                                                           .alt = "a"sv,
	                                                           .target = "/one.png"sv,
	                                                           .width = std::nullopt},
	                                                text(" "),
	                                                ast::image{.key = "a"sv,
	                                                           .alt = "a"sv,
	                                                           .target = std::nullopt,
	                                                           .width = std::nullopt}}}},
	                   make_resources({}, {{"a"sv, {.target = "/two.png"sv, .width = "50%"sv}}})));
}

TEST(parse, an_inline_image_defines_nothing_to_refer_to)
{
	using namespace jimd;

	for (auto const& [input, line, column] : {
			 error_case{.input = "[!a](x.png) [!a]\n"sv, .line = 1, .column = 15},
			 error_case{.input = "[!a] [!a](x.png)\n"sv, .line = 1, .column = 3},
		 }) {
		auto const result = parse(input);
		ASSERT_FALSE(result.has_value()) << input;
		EXPECT_EQ(result.error(),
		          (error{.code = errc::undefined_reference, .line = line, .column = column}))
		    << input;
	}
}

TEST(parse, an_inline_note_defines_nothing_to_refer_to)
{
	using namespace jimd;

	for (auto const& [input, line, column] : {
			 error_case{.input = "a[^n](text) b[^n]\n"sv, .line = 1, .column = 16},
			 error_case{.input = "a[^n] b[^n](text)\n"sv, .line = 1, .column = 4},
		 }) {
		auto const result = parse(input);
		ASSERT_FALSE(result.has_value()) << input;
		EXPECT_EQ(result.error(),
		          (error{.code = errc::undefined_reference, .line = line, .column = column}))
		    << input;
	}
}

TEST(parse, two_inline_notes_may_share_a_key)
{
	using namespace jimd;

	static constexpr auto input = "a[^n](one) b[^n](two)\n"sv;
	auto const result = parse(input);
	ASSERT_TRUE(result.has_value());
	EXPECT_EQ(
		*result,
		make_doc(
			input,
			{ast::paragraph{
				.children = {
					text("a"), ast::reference{.key = "n"sv, .content = ast::inlines{text("one")}},
					text(" b"),
					ast::reference{.key = "n"sv, .content = ast::inlines{text("two")}}}}}));
	EXPECT_TRUE(result->defs.footnotes.empty());
}

TEST(parse, an_inline_note_and_a_definition_of_one_key_are_separate)
{
	using namespace jimd;

	static constexpr auto input = "a[^n](inline) b[^n]\n\n[^n]: defined\n"sv;
	auto const result = parse(input);
	ASSERT_TRUE(result.has_value());
	EXPECT_EQ(*result,
	          make_doc(input,
	                   {ast::paragraph{
						   .children = {text("a"),
						                ast::reference{.key = "n"sv,
						                               .content = ast::inlines{text("inline")}},
						                text(" b"), ast::reference{.key = "n"sv, .content = {}}}}},
	                   make_resources({}, {}, {{"n"sv, ast::inlines{text("defined")}}})));
}

TEST(parse, duplicate_image_def_error)
{
	using namespace jimd;

	static constexpr auto input = "[!logo]: /one.png\n[!logo]: /two.png\n"sv;
	auto const result = parse(input);
	ASSERT_FALSE(result.has_value());
	EXPECT_EQ(result.error(), (error{.code = errc::duplicate_definition, .line = 2, .column = 3}));
}

TEST(parse, duplicate_footnote_def_error)
{
	using namespace jimd;

	static constexpr auto input = "[^note]: Note 1\n[^note]: Note 2\n"sv;
	auto const result = parse(input);
	ASSERT_FALSE(result.has_value());
	EXPECT_EQ(result.error(), (error{.code = errc::duplicate_definition, .line = 2, .column = 3}));
}

TEST(parse, an_empty_key_is_an_error_at_the_closing_bracket)
{
	using namespace jimd;

	for (auto const& [input, line, column] : {
			 error_case{.input = "[@]: u\n"sv, .line = 1, .column = 3},
			 error_case{.input = "[@]\n"sv, .line = 1, .column = 3},
			 error_case{.input = "[!]: x.png\n"sv, .line = 1, .column = 3},
			 error_case{.input = "[!]\n"sv, .line = 1, .column = 3},
			 error_case{.input = "[^]: x\n"sv, .line = 1, .column = 3},
			 error_case{.input = "[^]\n"sv, .line = 1, .column = 3},
			 error_case{.input = "[@ ]: u\n"sv, .line = 1, .column = 4},
			 error_case{.input = "a [@  ] b\n"sv, .line = 1, .column = 7},
			 error_case{.input = "a\n\n[^\t]: x\n"sv, .line = 3, .column = 4},
			 error_case{.input = ">\n[!]\n<\n"sv, .line = 2, .column = 3},
			 error_case{.input = "# a [@]\n"sv, .line = 1, .column = 7},
			 error_case{.input = "[^n]: a [@]\n"sv, .line = 1, .column = 11},
		 }) {
		auto const result = parse(input);
		ASSERT_FALSE(result.has_value()) << input;
		EXPECT_EQ(result.error(), (error{.code = errc::empty_key, .line = line, .column = column}))
		    << input;
	}
}

TEST(parse, an_inline_link_with_no_text_is_an_error_at_the_closing_bracket)
{
	using namespace jimd;

	for (auto const& [input, line, column] : {
			 error_case{.input = "[@](u)"sv, .line = 1, .column = 3},
			 error_case{.input = "[@ ](u)"sv, .line = 1, .column = 4},
			 error_case{.input = "x [@ \t](u) y"sv, .line = 1, .column = 7},
			 error_case{.input = "- a\n- [@](u)\n"sv, .line = 2, .column = 5},
			 error_case{.input = "[^n](a [@](u))"sv, .line = 1, .column = 10},
		 }) {
		auto const result = parse(input);
		ASSERT_FALSE(result.has_value()) << input;
		EXPECT_EQ(result.error(), (error{.code = errc::empty_link, .line = line, .column = column}))
		    << input;
	}
}

TEST(parse, an_inline_image_may_have_no_alt_text)
{
	using namespace jimd;

	static constexpr auto input = "[!](u.png 50%)"sv;
	auto const result = parse(input);
	ASSERT_TRUE(result.has_value());
	EXPECT_EQ(*result,
	          make_doc(input, {ast::paragraph{.children = {ast::image{.key = ""sv,
	                                                                  .alt = ""sv,
	                                                                  .target = "u.png"sv,
	                                                                  .width = "50%"sv}}}}));
}

TEST(parse, inline_notes_may_have_empty_keys)
{
	using namespace jimd;

	static constexpr auto input = "[^](a) [^ ](b) [^](c)"sv;
	auto const result = parse(input);
	ASSERT_TRUE(result.has_value());
	auto const& children = std::get<ast::paragraph>(result->blocks.front()).children;
	ASSERT_EQ(children.size(), 5U);
	EXPECT_EQ(std::get<ast::reference>(children.at(0)).content, (ast::inlines{text("a")}));
	EXPECT_EQ(std::get<ast::reference>(children.at(2)).content, (ast::inlines{text("b")}));
	EXPECT_EQ(std::get<ast::reference>(children.at(4)).content, (ast::inlines{text("c")}));
	EXPECT_TRUE(result->defs.footnotes.empty());
}

TEST(parse, undefined_resource_error)
{
	using namespace jimd;

	static constexpr auto input = "[@undefined_link]\n"sv;
	auto const result = parse(input);
	ASSERT_FALSE(result.has_value());
	EXPECT_EQ(result.error(), (error{.code = errc::undefined_reference, .line = 1, .column = 3}));
}

TEST(parse, undefined_image_error)
{
	using namespace jimd;

	static constexpr auto input = "[!undefined_img]\n"sv;
	auto const result = parse(input);
	ASSERT_FALSE(result.has_value());
	EXPECT_EQ(result.error(), (error{.code = errc::undefined_reference, .line = 1, .column = 3}));
}

TEST(parse, undefined_footnote_error)
{
	using namespace jimd;

	static constexpr auto input = "[^undefined_fn]\n"sv;
	auto const result = parse(input);
	ASSERT_FALSE(result.has_value());
	EXPECT_EQ(result.error(), (error{.code = errc::undefined_reference, .line = 1, .column = 3}));
}

TEST(parse, tag_namespacing)
{
	using namespace jimd;

	static constexpr auto input = "[@foo]\n[!foo]\n\n[@foo]: url\n[!foo]: img\n"sv;
	auto const result = parse(input);
	ASSERT_TRUE(result.has_value());

	auto const expected = make_doc(
		input,
		{ast::paragraph{.children =
		                    {
								ast::link{.key = "foo"sv, .text = "foo"sv, .target = std::nullopt},
								ast::soft_break{},
								ast::image{.key = "foo"sv,
								           .alt = "foo"sv,
								           .target = std::nullopt,
								           .width = std::nullopt},
							}}},
		make_resources({{"foo"sv, "url"sv}},
		               {{"foo"sv, {.target = "img"sv, .width = std::nullopt}}}));
	EXPECT_EQ(*result, expected);
}

TEST(parse, reference_inside_quote)
{
	using namespace jimd;

	static constexpr auto input = ">\n[@site]\n<\n\n[@site]: https://example.com\n"sv;
	auto const result = parse(input);
	ASSERT_TRUE(result.has_value());

	auto const expected =
		make_doc(input,
		         {ast::quote{.children = {ast::paragraph{.children = {ast::link{
															 .key = "site"sv,
															 .text = "site"sv,
															 .target = std::nullopt,
														 }}}}}},
		         make_resources({{"site"sv, "https://example.com"sv}}));
	EXPECT_EQ(*result, expected);
}

TEST(parse, undefined_reference_inside_quote)
{
	using namespace jimd;

	static constexpr auto input = ">\n[@missing]\n<\n"sv;
	auto const result = parse(input);
	ASSERT_FALSE(result.has_value());
	EXPECT_EQ(result.error(), (error{.code = errc::undefined_reference, .line = 2, .column = 3}));
}

TEST(parse, duplicate_precedence_over_undefined)
{
	using namespace jimd;

	static constexpr auto input = "[@missing]\n\n[@site]: url1\n[@site]: url2\n"sv;
	auto const result = parse(input);
	ASSERT_FALSE(result.has_value());
	EXPECT_EQ(result.error(), (error{.code = errc::duplicate_definition, .line = 4, .column = 3}));
}

TEST(parse, error_positions_agree_on_lone_cr)
{
	using namespace jimd;

	auto const syntax_result = parse("x\ry\n>\nhi\n"sv);
	ASSERT_FALSE(syntax_result.has_value());
	EXPECT_EQ(syntax_result.error(), (error{.code = errc::unclosed_quote, .line = 4, .column = 1}));

	auto const link_result = parse("x\ry\n[@a]: u\n[@a]: v\n"sv);
	ASSERT_FALSE(link_result.has_value());
	EXPECT_EQ(link_result.error(),
	          (error{.code = errc::duplicate_definition, .line = 3, .column = 3}));
}

TEST(parse, unclosed_resource_error)
{
	using namespace jimd;

	for (auto const& [input, line, column] : {
			 error_case{.input = "[@name"sv, .line = 1, .column = 7},
			 error_case{.input = "[!pic"sv, .line = 1, .column = 6},
			 error_case{.input = "[^note"sv, .line = 1, .column = 7},
			 error_case{.input = "text [@name and more\n"sv, .line = 1, .column = 21},
			 error_case{.input = "[@a](url"sv, .line = 1, .column = 9},
			 error_case{.input = "[!a](url\nmore)"sv, .line = 1, .column = 9},
			 error_case{.input = "[^a](note"sv, .line = 1, .column = 10},
			 error_case{.input = "para\n\n[@a\n"sv, .line = 3, .column = 4},
			 error_case{.input = ">\n[@a\n<\n"sv, .line = 2, .column = 4},
			 error_case{.input = "text\n[@a\n"sv, .line = 2, .column = 4},
		 }) {
		auto const result = parse(input);
		ASSERT_FALSE(result.has_value()) << input;
		EXPECT_EQ(result.error(),
		          (error{.code = errc::unclosed_resource, .line = line, .column = column}))
		    << input;
	}
}

TEST(parse, resource_id_does_not_span_lines)
{
	using namespace jimd;

	auto const result = parse("[@multi\nline](x)\n"sv);
	ASSERT_FALSE(result.has_value());
	EXPECT_EQ(result.error(), (error{.code = errc::unclosed_resource, .line = 1, .column = 8}));
}

TEST(parse, resource_target_does_not_span_lines)
{
	using namespace jimd;

	auto const result = parse("[@a](multi\nline)\n"sv);
	ASSERT_FALSE(result.has_value());
	EXPECT_EQ(result.error(), (error{.code = errc::unclosed_resource, .line = 1, .column = 11}));
}

TEST(parse, resource_definition_does_not_span_lines)
{
	using namespace jimd;

	auto const result = parse("[@multi\nline]: x\n"sv);
	ASSERT_FALSE(result.has_value());
	EXPECT_EQ(result.error().code, errc::unclosed_resource);
}

TEST(parse, closed_resources_on_one_line_still_parse)
{
	using namespace jimd;

	static constexpr auto input = "[@a](u) [!b](v) [^c](w) [@a]() [!](x) [^](y) x\n"sv;
	auto const result = parse(input);
	ASSERT_TRUE(result.has_value());
	EXPECT_EQ(std::get<ast::paragraph>(result->blocks.front()).children.size(), 12U);
	EXPECT_TRUE(result->defs.links.empty());
	EXPECT_TRUE(result->defs.images.empty());
	EXPECT_TRUE(result->defs.footnotes.empty());
}

TEST(parse, a_bracket_that_does_not_start_a_resource_is_plain_text)
{
	using namespace jimd;

	for (auto const input : {"see [this] here"sv, "a [ b"sv, "a ] b [ c"sv, "[]"sv, "[x"sv,
	                         "[ @a]"sv, "arr[0] = [1, 2]"sv}) {
		auto const result = parse(input);
		ASSERT_TRUE(result.has_value()) << input;
		EXPECT_EQ(*result,
		          make_doc(input, {ast::paragraph{.children = {ast::text{.value = input}}}}))
		    << input;
	}
}

TEST(parse, a_plain_bracket_sits_beside_a_resource)
{
	using namespace jimd;

	static constexpr auto input = "[a] [@k](u) [b]"sv;
	auto const result = parse(input);
	ASSERT_TRUE(result.has_value());
	EXPECT_EQ(*result,
	          make_doc(input,
	                   {ast::paragraph{
						   .children = {ast::text{.value = "[a] "sv},
						                ast::link{.key = "k"sv, .text = "k"sv, .target = "u"sv},
						                ast::text{.value = " [b]"sv}}}}));
}

TEST(parse, a_bracket_at_the_start_of_a_line_is_plain_text)
{
	using namespace jimd;

	static constexpr auto input = "[note] first\n[more]\n"sv;
	auto const result = parse(input);
	ASSERT_TRUE(result.has_value());
	EXPECT_EQ(*result,
	          make_doc(input, {ast::paragraph{.children = {ast::text{.value = "[note] first"sv},
	                                                       ast::soft_break{},
	                                                       ast::text{.value = "[more]"sv}}}}));
}

TEST(parse, a_tagged_bracket_is_still_a_resource_or_an_error)
{
	using namespace jimd;

	for (auto const input : {"[@a"sv, "[!a"sv, "[^a"sv, "x [@a"sv, "x [!a"sv, "x [^a"sv}) {
		auto const result = parse(input);
		ASSERT_FALSE(result.has_value()) << input;
		EXPECT_EQ(result.error().code, errc::unclosed_resource) << input;
	}
	auto const undefined = parse("[@a]"sv);
	ASSERT_FALSE(undefined.has_value());
	EXPECT_EQ(undefined.error().code, errc::undefined_reference);
}

TEST(parse, a_hash_that_does_not_start_a_heading_is_plain_text)
{
	using namespace jimd;

	for (auto const input :
	     {"I like C# a lot"sv, "#tag"sv, "#"sv, "##"sv, "a # b"sv, "issue #12, #13"sv, "#\n"sv,
	      "end #"sv, "a ## b"sv, "a #> b"sv, "#<"sv}) {
		auto const result = parse(input);
		ASSERT_TRUE(result.has_value()) << input;
		auto const text = input.ends_with('\n') ? input.substr(0, input.size() - 1) : input;
		EXPECT_EQ(*result,
		          make_doc(input, {ast::paragraph{.children = {ast::text{.value = text}}}}))
		    << input;
	}
}

TEST(parse, a_hash_line_after_a_paragraph_line_still_starts_a_heading)
{
	using namespace jimd;

	static constexpr auto input = "C# is fine\n# Title\n#tag\n"sv;
	auto const result = parse(input);
	ASSERT_TRUE(result.has_value());
	EXPECT_EQ(*result,
	          make_doc(input,
	                   {ast::paragraph{.children = {ast::text{.value = "C# is fine"sv}}},
	                    ast::heading{.level = 1, .children = {ast::text{.value = "Title"sv}}},
	                    ast::paragraph{.children = {ast::text{.value = "#tag"sv}}}}));
}

TEST(parse, a_hash_next_to_formatting_and_in_a_quote)
{
	using namespace jimd;

	static constexpr auto input = ">\n_a_ #b *c*# d\n<\n"sv;
	auto const result = parse(input);
	ASSERT_TRUE(result.has_value());
	EXPECT_EQ(*result,
	          make_doc(input,
	                   {ast::quote{
						   .children = {ast::paragraph{
							   .children = {ast::emphasis{.children = {ast::text{.value = "a"sv}}},
							                ast::text{.value = " #b "sv},
							                ast::strong{.children = {ast::text{.value = "c"sv}}},
							                ast::text{.value = "# d"sv}}}}}}));
}

TEST(parse, a_hash_directly_before_formatting_is_text)
{
	using namespace jimd;

	static constexpr auto input = "a #_b_ #*c*"sv;
	auto const result = parse(input);
	ASSERT_TRUE(result.has_value());
	EXPECT_EQ(*result,
	          make_doc(input,
	                   {ast::paragraph{
						   .children = {ast::text{.value = "a #"sv},
						                ast::emphasis{.children = {ast::text{.value = "b"sv}}},
						                ast::text{.value = " #"sv},
						                ast::strong{.children = {ast::text{.value = "c"sv}}}}}}));
}

TEST(parse, escapes_are_one_character_text_nodes)
{
	using namespace jimd;

	static constexpr auto input = R"(a \* b \_ \` \[ \# \) \\ z)"sv;
	auto const result = parse(input);
	ASSERT_TRUE(result.has_value());

	EXPECT_EQ(*result,
	          make_doc(input,
	                   {ast::paragraph{.children = {text("a "), text("*"), text(" b "), text("_"),
	                                                text(" "), text("`"), text(" "), text("["),
	                                                text(" "), text("#"), text(" "), text(")"),
	                                                text(" "), text("\\"), text(" z")}}}));
}

TEST(parse, an_escape_points_into_the_source)
{
	using namespace jimd;

	static constexpr auto input = "x\\*y"sv;
	auto const result = parse(input);
	ASSERT_TRUE(result.has_value());
	auto const& children = std::get<ast::paragraph>(result->blocks.front()).children;
	ASSERT_EQ(children.size(), 3U);
	EXPECT_EQ(std::get<ast::text>(children.at(1)).value.data(), input.substr(2, 1).data());
}

TEST(parse, an_escaped_marker_does_not_start_markup)
{
	using namespace jimd;

	static constexpr auto input = "\\# no heading\n\\[@a] \\_b\\_"sv;
	auto const result = parse(input);
	ASSERT_TRUE(result.has_value());

	EXPECT_EQ(*result, make_doc(input, {ast::paragraph{.children = {text("#"), text(" no heading"),
	                                                                ast::soft_break{}, text("["),
	                                                                text("@a] "), text("_"),
	                                                                text("b"), text("_")}}}));
}

TEST(parse, escapes_inside_emphasis)
{
	using namespace jimd;

	static constexpr auto input = R"(_a\_b_ *c\*d* _e\*f_ *g\_h*)"sv;
	auto const result = parse(input);
	ASSERT_TRUE(result.has_value());

	EXPECT_EQ(
		*result,
		make_doc(input,
		         {ast::paragraph{
					 .children = {
						 ast::emphasis{.children = {text("a"), text("_"), text("b")}}, text(" "),
						 ast::strong{.children = {text("c"), text("*"), text("d")}}, text(" "),
						 ast::emphasis{.children = {text("e"), text("*"), text("f")}}, text(" "),
						 ast::strong{.children = {text("g"), text("_"), text("h")}}}}}));
}

TEST(parse, a_backslash_is_literal_in_code)
{
	using namespace jimd;

	static constexpr auto input = "`a\\b` and `\\`\n\n```\n\\q\\\n```\n"sv;
	auto const result = parse(input);
	ASSERT_TRUE(result.has_value());

	EXPECT_EQ(*result,
	          make_doc(input, {ast::paragraph{.children = {ast::code_span{.value = "a\\b"sv},
	                                                       ast::text{.value = " and "sv},
	                                                       ast::code_span{.value = "\\"sv}}},
	                           ast::code_block{.info = ""sv, .body = "\\q\\\n"sv}}));
}

TEST(parse, invalid_escape_error)
{
	using namespace jimd;

	for (auto const& [input, line, column] : {
			 error_case{.input = "\\q"sv, .line = 1, .column = 1},
			 error_case{.input = "ab \\ c"sv, .line = 1, .column = 4},
			 error_case{.input = "end\\"sv, .line = 1, .column = 4},
			 error_case{.input = "end\\\nnext"sv, .line = 1, .column = 4},
			 error_case{.input = "end\\\r\n"sv, .line = 1, .column = 4},
			 error_case{.input = "x\ny \\n"sv, .line = 2, .column = 3},
			 error_case{.input = "_a\\qb_"sv, .line = 1, .column = 3},
			 error_case{.input = "*a\\"sv, .line = 1, .column = 3},
			 error_case{.input = ">\nx \\y\n<\n"sv, .line = 2, .column = 3},
			 error_case{.input = "\\é"sv, .line = 1, .column = 1},
			 error_case{.input = "\\]"sv, .line = 1, .column = 1},
			 error_case{.input = "\\("sv, .line = 1, .column = 1},
			 error_case{.input = "\\<"sv, .line = 1, .column = 1},
			 error_case{.input = "\\>"sv, .line = 1, .column = 1},
			 error_case{.input = "\\@"sv, .line = 1, .column = 1},
			 error_case{.input = "\\!"sv, .line = 1, .column = 1},
			 error_case{.input = "\\^"sv, .line = 1, .column = 1},
		 }) {
		auto const result = parse(input);
		ASSERT_FALSE(result.has_value()) << input;
		EXPECT_EQ(result.error(),
		          (error{.code = errc::invalid_escape, .line = line, .column = column}))
		    << input;
	}
}

TEST(parse, emphasis_closes_on_its_own_line)
{
	using namespace jimd;

	static constexpr auto input = "_a_\n*b*\n_c d_ *e*"sv;
	auto const result = parse(input);
	ASSERT_TRUE(result.has_value());
	EXPECT_EQ(*result,
	          make_doc(input,
	                   {ast::paragraph{
						   .children = {ast::emphasis{.children = {text("a")}}, ast::soft_break{},
						                ast::strong{.children = {text("b")}}, ast::soft_break{},
						                ast::emphasis{.children = {text("c d")}}, text(" "),
						                ast::strong{.children = {text("e")}}}}}));
}

TEST(parse, emphasis_may_hold_blanks)
{
	using namespace jimd;

	static constexpr auto input = "_ _ and * *"sv;
	auto const result = parse(input);
	ASSERT_TRUE(result.has_value());
	EXPECT_EQ(
		*result,
		make_doc(input,
		         {ast::paragraph{.children = {ast::emphasis{.children = {text(" ")}}, text(" and "),
		                                      ast::strong{.children = {text(" ")}}}}}));
}

TEST(parse, emphasis_and_strong_nest)
{
	using namespace jimd;

	static constexpr auto input = "*_both_* _*both*_ _a *b* c_ *a _b_ c*"sv;
	auto const result = parse(input);
	ASSERT_TRUE(result.has_value());

	auto const italic = [](ast::inlines children) -> ast::emphasis {
		return ast::emphasis{.children = std::move(children)};
	};
	auto const bold = [](ast::inlines children) -> ast::strong {
		return ast::strong{.children = std::move(children)};
	};
	EXPECT_EQ(
		*result,
		make_doc(input,
		         {ast::paragraph{
					 .children = {bold({italic({text("both")})}), text(" "),
					              italic({bold({text("both")})}), text(" "),
					              italic({text("a "), bold({text("b")}), text(" c")}), text(" "),
					              bold({text("a "), italic({text("b")}), text(" c")})}}}));
}

TEST(parse, emphasis_holds_code_resources_and_escapes)
{
	using namespace jimd;

	static constexpr auto input = "_a `b_c` [@k](u) [!i](p) \\_ z_"sv;
	auto const result = parse(input);
	ASSERT_TRUE(result.has_value());
	EXPECT_EQ(
		*result,
		make_doc(input,
		         {ast::paragraph{
					 .children = {ast::emphasis{
						 .children = {
							 text("a "), ast::code_span{.value = "b_c"sv}, text(" "),
							 ast::link{.key = "k"sv, .text = "k"sv, .target = "u"sv}, text(" "),
							 ast::image{
								 .key = "i"sv, .alt = "i"sv, .target = "p"sv, .width = std::nullopt},
							 text(" "), text("_"), text(" z")}}}}}));
}

TEST(parse, a_reference_inside_emphasis_is_checked)
{
	using namespace jimd;

	auto const result = parse("_see [@nowhere] ok_"sv);
	ASSERT_FALSE(result.has_value());
	EXPECT_EQ(result.error(), (error{.code = errc::undefined_reference, .line = 1, .column = 8}));
}

TEST(parse, nested_emphasis_must_close_in_order)
{
	using namespace jimd;

	for (auto const& [input, line, column] : {
			 error_case{.input = "_a *b_ c*"sv, .line = 1, .column = 10},
			 error_case{.input = "*a _b* c_"sv, .line = 1, .column = 10},
			 error_case{.input = "_a*b_"sv, .line = 1, .column = 6},
			 error_case{.input = "*a _b*"sv, .line = 1, .column = 7},
			 error_case{.input = "_*a_"sv, .line = 1, .column = 5},
		 }) {
		auto const result = parse(input);
		ASSERT_FALSE(result.has_value()) << input;
		EXPECT_EQ(result.error(),
		          (error{.code = errc::unclosed_emphasis, .line = line, .column = column}))
		    << input;
	}
	auto const result = parse("_a **_"sv);
	ASSERT_FALSE(result.has_value());
	EXPECT_EQ(result.error(), (error{.code = errc::empty_emphasis, .line = 1, .column = 4}));
}

TEST(parse, adjacent_emphasis_and_a_lone_cr)
{
	using namespace jimd;

	static constexpr auto input = "_a__b_ *c**d* _e\rf_"sv;
	auto const result = parse(input);
	ASSERT_TRUE(result.has_value());
	EXPECT_EQ(*result,
	          make_doc(input,
	                   {ast::paragraph{.children = {ast::emphasis{.children = {text("a")}},
	                                                ast::emphasis{.children = {text("b")}},
	                                                text(" "), ast::strong{.children = {text("c")}},
	                                                ast::strong{.children = {text("d")}}, text(" "),
	                                                ast::emphasis{.children = {text("e\rf")}}}}}));
}

TEST(parse, unclosed_emphasis_error)
{
	using namespace jimd;

	for (auto const& [input, line, column] : {
			 error_case{.input = "snake _case word"sv, .line = 1, .column = 17},
			 error_case{.input = "_"sv, .line = 1, .column = 2},
			 error_case{.input = "*bold"sv, .line = 1, .column = 6},
			 error_case{.input = "_a\nb_"sv, .line = 1, .column = 3},
			 error_case{.input = "a _b\n\n# head_er"sv, .line = 1, .column = 5},
			 error_case{.input = "ok\nx *y\nz*"sv, .line = 2, .column = 5},
			 error_case{.input = "_a\n"sv, .line = 1, .column = 3},
			 error_case{.input = "_a*b\n"sv, .line = 1, .column = 5},
			 error_case{.input = ">\nsome _text\n<\n"sv, .line = 2, .column = 11},
			 error_case{.input = "_a\\_"sv, .line = 1, .column = 5},
			 error_case{.input = "_a\r\nb_"sv, .line = 1, .column = 3},
		 }) {
		auto const result = parse(input);
		ASSERT_FALSE(result.has_value()) << input;
		EXPECT_EQ(result.error(),
		          (error{.code = errc::unclosed_emphasis, .line = line, .column = column}))
		    << input;
	}
}

TEST(parse, empty_emphasis_error)
{
	using namespace jimd;

	for (auto const& [input, line, column] : {
			 error_case{.input = "__"sv, .line = 1, .column = 1},
			 error_case{.input = "**"sv, .line = 1, .column = 1},
			 error_case{.input = "a ** b"sv, .line = 1, .column = 3},
			 error_case{.input = "x __ y"sv, .line = 1, .column = 3},
			 error_case{.input = "ok\n*a* **\n"sv, .line = 2, .column = 5},
			 error_case{.input = "_a_ __"sv, .line = 1, .column = 5},
			 error_case{.input = "___"sv, .line = 1, .column = 1},
			 error_case{.input = "****"sv, .line = 1, .column = 1},
		 }) {
		auto const result = parse(input);
		ASSERT_FALSE(result.has_value()) << input;
		EXPECT_EQ(result.error(),
		          (error{.code = errc::empty_emphasis, .line = line, .column = column}))
		    << input;
	}
}

TEST(parse, a_backslash_is_still_literal_in_definitions)
{
	using namespace jimd;

	static constexpr auto input = "[@k\\q]: v\\q\n\n[@k\\q]\n"sv;
	auto const result = parse(input);
	ASSERT_TRUE(result.has_value());
	EXPECT_EQ(result->defs.links.at("k\\q"sv), "v\\q"sv);
}

TEST(parse, heading_text_is_inline_content)
{
	using namespace jimd;

	static constexpr auto input = "## A *bold* and _it_ `code` \\# [@k](u) [^n](x)\n"sv;
	auto const result = parse(input);
	ASSERT_TRUE(result.has_value());
	EXPECT_EQ(
		*result,
		make_doc(
			input,
			{ast::heading{
				.level = 2,
				.children = {text("A "), ast::strong{.children = {text("bold")}}, text(" and "),
				             ast::emphasis{.children = {text("it")}}, text(" "),
				             ast::code_span{.value = "code"sv}, text(" "), text("#"), text(" "),
				             ast::link{.key = "k"sv, .text = "k"sv, .target = "u"sv}, text(" "),
				             ast::reference{.key = "n"sv, .content = ast::inlines{text("x")}}}}}));
}

TEST(parse, heading_references_are_checked_and_defined)
{
	using namespace jimd;

	static constexpr auto input = "# See [^n]\n\n[^n]: the note\n"sv;
	auto const result = parse(input);
	ASSERT_TRUE(result.has_value());
	EXPECT_EQ(result->defs.footnotes.size(), 1U);

	auto const undefined = parse("# See [@nowhere]\n"sv);
	ASSERT_FALSE(undefined.has_value());
	EXPECT_EQ(undefined.error(),
	          (error{.code = errc::undefined_reference, .line = 1, .column = 9}));
}

TEST(parse, heading_errors_are_inline_errors)
{
	using namespace jimd;

	struct heading_error {
		std::string_view input;
		errc code;
		std::size_t line;
		std::size_t column;
	};
	for (auto const& [input, code, line, column] : {
			 heading_error{
				 .input = "# a \\q\n"sv, .code = errc::invalid_escape, .line = 1, .column = 5},
			 heading_error{
				 .input = "# a _b\n"sv, .code = errc::unclosed_emphasis, .line = 1, .column = 7},
			 heading_error{
				 .input = "# a **\n"sv, .code = errc::empty_emphasis, .line = 1, .column = 5},
			 heading_error{
				 .input = "# a [@k\n"sv, .code = errc::unclosed_resource, .line = 1, .column = 8},
			 heading_error{.input = "text\n# a _b\n"sv,
			               .code = errc::unclosed_emphasis,
			               .line = 2,
			               .column = 7},
			 heading_error{
				 .input = ">\n# a \\q\n<\n"sv, .code = errc::invalid_escape, .line = 2, .column = 5},
		 }) {
		auto const result = parse(input);
		ASSERT_FALSE(result.has_value()) << input;
		EXPECT_EQ(result.error(), (error{.code = code, .line = line, .column = column})) << input;
	}
}

TEST(parse, heading_text_ends_at_the_line_end)
{
	using namespace jimd;

	static constexpr auto input = "# a _b_\n_c_\n"sv;
	auto const result = parse(input);
	ASSERT_TRUE(result.has_value());
	EXPECT_EQ(
		*result,
		make_doc(input,
		         {ast::heading{.level = 1,
		                       .children = {text("a "), ast::emphasis{.children = {text("b")}}}},
		          ast::paragraph{.children = {ast::emphasis{.children = {text("c")}}}}}));
}

TEST(parse, heading_needs_blanks_and_text_after_its_hashes)
{
	using namespace jimd;

	for (auto const input : {"# a"sv, "#  \tx"sv, "#\t\tx"sv}) {
		auto const result = parse(input);
		ASSERT_TRUE(result.has_value()) << input;
		auto const& heading = std::get<ast::heading>(result->blocks.front());
		EXPECT_EQ(heading.children.size(), 1U) << input;
		EXPECT_EQ(heading.children.front(), ast::inline_node{text(input.substr(input.size() - 1))})
		    << input;
	}
	for (auto const input : {"# \n"sv, "#  \n"sv, "p\n# \nq\n"sv}) {
		auto const result = parse(input);
		ASSERT_TRUE(result.has_value()) << input;
		ASSERT_EQ(result->blocks.size(), 1U) << input;
		EXPECT_TRUE(std::holds_alternative<ast::paragraph>(result->blocks.front())) << input;
	}
}

TEST(parse, footnote_definition_is_inline_content)
{
	using namespace jimd;

	static constexpr auto input = "[^n]: A *bold* `code` \\_ [@k](u) and _it_\nafter\n"sv;
	auto const result = parse(input);
	ASSERT_TRUE(result.has_value());
	EXPECT_EQ(result->defs.footnotes.at("n"sv),
	          (ast::inlines{text("A "), ast::strong{.children = {text("bold")}}, text(" "),
	                        ast::code_span{.value = "code"sv}, text(" "), text("_"), text(" "),
	                        ast::link{.key = "k"sv, .text = "k"sv, .target = "u"sv}, text(" and "),
	                        ast::emphasis{.children = {text("it")}}}));
	EXPECT_TRUE(result->defs.links.empty());
	ASSERT_EQ(result->blocks.size(), 1U);
	EXPECT_TRUE(std::holds_alternative<ast::paragraph>(result->blocks.front()));
}

TEST(parse, footnote_definition_errors_are_inline_errors)
{
	using namespace jimd;

	struct definition_error {
		std::string_view input;
		errc code;
		std::size_t column;
	};
	for (auto const& [input, code, column] : {
			 definition_error{
				 .input = "[^n]: a _b\n"sv, .code = errc::unclosed_emphasis, .column = 11},
			 definition_error{.input = "[^n]: a \\q\n"sv, .code = errc::invalid_escape, .column = 9},
			 definition_error{
				 .input = "[^n]: a [@k\n"sv, .code = errc::unclosed_resource, .column = 12},
			 definition_error{
				 .input = "[^n]: a [@k]\n"sv, .code = errc::undefined_reference, .column = 11},
		 }) {
		auto const result = parse(input);
		ASSERT_FALSE(result.has_value()) << input;
		EXPECT_EQ(result.error(), (error{.code = code, .line = 1, .column = column})) << input;
	}
}

TEST(parse, a_footnote_may_not_hold_a_footnote_reference)
{
	using namespace jimd;

	struct nested_case {
		std::string_view input;
		std::size_t line;
		std::size_t column;
	};
	for (auto const& [input, line, column] : {
			 nested_case{.input = "[^a]: see [^b]\n[^b]: x\n"sv, .line = 1, .column = 13},
			 nested_case{.input = "[^a]: _see [^b]_\n[^b]: x\n"sv, .line = 1, .column = 14},
			 nested_case{.input = "[^a]: *_see [^b]_*\n"sv, .line = 1, .column = 15},
			 nested_case{.input = "text\n\n[^a]: myself [^a]\n"sv, .line = 3, .column = 16},
			 nested_case{.input = "[^a]: x\n[^b]: y [^a]\n"sv, .line = 2, .column = 11},
		 }) {
		auto const result = parse(input);
		ASSERT_FALSE(result.has_value()) << input;
		EXPECT_EQ(result.error(),
		          (error{.code = errc::nested_footnote, .line = line, .column = column}))
		    << input;
	}
}

TEST(parse, a_footnote_definition_may_hold_links_and_images)
{
	using namespace jimd;

	static constexpr auto input = "A[^n].\n\n[^n]: [!i](p) [@k](u)\n"sv;
	auto const result = parse(input);
	ASSERT_TRUE(result.has_value());
	EXPECT_EQ(result->defs.footnotes.at("n"sv),
	          (ast::inlines{
				  ast::image{.key = "i"sv, .alt = "i"sv, .target = "p"sv, .width = std::nullopt},
				  text(" "), ast::link{.key = "k"sv, .text = "k"sv, .target = "u"sv}}));
	EXPECT_TRUE(result->defs.images.empty());
	EXPECT_TRUE(result->defs.links.empty());
}

TEST(parse, a_nested_footnote_is_found_after_other_nodes)
{
	using namespace jimd;

	for (auto const& [input, column] : {
			 std::pair{"[^a]: text _[^b]_\n"sv, std::size_t{15}},
			 std::pair{"[^a]: *x* _[^b]_\n"sv, std::size_t{14}},
			 std::pair{"[^a]: _a *b _c [^b]_ d* e_\n"sv, std::size_t{18}},
			 std::pair{"[^a]: _x_ and [^b]\n"sv, std::size_t{17}},
		 }) {
		auto const result = parse(input);
		ASSERT_FALSE(result.has_value()) << input;
		EXPECT_EQ(result.error(),
		          (error{.code = errc::nested_footnote, .line = 1, .column = column}))
		    << input;
	}
}

TEST(parse, footnote_definition_shape)
{
	using namespace jimd;

	for (auto const input : {"[^a]: x"sv, "[^a]:x"sv, "[^a]:\t x\n"sv}) {
		auto const result = parse(input);
		ASSERT_TRUE(result.has_value()) << input;
		EXPECT_EQ(result->defs.footnotes.at("a"sv), (ast::inlines{text("x")})) << input;
		EXPECT_TRUE(result->blocks.empty()) << input;
	}
	for (auto const input : {"[^a]:"sv, "[^a]:   \n"sv}) {
		auto const result = parse(input);
		ASSERT_FALSE(result.has_value()) << input;
		EXPECT_EQ(result.error().code, errc::undefined_reference) << input;
	}
}

TEST(parse, inline_footnote_content_is_inline)
{
	using namespace jimd;

	static constexpr auto input = "x[^n](A *bold* `c)` \\) [@k](u) _e)_ z) y"sv;
	auto const result = parse(input);
	ASSERT_TRUE(result.has_value());

	auto const content =
		ast::inlines{text("A "), ast::strong{.children = {text("bold")}},
		             text(" "),  ast::code_span{.value = "c)"sv},
		             text(" "),  text(")"),
		             text(" "),  ast::link{.key = "k"sv, .text = "k"sv, .target = "u"sv},
		             text(" "),  ast::emphasis{.children = {text("e)")}},
		             text(" z")};
	EXPECT_EQ(*result,
	          make_doc(input,
	                   {ast::paragraph{
						   .children = {text("x"), ast::reference{.key = "n"sv, .content = content},
						                text(" y")}}}));
}

TEST(parse, inline_footnote_may_be_empty_and_ends_at_the_first_plain_paren)
{
	using namespace jimd;

	static constexpr auto input = "a[^n]() b[^m](x) (c)"sv;
	auto const result = parse(input);
	ASSERT_TRUE(result.has_value());
	auto const& children = std::get<ast::paragraph>(result->blocks.front()).children;
	EXPECT_EQ(children.at(1),
	          (ast::inline_node{ast::reference{.key = "n"sv, .content = ast::inlines{}}}));
	EXPECT_EQ(children.at(3),
	          (ast::inline_node{ast::reference{.key = "m"sv, .content = ast::inlines{text("x")}}}));
	EXPECT_EQ(children.back(), ast::inline_node{text(" (c)")});
}

TEST(parse, inline_footnote_errors)
{
	using namespace jimd;

	struct footnote_error {
		std::string_view input;
		errc code;
		std::size_t column;
	};
	for (auto const& [input, code, column] : {
			 footnote_error{.input = "a[^n](x"sv, .code = errc::unclosed_resource, .column = 8},
			 footnote_error{.input = "a[^n](x\\)"sv, .code = errc::unclosed_resource, .column = 10},
			 footnote_error{.input = "a[^n](x _y)"sv, .code = errc::unclosed_emphasis, .column = 12},
			 footnote_error{.input = "a[^n](x \\q)"sv, .code = errc::invalid_escape, .column = 9},
			 footnote_error{
				 .input = "a[^n](x [@k)"sv, .code = errc::unclosed_resource, .column = 13},
			 footnote_error{
				 .input = "a[^n](x [@k])"sv, .code = errc::undefined_reference, .column = 11},
			 footnote_error{.input = "a[^n](x [^m])"sv, .code = errc::nested_footnote, .column = 11},
			 footnote_error{
				 .input = "a[^n](_[^m](y)_)"sv, .code = errc::nested_footnote, .column = 10},
		 }) {
		auto const result = parse(input);
		ASSERT_FALSE(result.has_value()) << input;
		EXPECT_EQ(result.error(), (error{.code = code, .line = 1, .column = column})) << input;
	}
}

TEST(parse, inline_footnote_does_not_cross_a_line)
{
	using namespace jimd;

	auto const result = parse("a[^n](x\ny)"sv);
	ASSERT_FALSE(result.has_value());
	EXPECT_EQ(result.error(), (error{.code = errc::unclosed_resource, .line = 1, .column = 8}));
}

TEST(parse, inline_footnote_target_needs_no_blank_before_it)
{
	using namespace jimd;

	for (auto const input : {"a[^k] (x)"sv, "a[^k]\n(x)"sv}) {
		auto const result = parse(input);
		ASSERT_FALSE(result.has_value()) << input;
		EXPECT_EQ(result.error().code, errc::undefined_reference) << input;
	}
}

TEST(parse, an_empty_inline_footnote_still_defines_one)
{
	using namespace jimd;

	auto const result = parse("a[^k]()"sv);
	ASSERT_TRUE(result.has_value());
	auto const& children = std::get<ast::paragraph>(result->blocks.front()).children;
	auto const& note = std::get<ast::reference>(children.at(1));
	EXPECT_EQ(note.content, std::optional<ast::inlines>{ast::inlines{}});
}

TEST(parse, a_run_of_unclosed_inline_footnotes_is_an_error_not_a_crash)
{
	using namespace jimd;

	static constexpr int repeats = 50000;
	std::string input;
	for (int i = 0; i < repeats; ++i) {
		input += "[^k](";
	}
	auto const result = parse(input);
	ASSERT_FALSE(result.has_value());
	EXPECT_EQ(result.error().code, errc::nested_footnote);
	EXPECT_EQ(result.error().column, 8U);
}

TEST(parse, the_first_nested_footnote_is_the_one_reported)
{
	using namespace jimd;

	auto const result = parse("[^n]([^m]([^o](x)))"sv);
	ASSERT_FALSE(result.has_value());
	EXPECT_EQ(result.error().column, 8U);
}

TEST(parse, metadata_lines_at_the_top)
{
	using namespace jimd;

	static constexpr auto input =
		"[%title]: My *page*  \n\n[%description]:\tA page, with _raw_ [@text] \\ \n[%x y]: 1\n\n# Head\n"sv;
	auto const result = parse(input);
	ASSERT_TRUE(result.has_value());

	std::flat_map<std::string_view, std::string_view> expected;
	expected["title"] = "My *page*";
	expected["description"] = "A page, with _raw_ [@text] \\";
	expected["x y"] = "1";
	EXPECT_EQ(result->metadata, expected);
	EXPECT_EQ(result->blocks,
	          (std::vector<ast::block>{ast::heading{.level = 1, .children = {text("Head")}}}));
	EXPECT_TRUE(result->defs.links.empty());
}

TEST(parse, metadata_may_follow_blank_lines_and_stand_alone)
{
	using namespace jimd;

	for (auto const input : {"\n\n[%a]: b\n"sv, "[%a]: b"sv, "[%a]: b\n\n\n"sv}) {
		auto const result = parse(input);
		ASSERT_TRUE(result.has_value()) << input;
		EXPECT_EQ(result->metadata.size(), 1U) << input;
		EXPECT_EQ(result->metadata.at("a"sv), "b"sv) << input;
		EXPECT_TRUE(result->blocks.empty()) << input;
	}
}

TEST(parse, a_metadata_line_after_content_is_plain_text)
{
	using namespace jimd;

	static constexpr auto input = "Intro\n[%a]: b\n\n[%c]: d\n"sv;
	auto const result = parse(input);
	ASSERT_TRUE(result.has_value());
	EXPECT_TRUE(result->metadata.empty());
	EXPECT_EQ(result->blocks,
	          (std::vector<ast::block>{
				  ast::paragraph{.children = {text("Intro"), ast::soft_break{}, text("[%a]: b")}},
				  ast::paragraph{.children = {text("[%c]: d")}}}));
}

TEST(parse, metadata_inside_a_quote_is_plain_text)
{
	using namespace jimd;

	static constexpr auto input = ">\n[%a]: b\n<\n"sv;
	auto const result = parse(input);
	ASSERT_TRUE(result.has_value());
	EXPECT_TRUE(result->metadata.empty());
	EXPECT_EQ(result->blocks.size(), 1U);
}

TEST(parse, malformed_metadata_lines_are_plain_text)
{
	using namespace jimd;

	for (auto const input :
	     {"[%]: v"sv, "[%k]:"sv, "[%k]: "sv, "[%k] v"sv, "[%k"sv, " [%k]: v"sv}) {
		auto const result = parse(input);
		ASSERT_TRUE(result.has_value()) << input;
		EXPECT_TRUE(result->metadata.empty()) << input;
		ASSERT_EQ(result->blocks.size(), 1U) << input;
	}
}

TEST(parse, duplicate_metadata_error)
{
	using namespace jimd;

	auto const result = parse("[%a]: 1\n\n[%b]: 2\n[%a]: 3\n"sv);
	ASSERT_FALSE(result.has_value());
	EXPECT_EQ(result.error(), (error{.code = errc::duplicate_metadata, .line = 4, .column = 3}));
}

TEST(parse, metadata_keys_are_case_sensitive_and_do_not_clash_with_definitions)
{
	using namespace jimd;

	static constexpr auto input = "[%a]: 1\n[%A]: 2\n[@a]: u\n[^a]: n\n[!a]: p\n"sv;
	auto const result = parse(input);
	ASSERT_TRUE(result.has_value());
	EXPECT_EQ(result->metadata.size(), 2U);
	EXPECT_EQ(result->defs.links.size(), 1U);
	EXPECT_EQ(result->defs.footnotes.size(), 1U);
	EXPECT_EQ(result->defs.images.size(), 1U);
}

TEST(parse, link_target_may_hold_one_level_of_parentheses)
{
	using namespace jimd;

	static constexpr auto input =
		"[@w](https://en.wikipedia.org/wiki/Foo_(bar)) [!i](a(b)c.png) [@e](()) (see [@a](x)) end"sv;
	auto const result = parse(input);
	ASSERT_TRUE(result.has_value());
	auto const& children = std::get<ast::paragraph>(result->blocks.front()).children;
	EXPECT_EQ(std::get<ast::link>(children.at(0)).target,
	          "https://en.wikipedia.org/wiki/Foo_(bar)"sv);
	EXPECT_EQ(std::get<ast::image>(children.at(2)).target, "a(b)c.png"sv);
	EXPECT_EQ(std::get<ast::link>(children.at(4)).target, "()"sv);
	EXPECT_EQ(std::get<ast::link>(children.at(6)).target, "x"sv);
	EXPECT_EQ(children.back(), ast::inline_node{text(") end")});
}

TEST(parse, only_one_level_of_parentheses_is_balanced)
{
	using namespace jimd;

	static constexpr auto input = "[@a](x(y(z)))"sv;
	auto const result = parse(input);
	ASSERT_TRUE(result.has_value());
	auto const& children = std::get<ast::paragraph>(result->blocks.front()).children;
	EXPECT_EQ(std::get<ast::link>(children.front()).target, "x(y(z)"sv);
	EXPECT_EQ(children.back(), ast::inline_node{text(")")});
}

TEST(parse, an_unbalanced_parenthesis_in_a_target)
{
	using namespace jimd;

	static constexpr auto input = "[@a](x(y)"sv;
	auto const result = parse(input);
	ASSERT_FALSE(result.has_value());
	EXPECT_EQ(result.error(), (error{.code = errc::unclosed_resource, .line = 1, .column = 10}));

	auto const lone = parse("[@a](x(y"sv);
	ASSERT_FALSE(lone.has_value());
	EXPECT_EQ(lone.error().code, errc::unclosed_resource);

	auto const crossing = parse("[@a](x(y\n)z)"sv);
	ASSERT_FALSE(crossing.has_value());
	EXPECT_EQ(crossing.error(), (error{.code = errc::unclosed_resource, .line = 1, .column = 9}));
}

TEST(parse, a_footnote_still_ends_at_its_first_plain_parenthesis)
{
	using namespace jimd;

	static constexpr auto input = "a[^n](b(c)) d"sv;
	auto const result = parse(input);
	ASSERT_TRUE(result.has_value());
	auto const& children = std::get<ast::paragraph>(result->blocks.front()).children;
	EXPECT_EQ(std::get<ast::reference>(children.at(1)).content, (ast::inlines{text("b(c")}));
}

TEST(parse, targets_that_changed_meaning_with_balanced_parentheses)
{
	using namespace jimd;

	struct target_case {
		std::string_view input;
		std::string_view target;
		std::string_view rest;
	};
	for (auto const& [input, target, rest] : {
			 target_case{.input = "[@a](x(y))"sv, .target = "x(y)"sv, .rest = ""sv},
			 target_case{.input = "[@a](x(y)z(w))"sv, .target = "x(y)z(w)"sv, .rest = ""sv},
			 target_case{.input = "[@a]((()))"sv, .target = "(()"sv, .rest = ")"sv},
			 target_case{.input = "[@a]( (a) )"sv, .target = " (a) "sv, .rest = ""sv},
		 }) {
		auto const result = parse(input);
		ASSERT_TRUE(result.has_value()) << input;
		auto const& children = std::get<ast::paragraph>(result->blocks.front()).children;
		EXPECT_EQ(std::get<ast::link>(children.front()).target, target) << input;
		EXPECT_EQ(children.size(), rest.empty() ? 1U : 2U) << input;
		if (!rest.empty()) {
			EXPECT_EQ(children.back(), ast::inline_node{text(rest)}) << input;
		}
	}
	for (auto const input : {"[@a](x(y)z"sv, "[@a](x(y) and (z)"sv, "[@a](\\()"sv, "[!i](a(b)"sv}) {
		auto const result = parse(input);
		ASSERT_FALSE(result.has_value()) << input;
		EXPECT_EQ(result.error().code, errc::unclosed_resource) << input;
	}
}

TEST(parse, an_underscore_between_alphanumerics_is_part_of_the_word)
{
	using namespace jimd;

	for (auto const input : {"snake_case word"sv, "a_b_c"sv, "x1_2y"sv, "file_name.txt"sv,
	                         "10_000"sv, "Z_a"sv, "end_of_line"sv}) {
		auto const result = parse(input);
		ASSERT_TRUE(result.has_value()) << input;
		EXPECT_EQ(*result, make_doc(input, {ast::paragraph{.children = {text(input)}}})) << input;
	}
}

TEST(parse, an_underscore_not_flanked_by_alphanumerics_still_starts_emphasis)
{
	using namespace jimd;

	for (auto const input : {"snake _case"sv, "snake_ case"sv, "_snake"sv, "a_"sv, "a__b"sv,
	                         "-_a"sv, "a_-b"sv, "\xC3\xA9_a"sv, "a_\xC3\xA9"sv}) {
		auto const result = parse(input);
		ASSERT_FALSE(result.has_value()) << input;
		EXPECT_TRUE(result.error().code == errc::unclosed_emphasis ||
		            result.error().code == errc::empty_emphasis)
		    << input;
	}
}

TEST(parse, intra_word_underscores_in_and_around_emphasis)
{
	using namespace jimd;

	static constexpr auto input = "_a_b_ and *c_d* and _e_ f_g"sv;
	auto const result = parse(input);
	ASSERT_TRUE(result.has_value());
	EXPECT_EQ(*result,
	          make_doc(input,
	                   {ast::paragraph{
						   .children = {ast::emphasis{.children = {text("a_b")}}, text(" and "),
						                ast::strong{.children = {text("c_d")}}, text(" and "),
						                ast::emphasis{.children = {text("e")}}, text(" f_g")}}}));
}

TEST(parse, intra_word_underscores_in_headings_footnotes_and_definitions)
{
	using namespace jimd;

	static constexpr auto input = "# my_heading\n\nx[^a](in_note) y[^b]\n\n[^b]: other_note\n"sv;
	auto const result = parse(input);
	ASSERT_TRUE(result.has_value());
	EXPECT_EQ(std::get<ast::heading>(result->blocks.front()).children,
	          (ast::inlines{text("my_heading")}));
	auto const& children = std::get<ast::paragraph>(result->blocks.at(1)).children;
	EXPECT_EQ(std::get<ast::reference>(children.at(1)).content, (ast::inlines{text("in_note")}));
	EXPECT_EQ(result->defs.footnotes.at("b"sv), (ast::inlines{text("other_note")}));
}

TEST(parse, an_escaped_underscore_still_works_beside_a_word)
{
	using namespace jimd;

	static constexpr auto input = "snake\\_ case a\\_b"sv;
	auto const result = parse(input);
	ASSERT_TRUE(result.has_value());
	EXPECT_EQ(*result, make_doc(input, {ast::paragraph{.children = {text("snake"), text("_"),
	                                                                text(" case a"), text("_"),
	                                                                text("b")}}}));
}

TEST(parse, an_underscore_after_another_construct_is_still_a_marker)
{
	using namespace jimd;

	static constexpr auto input = "*b*_c_ [@k](u)_x_ `y`_z_"sv;
	auto const result = parse(input);
	ASSERT_TRUE(result.has_value());
	EXPECT_EQ(*result,
	          make_doc(input,
	                   {ast::paragraph{
						   .children = {ast::strong{.children = {text("b")}},
						                ast::emphasis{.children = {text("c")}}, text(" "),
						                ast::link{.key = "k"sv, .text = "k"sv, .target = "u"sv},
						                ast::emphasis{.children = {text("x")}}, text(" "),
						                ast::code_span{.value = "y"sv},
						                ast::emphasis{.children = {text("z")}}}}}));

	for (auto const bad : {")_b"sv, "]_b"sv, ".-_b"sv}) {
		auto const failed = parse(bad);
		ASSERT_FALSE(failed.has_value()) << bad;
		EXPECT_EQ(failed.error().code, errc::unclosed_emphasis) << bad;
	}
}

TEST(parse, intra_word_range_ends)
{
	using namespace jimd;

	for (auto const input : {"9_a"sv, "a_Z"sv, "A_z"sv, "0_9"sv}) {
		auto const result = parse(input);
		ASSERT_TRUE(result.has_value()) << input;
		EXPECT_EQ(*result, make_doc(input, {ast::paragraph{.children = {text(input)}}})) << input;
	}
	for (auto const input : {"/_a"sv, "a_:"sv, "@_a"sv, "a_["sv}) {
		EXPECT_FALSE(parse(input).has_value()) << input;
	}
}

namespace
{
auto to_crlf(std::string_view text) -> std::string
{
	std::string out;
	for (char const chr : text) {
		if (chr == '\n') {
			out += '\r';
		}
		out += chr;
	}
	return out;
}
}

TEST(parse, crlf_documents_parse_like_lf_documents)
{
	using namespace jimd;

	for (auto const lf_input : {
			 "[%title]: T\n[%description]: D\n\n# Head *x*\n\nA para\nwith two lines and _em_.\n\n"
			 ">\nquoted\n>\nnested\n<\n<\n\n[@k]: url\n[!i]: pic\n[^n]: a *note*\n\n"
			 "Use [@k], [!i] and [^n].\n"sv,
			 "text\n# heading\n> not a quote\n"sv,
			 ">\n# in a quote\n<\n"sv,
			 "a[^m](inline\\) note) b\n"sv,
			 "[@k](u)\n[@k]\n\n[@k]: v\n"sv,
			 "\n\n  \nonly blanks before\n"sv,
			 "text\n- a _b_\n- c\n\n>\n- d\n<\n"sv,
			 "a $x_1$ _b $y$_ [^n](c $z$)\n- $w$\n"sv,
			 "[!a](x 75%) [!a]\n\n[!a]: y z 5%\n[!b]: y z 5%\n[!b]\n"sv,
		 }) {
		auto const crlf_input = to_crlf(lf_input);
		auto const lf_result = parse(lf_input);
		auto const crlf_result = parse(crlf_input);
		ASSERT_TRUE(lf_result.has_value()) << lf_input;
		ASSERT_TRUE(crlf_result.has_value()) << lf_input;
		EXPECT_EQ(crlf_result->metadata, lf_result->metadata) << lf_input;
		EXPECT_EQ(crlf_result->blocks, lf_result->blocks) << lf_input;
		EXPECT_EQ(crlf_result->defs, lf_result->defs) << lf_input;
	}
}

TEST(parse, crlf_documents_fail_like_lf_documents)
{
	using namespace jimd;

	for (auto const lf_input : {
			 ">\nunclosed\n"sv,
			 "```\nunclosed\n"sv,
			 "a _b\nc\n"sv,
			 "x **\n"sv,
			 "[@a\n"sv,
			 "[@a](u\n"sv,
			 "line\n\\q\n"sv,
			 "[^n]: see [^m]\n"sv,
			 "[%a]: 1\n[%a]: 2\n"sv,
			 "[@a]: 1\n[@a]: 2\n"sv,
			 "# head _x\n"sv,
			 "- a\n- b _c\n"sv,
			 "a $b\nc$\n"sv,
			 "a $$ b\n"sv,
			 "$$\nx\n"sv,
			 "$$\n \n$$\n"sv,
		 }) {
		auto const lf_result = parse(lf_input);
		auto const crlf_input = to_crlf(lf_input);
		auto const crlf_result = parse(crlf_input);
		ASSERT_FALSE(lf_result.has_value()) << lf_input;
		ASSERT_FALSE(crlf_result.has_value()) << lf_input;
		EXPECT_EQ(crlf_result.error(), lf_result.error()) << lf_input;
	}
}

TEST(parse, crlf_leaves_no_carriage_return_in_the_tree)
{
	using namespace jimd;

	static constexpr auto input =
		"[%x]: v\r\n[%title]:\r\n# H\r\n>\r\nq\r\n<\r\n[@k]: u\r\np\r\nq\r\n"sv;
	auto const result = parse(input);
	ASSERT_TRUE(result.has_value());
	EXPECT_EQ(result->metadata.size(), 1U);
	EXPECT_EQ(result->metadata.at("x"sv), "v"sv);
	EXPECT_EQ(result->defs.links.at("k"sv), "u"sv);
	EXPECT_EQ(result->blocks,
	          (std::vector<ast::block>{
				  ast::paragraph{.children = {text("[%title]:")}},
				  ast::heading{.level = 1, .children = {text("H")}},
				  ast::quote{.children = {ast::paragraph{.children = {text("q")}}}},
				  ast::paragraph{.children = {text("p"), ast::soft_break{}, text("q")}}}));
}

TEST(parse, a_lone_carriage_return_is_still_text)
{
	using namespace jimd;

	static constexpr auto input = "a\rb\r\nc\r"sv;
	auto const result = parse(input);
	ASSERT_TRUE(result.has_value());
	EXPECT_EQ(result->blocks, (std::vector<ast::block>{ast::paragraph{
								  .children = {text("a\rb"), ast::soft_break{}, text("c\r")}}}));
}
