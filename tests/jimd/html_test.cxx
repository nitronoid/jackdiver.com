#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <expected>
#include <flat_map>
#include <format>
#include <functional>
#include <optional>
#include <ranges>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <jimd/ast.hxx>
#include <jimd/detail/align.hxx>
#include <jimd/detail/position.hxx>
#include <jimd/html.hxx>
#include <jimd/parse.hxx>
#include <support/result_printers.hxx>

using namespace std::literals;

namespace
{
auto text_of(std::string_view input) -> std::string
{
	std::string out;
	jimd::html::escape_text(input, out);
	return out;
}

auto attr_of(std::string_view input) -> std::string
{
	std::string out;
	jimd::html::escape_attr(input, out);
	return out;
}

auto render_blocks(std::vector<jimd::ast::block> blocks) -> std::string
{
	jimd::ast::document doc;
	doc.blocks = std::move(blocks);
	auto const html = jimd::html::render(doc);
	EXPECT_TRUE(html.has_value()) << testing::PrintToString(html);
	return html.value_or("");
}

auto render_with(jimd::html::math_table const& table, std::vector<jimd::ast::block> blocks)
	-> std::string
{
	jimd::ast::document doc;
	doc.blocks = std::move(blocks);
	auto const html = jimd::html::render(doc, {.math = &table});
	EXPECT_TRUE(html.has_value()) << testing::PrintToString(html);
	return html.value_or("");
}

auto render_with_code(jimd::html::code_table const& table, std::vector<jimd::ast::block> blocks)
	-> std::string
{
	jimd::ast::document doc;
	doc.blocks = std::move(blocks);
	auto const html = jimd::html::render(doc, {.code = &table});
	EXPECT_TRUE(html.has_value()) << testing::PrintToString(html);
	return html.value_or("");
}

auto paragraph_of(jimd::ast::inline_node node) -> jimd::ast::paragraph
{
	return jimd::ast::paragraph{.children = {std::move(node)}};
}

auto heading_of(std::size_t level, std::string_view content) -> jimd::ast::heading
{
	return jimd::ast::heading{.level = level, .children = {jimd::ast::text{content}}};
}

auto slug_of(std::string_view content) -> std::string
{
	return jimd::html::slugify({jimd::ast::text{content}});
}

auto note_ref(std::size_t number) -> std::string
{
	return std::format(R"(<sup class="note-ref" style="anchor-name: --note-{0}">)"
	                   R"(<a href="#note-{0}" id="noteref-{0}">{0}</a></sup>)",
	                   number);
}

auto note_link(std::size_t number) -> std::string
{
	return std::format(R"(<sup class="note-ref"><a href="#note-{0}">{0}</a></sup>)", number);
}

auto note_aside(std::size_t number, std::string_view content) -> std::string
{
	return std::format(
		R"(<aside id="note-{0}" data-note="{0}" style="position-anchor: --note-{0}">)"
		"\n<p>{1}</p>\n</aside>\n",
		number, content);
}

auto paragraph_with_reference(std::string_view text, std::string_view key) -> jimd::ast::paragraph
{
	return jimd::ast::paragraph{
		.children = {jimd::ast::text{text},
		             jimd::ast::reference{.key = key, .content = std::nullopt}}};
}

auto paragraph_with_note(std::string_view text, std::string_view key, std::string_view content)
	-> jimd::ast::paragraph
{
	return jimd::ast::paragraph{
		.children = {jimd::ast::text{text},
		             jimd::ast::reference{
						 .key = key, .content = jimd::ast::inlines{jimd::ast::text{content}}}}};
}
}

TEST(html_escape, text_escapes_each_special_character)
{
	EXPECT_EQ(text_of("&"sv), "&amp;");
	EXPECT_EQ(text_of("<"sv), "&lt;");
	EXPECT_EQ(text_of(">"sv), "&gt;");
}

TEST(html_escape, text_leaves_quotes_alone)
{
	EXPECT_EQ(text_of(R"("'")"sv), R"("'")");
}

TEST(html_escape, text_mixed)
{
	EXPECT_EQ(text_of("a < b && c > d"sv), "a &lt; b &amp;&amp; c &gt; d");
	EXPECT_EQ(text_of("&amp;"sv), "&amp;amp;");
}

TEST(html_escape, text_passes_utf8_through)
{
	EXPECT_EQ(text_of("caf\xC3\xA9 <"sv), "caf\xC3\xA9 &lt;");
}

TEST(html_escape, text_empty_and_plain)
{
	EXPECT_EQ(text_of(""sv), "");
	EXPECT_EQ(text_of("plain text"sv), "plain text");
}

TEST(html_escape, attr_escapes_each_special_character)
{
	EXPECT_EQ(attr_of("&"sv), "&amp;");
	EXPECT_EQ(attr_of("<"sv), "&lt;");
	EXPECT_EQ(attr_of(">"sv), "&gt;");
	EXPECT_EQ(attr_of("\""sv), "&quot;");
}

TEST(html_escape, attr_leaves_apostrophes_alone)
{
	EXPECT_EQ(attr_of("it's"sv), "it's");
}

TEST(html_escape, attr_mixed)
{
	EXPECT_EQ(attr_of(R"(a="b"&<c>)"sv), "a=&quot;b&quot;&amp;&lt;c&gt;");
}

TEST(html_escape, attr_empty_and_plain)
{
	EXPECT_EQ(attr_of(""sv), "");
	EXPECT_EQ(attr_of("plain"sv), "plain");
}

TEST(html_escape, appends_to_existing_output)
{
	std::string out = "x=";
	jimd::html::escape_attr("<"sv, out);
	jimd::html::escape_text(">"sv, out);
	EXPECT_EQ(out, "x=&lt;&gt;");
}

TEST(html_render, empty_document)
{
	EXPECT_EQ(render_blocks({}), "");
}

TEST(html_render, paragraph_with_text)
{
	namespace ast = jimd::ast;
	EXPECT_EQ(render_blocks({ast::paragraph{.children = {ast::text{"hello"sv}}}}),
	          "<p>hello</p>\n");
}

TEST(html_render, text_is_escaped)
{
	namespace ast = jimd::ast;
	EXPECT_EQ(render_blocks({ast::paragraph{.children = {ast::text{"a < b & \"c\""sv}}}}),
	          "<p>a &lt; b &amp; “c”</p>\n");
}

TEST(html_render, soft_break)
{
	namespace ast = jimd::ast;
	EXPECT_EQ(render_blocks({ast::paragraph{
				  .children = {ast::text{"one"sv}, ast::soft_break{}, ast::text{"two"sv}},
			  }}),
	          "<p>one\ntwo</p>\n");
}

TEST(html_render, blocks_each_start_on_their_own_line)
{
	namespace ast = jimd::ast;
	EXPECT_EQ(render_blocks({
				  ast::paragraph{.children = {ast::text{"first"sv}}},
				  ast::paragraph{.children = {ast::text{"second"sv}}},
			  }),
	          "<p>first</p>\n<p>second</p>\n");
}

TEST(html_render, quote)
{
	namespace ast = jimd::ast;
	EXPECT_EQ(render_blocks({ast::quote{.children = {ast::paragraph{
											.children = {ast::text{"quoted"sv}},
										}}}}),
	          "<blockquote>\n<p>quoted</p>\n</blockquote>\n");
}

TEST(html_render, nested_quote)
{
	namespace ast = jimd::ast;
	EXPECT_EQ(
		render_blocks({ast::quote{
			.children =
				{
					ast::paragraph{.children = {ast::text{"outer"sv}}},
					ast::quote{.children =
					               {
									   ast::paragraph{.children = {ast::text{"inner"sv}}},
								   }},
				}}}),
		"<blockquote>\n<p>outer</p>\n<blockquote>\n<p>inner</p>\n</blockquote>\n</blockquote>\n");
}

TEST(html_render, code_block_without_tag)
{
	namespace ast = jimd::ast;
	EXPECT_EQ(render_blocks({ast::code_block{.info = ""sv, .body = "int x;\n"sv}}),
	          "<pre><code>int x;\n</code></pre>\n");
}

TEST(html_render, code_block_with_tag)
{
	namespace ast = jimd::ast;
	EXPECT_EQ(render_blocks({ast::code_block{.info = "cpp"sv, .body = "int x;\n"sv}}),
	          "<pre><code class=\"language-cpp\">int x;\n</code></pre>\n");
}

TEST(html_render, code_block_tag_is_the_first_word_and_escaped)
{
	namespace ast = jimd::ast;
	EXPECT_EQ(render_blocks({ast::code_block{.info = "c\"x title=y"sv, .body = ""sv}}),
	          "<pre><code class=\"language-c&quot;x\"></code></pre>\n");
}

TEST(html_render, code_block_body_is_escaped_and_not_markup)
{
	namespace ast = jimd::ast;
	EXPECT_EQ(render_blocks({ast::code_block{.info = ""sv, .body = "<b>*a*</b> & \"q\"\n"sv}}),
	          "<pre><code>&lt;b&gt;*a*&lt;/b&gt; &amp; \"q\"\n</code></pre>\n");
}

TEST(html_render, code_block_crlf_becomes_lf)
{
	namespace ast = jimd::ast;
	EXPECT_EQ(render_blocks({ast::code_block{.info = ""sv, .body = "a\r\nb\r\n"sv}}),
	          "<pre><code>a\nb\n</code></pre>\n");
}

TEST(html_render, code_block_lone_cr_is_kept)
{
	namespace ast = jimd::ast;
	EXPECT_EQ(render_blocks({ast::code_block{.info = ""sv, .body = "a\rb\r\r\n"sv}}),
	          "<pre><code>a\rb\r\n</code></pre>\n");
}

TEST(html_render, code_block_tag_ends_at_a_tab)
{
	namespace ast = jimd::ast;
	EXPECT_EQ(render_blocks({ast::code_block{.info = "cpp\tx"sv, .body = ""sv}}),
	          "<pre><code class=\"language-cpp\"></code></pre>\n");
}

TEST(html_render, code_block_body_of_only_crlf)
{
	namespace ast = jimd::ast;
	EXPECT_EQ(render_blocks({ast::code_block{.info = ""sv, .body = "\r\n"sv}}),
	          "<pre><code>\n</code></pre>\n");
}

TEST(html_render, code_block_inside_quote)
{
	namespace ast = jimd::ast;
	EXPECT_EQ(
		render_blocks({ast::quote{.children = {ast::code_block{.info = ""sv, .body = "a\r\n"sv}}}}),
		"<blockquote>\n<pre><code>a\n</code></pre>\n</blockquote>\n");
}

TEST(html_render, a_code_block_in_the_table_writes_its_entry_as_it_is)
{
	namespace ast = jimd::ast;
	jimd::html::code_table const table{
		{{"cpp", "int x = 1 < 2;\n"}, "<span class=\"hljs-type\">int</span> x = 1 &lt; 2;\n"}};
	EXPECT_EQ(render_with_code(
				  table, {ast::code_block{.info = "cpp title=x"sv, .body = "int x = 1 < 2;\n"sv}}),
	          "<pre class=\"highlight\"><code class=\"language-cpp\">"
	          "<span class=\"hljs-type\">int</span> x = 1 &lt; 2;\n</code></pre>\n");
}

TEST(html_render, a_code_entry_is_not_escaped_again)
{
	namespace ast = jimd::ast;
	jimd::html::code_table const table{{{"c", "<&>"}, "&lt;&amp;&gt;<b>\"'"}};
	EXPECT_EQ(render_with_code(table, {ast::code_block{.info = "c"sv, .body = "<&>"sv}}),
	          "<pre class=\"highlight\"><code class=\"language-c\">&lt;&amp;&gt;<b>\"'"
	          "</code></pre>\n");
}

TEST(html_render, a_code_entry_is_keyed_by_the_body_with_each_crlf_turned_into_lf)
{
	namespace ast = jimd::ast;
	jimd::html::code_table const table{{{"c", "a\nb\r\n"}, "x"}};
	EXPECT_EQ(render_with_code(table, {ast::code_block{.info = "c"sv, .body = "a\r\nb\r\r\n"sv}}),
	          "<pre class=\"highlight\"><code class=\"language-c\">x</code></pre>\n");
}

TEST(html_render, a_code_block_not_in_the_table_is_written_as_without_one)
{
	namespace ast = jimd::ast;
	jimd::html::code_table const table{{{"cpp", "a"}, "<i>A</i>"}, {{"c", "b"}, "<i>B</i>"}};
	EXPECT_EQ(render_with_code(table, {ast::code_block{.info = "cpp"sv, .body = "b"sv},
	                                   ast::code_block{.info = "rust"sv, .body = "a"sv},
	                                   ast::code_block{.info = "c"sv, .body = "a<"sv}}),
	          "<pre><code class=\"language-cpp\">b</code></pre>\n"
	          "<pre><code class=\"language-rust\">a</code></pre>\n"
	          "<pre><code class=\"language-c\">a&lt;</code></pre>\n");
	EXPECT_EQ(render_with_code({}, {ast::code_block{.info = "cpp"sv, .body = "a"sv}}),
	          "<pre><code class=\"language-cpp\">a</code></pre>\n");
}

TEST(html_render, a_code_block_with_no_language_is_never_looked_up)
{
	namespace ast = jimd::ast;
	jimd::html::code_table const table{{{"", "a<"}, "<i>A</i>"}, {{"", ""}, "<i>X</i>"}};
	EXPECT_EQ(render_with_code(table, {ast::code_block{.info = ""sv, .body = "a<"sv}}),
	          "<pre><code>a&lt;</code></pre>\n");
	EXPECT_EQ(render_with_code(table, {ast::code_block{.info = " x"sv, .body = ""sv}}),
	          "<pre><code></code></pre>\n");
}

TEST(html_render, a_code_entry_is_written_for_each_block_in_a_quote_and_the_language_is_escaped)
{
	namespace ast = jimd::ast;
	jimd::html::code_table const table{{{"c\"x", "a"}, "<i>a</i>"}};
	EXPECT_EQ(render_with_code(
				  table,
				  {ast::quote{.children = {ast::quote{.children = {ast::code_block{
														  .info = "c\"x y"sv, .body = "a"sv}}},
				                           ast::code_block{.info = "c\"x"sv, .body = "a"sv}}}}),
	          "<blockquote>\n<blockquote>\n"
	          "<pre class=\"highlight\"><code class=\"language-c&quot;x\"><i>a</i></code></pre>\n"
	          "</blockquote>\n"
	          "<pre class=\"highlight\"><code class=\"language-c&quot;x\"><i>a</i></code></pre>\n"
	          "</blockquote>\n");
}

TEST(html_render, an_empty_code_table_changes_nothing)
{
	auto const doc = jimd::parse("```cpp\nx\n```\n");
	ASSERT_TRUE(doc.has_value());
	jimd::html::code_table const table;
	EXPECT_EQ(jimd::html::render(*doc, {.math = nullptr, .code = &table}),
	          "<pre><code class=\"language-cpp\">x\n</code></pre>\n");
}

TEST(html_render, list)
{
	namespace ast = jimd::ast;
	EXPECT_EQ(render_blocks({ast::list{
				  .items = {{ast::text{"one"sv}},
				            {ast::text{"a "sv}, ast::emphasis{.children = {ast::text{"b"sv}}}},
				            {ast::code_span{"c"sv}}}}}),
	          "<ul>\n<li>one</li>\n<li>a <em>b</em></li>\n<li><code>c</code></li>\n</ul>\n");
}

TEST(html_render, list_items_are_escaped)
{
	namespace ast = jimd::ast;
	EXPECT_EQ(render_blocks({ast::list{.items = {{ast::text{"<&>"sv}}}}}),
	          "<ul>\n<li>&lt;&amp;&gt;</li>\n</ul>\n");
}

TEST(html_render, an_empty_list_and_an_empty_item)
{
	namespace ast = jimd::ast;
	EXPECT_EQ(render_blocks({ast::list{}}), "<ul>\n</ul>\n");
	EXPECT_EQ(render_blocks({ast::list{.items = {{}}}}), "<ul>\n<li></li>\n</ul>\n");
}

TEST(html_render, list_between_other_blocks_and_in_a_quote)
{
	namespace ast = jimd::ast;
	EXPECT_EQ(render_blocks({paragraph_of(ast::text{"before"sv}),
	                         ast::quote{.children = {ast::list{.items = {{ast::text{"in"sv}}}}}},
	                         ast::list{.items = {{ast::text{"after"sv}}}}}),
	          "<p>before</p>\n<blockquote>\n<ul>\n<li>in</li>\n</ul>\n</blockquote>\n"
	          "<ul>\n<li>after</li>\n</ul>\n");
}

TEST(html_render, list_item_with_a_link_and_a_footnote)
{
	namespace ast = jimd::ast;
	ast::document doc;
	doc.defs.footnotes.emplace("k"sv, ast::inlines{ast::text{"n"sv}});
	doc.blocks = {ast::list{.items = {{ast::link{.key = "l"sv, .text = "t"sv, .target = "/u"sv},
	                                   ast::reference{.key = "k"sv, .content = std::nullopt}},
	                                  {ast::reference{.key = "k"sv, .content = std::nullopt}}}}};
	EXPECT_EQ(jimd::html::render(doc), "<ul>\n<li><a href=\"/u\">t</a>" + note_ref(1) +
	                                       "</li>\n<li>" + note_link(1) + "</li>\n</ul>\n" +
	                                       note_aside(1, "n"));
}

TEST(html_render, an_unresolved_link_in_a_list_item_fails_the_render)
{
	namespace ast = jimd::ast;
	std::string_view const source = "- [@gone]"sv;
	std::string_view const key = "gone"sv;
	ast::document doc;
	doc.source = source;
	doc.blocks = {ast::list{.items = {{ast::link{.key = source.substr(source.find(key), key.size()),
	                                             .text = key,
	                                             .target = std::nullopt}}}}};
	EXPECT_EQ(jimd::html::render(doc), std::unexpected(jimd::error{
										   .code = jimd::errc::undefined_reference,
										   .line = 1,
										   .column = 5,
									   }));
}

TEST(html_render, inline_math_writes_its_table_entry_as_it_is)
{
	namespace ast = jimd::ast;
	jimd::html::math_table const table{
		{{jimd::html::math_mode::inline_math, "a<b"}, "<math>&lt;</math>"}};
	EXPECT_EQ(render_with(table, {ast::paragraph{.children = {ast::text{"x "sv}, ast::math{"a<b"sv},
	                                                          ast::text{" y"sv}}}}),
	          "<p>x <math>&lt;</math> y</p>\n");
}

TEST(html_render, inline_math_looks_up_the_inline_entry_for_its_tex)
{
	namespace ast = jimd::ast;
	using jimd::html::math_mode;
	jimd::html::math_table const table{{{math_mode::display, "x"}, "<math display=\"block\"/>"},
	                                   {{math_mode::inline_math, "x"}, "<math>x</math>"},
	                                   {{math_mode::inline_math, "xx"}, "<math>xx</math>"},
	                                   {{math_mode::inline_math, "y"}, "<math>y</math>"}};
	EXPECT_EQ(render_with(table, {paragraph_of(ast::math{"y"sv}), paragraph_of(ast::math{"x"sv}),
	                              paragraph_of(ast::math{"xx"sv})}),
	          "<p><math>y</math></p>\n<p><math>x</math></p>\n<p><math>xx</math></p>\n");
}

TEST(html_render, inline_math_is_found_whatever_view_holds_its_tex)
{
	namespace ast = jimd::ast;
	jimd::html::math_table const table{{{jimd::html::math_mode::inline_math, "x"}, "<math/>"}};
	std::string const tex = "x";
	EXPECT_EQ(render_with(table, {paragraph_of(ast::math{tex})}), "<p><math/></p>\n");
}

TEST(html_render, inline_math_inside_other_inlines_blocks_and_footnotes)
{
	namespace ast = jimd::ast;
	jimd::html::math_table const table{{{jimd::html::math_mode::inline_math, "x"}, "<math/>"}};
	auto const math_node = ast::math{"x"sv};
	EXPECT_EQ(render_with(
				  table,
				  {ast::paragraph{.children = {ast::emphasis{.children = {math_node}},
				                               ast::strong{.children = {math_node}}}},
				   ast::list{.items = {{math_node}}},
				   ast::quote{.children = {paragraph_of(math_node)}},
				   paragraph_of(ast::reference{.key = "k"sv, .content = ast::inlines{math_node}})}),
	          "<p><em><math/></em><strong><math/></strong></p>\n<ul>\n<li><math/></li>\n</ul>\n"
	          "<blockquote>\n<p><math/></p>\n</blockquote>\n<p>" +
	              note_ref(1) + "</p>\n" + note_aside(1, "<math/>"));
}

TEST(html_render, a_heading_with_math_takes_its_slug_from_the_tex)
{
	namespace ast = jimd::ast;
	jimd::html::math_table const table{{{jimd::html::math_mode::inline_math, "E=mc^2"}, "<math/>"}};
	EXPECT_EQ(render_with(table, {ast::heading{.level = 2,
	                                           .children = {ast::text{"Energy "sv},
	                                                        ast::math{"E=mc^2"sv}}}}),
	          "<h2 id=\"energy-e-mc-2\"><a href=\"#energy-e-mc-2\">Energy <math/></a></h2>\n");
}

TEST(html_render, math_without_an_entry_fails_the_render_at_its_tex)
{
	namespace ast = jimd::ast;
	using jimd::html::math_mode;
	std::string_view const source = "text\na $x$ b"sv;
	auto const tex = source.substr(source.find("$x$") + 1, 1);
	ast::document doc;
	doc.source = source;
	doc.blocks = {paragraph_of(ast::math{tex})};
	auto const expected = std::unexpected(jimd::error{
		.code = jimd::errc::unrendered_math,
		.line = 2,
		.column = 4,
	});

	EXPECT_EQ(jimd::html::render(doc), expected);
	jimd::html::math_table table;
	EXPECT_EQ(jimd::html::render(doc, {.math = &table}), expected);
	table.insert({{math_mode::inline_math, "y"}, "<math/>"});
	table.insert({{math_mode::display, "x"}, "<math/>"});
	EXPECT_EQ(jimd::html::render(doc, {.math = &table}), expected);
}

TEST(html_render, math_without_an_entry_in_a_footnote_or_a_list_fails_the_render)
{
	namespace ast = jimd::ast;
	for (auto const& block : {
			 ast::block{ast::list{.items = {{ast::math{"x"sv}}}}},
			 ast::block{paragraph_of(
				 ast::reference{.key = "k"sv, .content = ast::inlines{ast::math{"x"sv}}})},
		 }) {
		ast::document doc;
		doc.blocks = {block};
		auto const html = jimd::html::render(doc);
		ASSERT_FALSE(html.has_value());
		EXPECT_EQ(html.error().code, jimd::errc::unrendered_math);
	}
}

TEST(html_render, display_math_writes_its_table_entry_as_a_block)
{
	namespace ast = jimd::ast;
	jimd::html::math_table const table{
		{{jimd::html::math_mode::display, "a<b\n"}, {"<math display=\"block\">&lt;</math>"}}};
	EXPECT_EQ(render_with(table, {paragraph_of(ast::text{"before"sv}), ast::display_math{"a<b\n"sv},
	                              paragraph_of(ast::text{"after"sv})}),
	          "<p>before</p>\n<math display=\"block\">&lt;</math>\n<p>after</p>\n");
}

TEST(html_render, display_math_looks_up_its_tex_with_each_crlf_turned_into_lf)
{
	namespace ast = jimd::ast;
	jimd::html::math_table const table{{{jimd::html::math_mode::display, "a\nb\r\n"}, "<math/>"},
	                                   {{jimd::html::math_mode::display, "a\nb\n"}, "<math lf/>"}};
	EXPECT_EQ(render_with(table, {ast::display_math{"a\r\nb\r\n"sv}}), "<math lf/>\n");
}

TEST(html_render, display_and_inline_entries_are_separate)
{
	namespace ast = jimd::ast;
	using jimd::html::math_mode;
	jimd::html::math_table const table{{{math_mode::inline_math, "x\n"}, "<math inline/>"},
	                                   {{math_mode::display, "x"}, "<math display/>"}};
	for (auto const& block : {
			 ast::block{ast::display_math{"x\n"sv}},
			 ast::block{paragraph_of(ast::math{"x"sv})},
		 }) {
		ast::document doc;
		doc.blocks = {block};
		auto const html = jimd::html::render(doc, {.math = &table});
		ASSERT_FALSE(html.has_value());
		EXPECT_EQ(html.error().code, jimd::errc::unrendered_math);
	}
}

TEST(html_render, display_math_without_an_entry_fails_the_render_at_its_body)
{
	namespace ast = jimd::ast;
	std::string_view const source = "text\n$$\nx\n$$\n"sv;
	ast::document doc;
	doc.source = source;
	doc.blocks = {ast::display_math{source.substr(source.find("x\n"), 2)}};
	auto const expected = std::unexpected(jimd::error{
		.code = jimd::errc::unrendered_math,
		.line = 3,
		.column = 1,
	});

	EXPECT_EQ(jimd::html::render(doc), expected);
	jimd::html::math_table table;
	EXPECT_EQ(jimd::html::render(doc, {.math = &table}), expected);
	table.insert({{jimd::html::math_mode::display, "y\n"}, "<math/>"});
	EXPECT_EQ(jimd::html::render(doc, {.math = &table}), expected);
}

TEST(html_render, display_math_in_a_quote)
{
	namespace ast = jimd::ast;
	jimd::html::math_table const table{{{jimd::html::math_mode::display, "x\n"}, "<math/>"}};
	EXPECT_EQ(render_with(table, {ast::quote{.children = {ast::display_math{"x\n"sv}}}}),
	          "<blockquote>\n<math/>\n</blockquote>\n");
}

TEST(html_render, aligned_math_writes_its_table_entry_as_a_block)
{
	namespace ast = jimd::ast;
	jimd::html::math_table const table{
		{{jimd::html::math_mode::display, "\\begin{align*}\na<b\n\\\\\nc\n\\end{align*}"},
		 {"<math display=\"block\">&lt;</math>"}}};
	EXPECT_EQ(render_with(table, {paragraph_of(ast::text{"before"sv}),
	                              ast::aligned_math{.rows = {"a<b"sv, "c"sv}},
	                              paragraph_of(ast::text{"after"sv})}),
	          "<p>before</p>\n<math display=\"block\">&lt;</math>\n<p>after</p>\n");
}

TEST(html_render, aligned_math_without_an_entry_fails_the_render_at_its_first_row)
{
	namespace ast = jimd::ast;
	std::string_view const source = "text\n$$$\n\na\nb\n$$$\n"sv;
	ast::document doc;
	doc.source = source;
	doc.blocks = {ast::aligned_math{
		.rows = {source.substr(source.find("a\n"), 1), source.substr(source.find("b\n"), 1)}}};
	auto const expected = std::unexpected(jimd::error{
		.code = jimd::errc::unrendered_math,
		.line = 4,
		.column = 1,
	});

	EXPECT_EQ(jimd::html::render(doc), expected);
	jimd::html::math_table table{{{jimd::html::math_mode::display, "a\nb\n"}, "<math/>"}};
	EXPECT_EQ(jimd::html::render(doc, {.math = &table}), expected);
}

TEST(html_render, code_span)
{
	namespace ast = jimd::ast;
	EXPECT_EQ(render_blocks({ast::paragraph{.children = {ast::code_span{"a < *b*"sv}}}}),
	          "<p><code>a &lt; *b*</code></p>\n");
}

TEST(html_render, emphasis)
{
	namespace ast = jimd::ast;
	EXPECT_EQ(
		render_blocks({ast::paragraph{.children =
		                                  {
											  ast::text{"a "sv},
											  ast::emphasis{.children = {ast::text{"b & c"sv}}},
											  ast::text{" d"sv},
										  }}}),
		"<p>a <em>b &amp; c</em> d</p>\n");
}

TEST(html_render, strong)
{
	namespace ast = jimd::ast;
	EXPECT_EQ(render_blocks({ast::paragraph{.children =
	                                            {
													ast::strong{.children = {ast::text{"bold"sv}}},
												}}}),
	          "<p><strong>bold</strong></p>\n");
}

TEST(html_render, emphasis_and_strong_nest)
{
	namespace ast = jimd::ast;
	EXPECT_EQ(render_blocks({ast::paragraph{
				  .children =
					  {
						  ast::emphasis{.children =
						                    {
												ast::text{"a "sv},
												ast::strong{.children = {ast::text{"b"sv}}},
												ast::emphasis{.children = {ast::code_span{"c"sv}}},
											}},
					  }}}),
	          "<p><em>a <strong>b</strong><em><code>c</code></em></em></p>\n");
}

TEST(html_render, empty_emphasis_and_soft_break_inside_strong)
{
	namespace ast = jimd::ast;
	EXPECT_EQ(render_blocks({ast::paragraph{
				  .children =
					  {
						  ast::emphasis{},
						  ast::strong{
							  .children = {ast::text{"x"sv}, ast::soft_break{}, ast::text{"y"sv}}},
					  }}}),
	          "<p><em></em><strong>x\ny</strong></p>\n");
}

TEST(html_render, heading_levels)
{
	EXPECT_EQ(render_blocks({heading_of(1, "A"sv)}), "<h1 id=\"a\"><a href=\"#a\">A</a></h1>\n");
	EXPECT_EQ(render_blocks({heading_of(3, "A"sv)}), "<h3 id=\"a\"><a href=\"#a\">A</a></h3>\n");
	EXPECT_EQ(render_blocks({heading_of(6, "A"sv)}), "<h6 id=\"a\"><a href=\"#a\">A</a></h6>\n");
}

TEST(html_render, heading_level_is_clamped)
{
	EXPECT_EQ(render_blocks({heading_of(7, "A"sv)}), "<h6 id=\"a\"><a href=\"#a\">A</a></h6>\n");
	EXPECT_EQ(render_blocks({heading_of(100, "A"sv)}), "<h6 id=\"a\"><a href=\"#a\">A</a></h6>\n");
	EXPECT_EQ(render_blocks({heading_of(0, "A"sv)}), "<h1 id=\"a\"><a href=\"#a\">A</a></h1>\n");
}

TEST(html_render, heading_content_is_escaped_and_slug_comes_from_plain_text)
{
	namespace ast = jimd::ast;
	EXPECT_EQ(
		render_blocks({ast::heading{
			.level = 2,
			.children = {ast::emphasis{.children = {ast::text{"A & B"sv}}}, ast::text{" <c>"sv}},
		}}),
		"<h2 id=\"a-b-c\"><a href=\"#a-b-c\"><em>A &amp; B</em> &lt;c&gt;</a></h2>\n");
}

TEST(html_render, a_heading_is_wrapped_in_a_link_to_its_own_id)
{
	namespace ast = jimd::ast;
	EXPECT_EQ(render_blocks({ast::heading{
				  .level = 2,
				  .children = {ast::text{"Plain "sv}, ast::emphasis{.children = {ast::text{"a"sv}}},
				               ast::text{" "sv}, ast::strong{.children = {ast::text{"b"sv}}},
				               ast::text{" "sv}, ast::code_span{"c"sv}}}}),
	          "<h2 id=\"plain-a-b-c\"><a href=\"#plain-a-b-c\">Plain <em>a</em> <strong>b</strong> "
	          "<code>c</code></a></h2>\n");
}

TEST(html_render, a_heading_with_an_image_or_maths_is_still_wrapped)
{
	namespace ast = jimd::ast;
	jimd::html::math_table const table{{{jimd::html::math_mode::inline_math, "x"}, "<math/>"}};
	ast::document doc;
	doc.defs.images.emplace("pic"sv, ast::image_def{.target = "pic.png"sv, .width = std::nullopt});
	doc.blocks = {ast::heading{.level = 1,
	                           .children = {ast::image{.key = "pic"sv,
	                                                   .alt = "pic"sv,
	                                                   .target = std::nullopt,
	                                                   .width = std::nullopt},
	                                        ast::math{"x"sv}}}};
	EXPECT_EQ(
		jimd::html::render(doc, {.math = &table}),
		"<h1 id=\"picx\"><a href=\"#picx\"><img src=\"pic.png\" alt=\"pic\"><math/></a></h1>\n");
}

TEST(html_render, a_numbered_heading_id_is_the_target_of_its_link)
{
	EXPECT_EQ(render_blocks({heading_of(1, "Same"sv), heading_of(1, "Same"sv)}),
	          "<h1 id=\"same\"><a href=\"#same\">Same</a></h1>\n"
	          "<h1 id=\"same-2\"><a href=\"#same-2\">Same</a></h1>\n");
}

TEST(html_render, a_heading_with_a_link_in_any_inline_keeps_the_plain_form)
{
	namespace ast = jimd::ast;
	auto const anchor = ast::link{.key = "k"sv, .text = "Go"sv, .target = "/go"sv};
	auto const heading_of_node = [](ast::inline_node node) -> std::string {
		return render_blocks({ast::heading{.level = 1, .children = {std::move(node)}}});
	};
	EXPECT_EQ(heading_of_node(anchor), "<h1 id=\"go\"><a href=\"/go\">Go</a></h1>\n");
	EXPECT_EQ(heading_of_node(ast::emphasis{.children = {anchor}}),
	          "<h1 id=\"go\"><em><a href=\"/go\">Go</a></em></h1>\n");
	EXPECT_EQ(heading_of_node(ast::strong{.children = {ast::emphasis{.children = {anchor}}}}),
	          "<h1 id=\"go\"><strong><em><a href=\"/go\">Go</a></em></strong></h1>\n");
	EXPECT_EQ(
		render_blocks({heading_of(1, "Go"sv), ast::heading{.level = 1, .children = {anchor}}}),
		"<h1 id=\"go\"><a href=\"#go\">Go</a></h1>\n"
		"<h1 id=\"go-2\"><a href=\"/go\">Go</a></h1>\n");
}

TEST(html_render, a_heading_with_a_note_reference_keeps_the_plain_form)
{
	namespace ast = jimd::ast;
	auto const note = ast::reference{.key = "k"sv, .content = ast::inlines{ast::text{"n"sv}}};
	EXPECT_EQ(render_blocks({ast::heading{.level = 2, .children = {ast::text{"A"sv}, note}}}),
	          "<h2 id=\"a\">A" + note_ref(1) + "</h2>\n" + note_aside(1, "n"));
	EXPECT_EQ(render_blocks({ast::heading{
				  .level = 2, .children = {ast::emphasis{.children = {ast::text{"A"sv}, note}}}}}),
	          "<h2 id=\"a\"><em>A" + note_ref(1) + "</em></h2>\n" + note_aside(1, "n"));
	EXPECT_EQ(render_blocks({ast::heading{
				  .level = 2, .children = {ast::strong{.children = {ast::text{"A"sv}, note}}}}}),
	          "<h2 id=\"a\"><strong>A" + note_ref(1) + "</strong></h2>\n" + note_aside(1, "n"));
	ast::document doc;
	doc.defs.footnotes.emplace("k"sv, ast::inlines{ast::text{"n"sv}});
	auto const bare = ast::reference{.key = "k"sv, .content = std::nullopt};
	doc.blocks = {paragraph_of(bare),
	              ast::heading{.level = 2, .children = {ast::text{"A"sv}, bare}}};
	EXPECT_EQ(jimd::html::render(doc), "<p>" + note_ref(1) + "</p>\n" + note_aside(1, "n") +
	                                       "<h2 id=\"a\">A" + note_link(1) + "</h2>\n");
}

TEST(html_render, a_heading_with_an_apostrophe_has_a_slug_without_it)
{
	EXPECT_EQ(render_blocks({heading_of(1, "Let's go"sv), heading_of(1, "Lets go"sv)}),
	          "<h1 id=\"lets-go\"><a href=\"#lets-go\">Let’s go</a></h1>\n"
	          "<h1 id=\"lets-go-2\"><a href=\"#lets-go-2\">Lets go</a></h1>\n");
}

TEST(html_render, duplicate_headings_get_numbered_ids)
{
	EXPECT_EQ(render_blocks({heading_of(1, "Same"sv), heading_of(2, "Same"sv),
	                         heading_of(1, "same!"sv), heading_of(1, "Other"sv)}),
	          "<h1 id=\"same\"><a href=\"#same\">Same</a></h1>\n"
	          "<h2 id=\"same-2\"><a href=\"#same-2\">Same</a></h2>\n"
	          "<h1 id=\"same-3\"><a href=\"#same-3\">same!</a></h1>\n"
	          "<h1 id=\"other\"><a href=\"#other\">Other</a></h1>\n");
}

TEST(html_render, numbered_id_skips_ids_already_taken)
{
	EXPECT_EQ(render_blocks({heading_of(1, "a-2"sv), heading_of(1, "a"sv), heading_of(1, "a"sv)}),
	          "<h1 id=\"a-2\"><a href=\"#a-2\">a-2</a></h1>\n"
	          "<h1 id=\"a\"><a href=\"#a\">a</a></h1>\n"
	          "<h1 id=\"a-3\"><a href=\"#a-3\">a</a></h1>\n");
}

TEST(html_render, empty_heading_gets_section_ids)
{
	namespace ast = jimd::ast;
	EXPECT_EQ(render_blocks({ast::heading{.level = 1, .children = {}}, heading_of(1, "?!"sv)}),
	          "<h1 id=\"section\"><a href=\"#section\"></a></h1>\n"
	          "<h1 id=\"section-2\"><a href=\"#section-2\">?!</a></h1>\n");
}

TEST(html_render, headings_inside_a_quote_share_the_id_space)
{
	namespace ast = jimd::ast;
	EXPECT_EQ(render_blocks({heading_of(1, "T"sv), ast::quote{.children = {heading_of(1, "T"sv)}}}),
	          "<h1 id=\"t\"><a href=\"#t\">T</a></h1>\n"
	          "<blockquote>\n<h1 id=\"t-2\"><a href=\"#t-2\">T</a></h1>\n</blockquote>\n");
}

TEST(html_slugify, lowercases_letters_and_keeps_digits)
{
	EXPECT_EQ(slug_of("Hello World 42"sv), "hello-world-42");
}

TEST(html_slugify, only_ascii_letters_and_digits_are_kept)
{
	EXPECT_EQ(slug_of("AZaz09"sv), "azaz09");
	EXPECT_EQ(slug_of("@a[b`c{d/0:"sv), "a-b-c-d-0");
}

TEST(html_slugify, punctuation_runs_become_one_dash)
{
	EXPECT_EQ(slug_of("Hello,   World!"sv), "hello-world");
	EXPECT_EQ(slug_of("a - b -- c"sv), "a-b-c");
	EXPECT_EQ(slug_of("snake_case.name"sv), "snake-case-name");
}

TEST(html_slugify, leading_and_trailing_punctuation_is_dropped)
{
	EXPECT_EQ(slug_of("--a--"sv), "a");
	EXPECT_EQ(slug_of("  (x)  "sv), "x");
	EXPECT_EQ(slug_of("C++"sv), "c");
}

TEST(html_slugify, non_ascii_is_a_separator)
{
	EXPECT_EQ(slug_of("caf\xC3\xA9 au lait"sv), "caf-au-lait");
	EXPECT_EQ(slug_of("\xC3\xA9"sv), "section");
}

TEST(html_slugify, an_apostrophe_is_dropped_and_does_not_split_the_word)
{
	EXPECT_EQ(slug_of("Let's go"sv), "lets-go");
	EXPECT_EQ(slug_of("Let\xE2\x80\x99s go"sv), "lets-go");
	EXPECT_EQ(slug_of("don't won\xE2\x80\x99t"sv), "dont-wont");
	EXPECT_EQ(slug_of("rock 'n' roll"sv), "rock-n-roll");
	EXPECT_EQ(slug_of("rock \xE2\x80\x99n\xE2\x80\x99 roll"sv), "rock-n-roll");
}

TEST(html_slugify, an_apostrophe_alone_leaves_nothing)
{
	EXPECT_EQ(slug_of("'"sv), "section");
	EXPECT_EQ(slug_of("\xE2\x80\x99"sv), "section");
	EXPECT_EQ(slug_of("'' \xE2\x80\x99 '"sv), "section");
}

TEST(html_slugify, an_apostrophe_at_either_end_or_beside_a_separator_changes_nothing)
{
	EXPECT_EQ(slug_of("'tis"sv), "tis");
	EXPECT_EQ(slug_of("dogs'"sv), "dogs");
	EXPECT_EQ(slug_of("'quoted'"sv), "quoted");
	EXPECT_EQ(slug_of("a ' b"sv), "a-b");
	EXPECT_EQ(slug_of("a' b"sv), "a-b");
	EXPECT_EQ(slug_of("a '-' b"sv), "a-b");
	EXPECT_EQ(slug_of("a''b"sv), "ab");
}

TEST(html_slugify, other_quote_marks_stay_separators)
{
	EXPECT_EQ(slug_of("a\xE2\x80\x98"
	                  "b"sv),
	          "a-b");
	EXPECT_EQ(slug_of("\xE2\x80\x98hi\xE2\x80\x99"sv), "hi");
	EXPECT_EQ(slug_of("a\xE2\x80\x9C"
	                  "b\xE2\x80\x9D"sv),
	          "a-b");
	EXPECT_EQ(slug_of("say \"hi\" now"sv), "say-hi-now");
}

TEST(html_slugify, part_of_an_apostrophe_is_not_one)
{
	EXPECT_EQ(slug_of("a\xE2"
	                  "b"sv),
	          "a-b");
	EXPECT_EQ(slug_of("a\xE2\x80"
	                  "b"sv),
	          "a-b");
	EXPECT_EQ(slug_of("a\xE2"sv), "a");
	EXPECT_EQ(slug_of("a\xE2\x80"sv), "a");
	EXPECT_EQ(slug_of("a\x80\x99"
	                  "b"sv),
	          "a-b");
	EXPECT_EQ(slug_of("a\xE2\x99"
	                  "b"sv),
	          "a-b");
	EXPECT_EQ(slug_of("a\xE2\x80'"
	                  "b"sv),
	          "a-b");
}

TEST(html_slugify, an_apostrophe_inside_emphasis_is_dropped)
{
	namespace ast = jimd::ast;
	EXPECT_EQ(jimd::html::slugify({ast::text{"Let"sv},
	                               ast::emphasis{.children = {ast::text{"'s"sv}}},
	                               ast::text{" go"sv}}),
	          "lets-go");
}

TEST(html_slugify, empty_slug_is_section)
{
	EXPECT_EQ(jimd::html::slugify({}), "section");
	EXPECT_EQ(slug_of(""sv), "section");
	EXPECT_EQ(slug_of("!?"sv), "section");
}

TEST(html_slugify, uses_plain_text_of_formatting)
{
	namespace ast = jimd::ast;
	EXPECT_EQ(jimd::html::slugify({ast::text{"One "sv},
	                               ast::strong{.children = {ast::text{"Two"sv}}}, ast::soft_break{},
	                               ast::code_span{"three"sv}}),
	          "one-two-three");
}

TEST(html_render, link_with_inline_target)
{
	namespace ast = jimd::ast;
	EXPECT_EQ(render_blocks({paragraph_of(ast::link{
				  .key = "site"sv, .text = "my site"sv, .target = "https://example.com/"sv})}),
	          "<p><a href=\"https://example.com/\">my site</a></p>\n");
}

TEST(html_render, link_with_defined_target)
{
	namespace ast = jimd::ast;
	ast::document doc;
	doc.defs.links.emplace("site"sv, "https://example.com/"sv);
	doc.blocks = {
		paragraph_of(ast::link{.key = "site"sv, .text = "site"sv, .target = std::nullopt})};
	EXPECT_EQ(jimd::html::render(doc), "<p><a href=\"https://example.com/\">site</a></p>\n"s);
}

TEST(html_render, inline_target_wins_over_a_definition)
{
	namespace ast = jimd::ast;
	ast::document doc;
	doc.defs.links.emplace("site"sv, "https://defined.example/"sv);
	doc.blocks = {
		paragraph_of(ast::link{.key = "site"sv, .text = "site"sv, .target = "/inline"sv})};
	EXPECT_EQ(jimd::html::render(doc), "<p><a href=\"/inline\">site</a></p>\n"s);
}

TEST(html_render, link_text_and_target_are_escaped)
{
	namespace ast = jimd::ast;
	EXPECT_EQ(render_blocks({paragraph_of(
				  ast::link{.key = "k"sv, .text = "a < b"sv, .target = "/?a=1&b=\"2\"<>"sv})}),
	          "<p><a href=\"/?a=1&amp;b=&quot;2&quot;&lt;&gt;\">a &lt; b</a></p>\n");
}

TEST(html_render, image_with_inline_target)
{
	namespace ast = jimd::ast;
	EXPECT_EQ(
		render_blocks({paragraph_of(ast::image{
			.key = "logo"sv, .alt = "the logo"sv, .target = "/logo.png"sv, .width = std::nullopt})}),
		"<p><img src=\"/logo.png\" alt=\"the logo\"></p>\n");
}

TEST(html_render, image_with_defined_target)
{
	namespace ast = jimd::ast;
	ast::document doc;
	doc.defs.images.emplace("logo"sv,
	                        ast::image_def{.target = "/logo.png"sv, .width = std::nullopt});
	doc.blocks = {paragraph_of(ast::image{
		.key = "logo"sv, .alt = "logo"sv, .target = std::nullopt, .width = std::nullopt})};
	EXPECT_EQ(jimd::html::render(doc), "<p><img src=\"/logo.png\" alt=\"logo\"></p>\n"s);
}

TEST(html_render, image_with_a_width_writes_a_style)
{
	namespace ast = jimd::ast;
	EXPECT_EQ(render_blocks({paragraph_of(ast::image{
				  .key = "a"sv, .alt = "pic"sv, .target = "/p.png"sv, .width = "75%"sv})}),
	          "<p><img src=\"/p.png\" alt=\"pic\" style=\"width: 75%\"></p>\n");
}

TEST(html_render, the_width_is_written_as_it_is_and_escaped)
{
	namespace ast = jimd::ast;
	EXPECT_EQ(
		render_blocks({paragraph_of(ast::image{
						   .key = "a"sv, .alt = "a"sv, .target = "/p.png"sv, .width = "075%"sv}),
		               paragraph_of(ast::image{
						   .key = "b"sv, .alt = "b"sv, .target = "/p.png"sv, .width = "\"<x"sv})}),
		"<p><img src=\"/p.png\" alt=\"a\" style=\"width: 075%\"></p>\n"
		"<p><img src=\"/p.png\" alt=\"b\" style=\"width: &quot;&lt;x\"></p>\n");
}

TEST(html_render, a_reference_to_a_defined_image_gets_the_definitions_width)
{
	namespace ast = jimd::ast;
	ast::document doc;
	doc.defs.images.emplace("a"sv, ast::image_def{.target = "/a.png"sv, .width = "50%"sv});
	doc.defs.images.emplace("b"sv, ast::image_def{.target = "/b.png"sv, .width = std::nullopt});
	doc.blocks = {paragraph_of(ast::image{
					  .key = "a"sv, .alt = "a"sv, .target = std::nullopt, .width = std::nullopt}),
	              paragraph_of(ast::image{
					  .key = "b"sv, .alt = "b"sv, .target = std::nullopt, .width = std::nullopt})};
	EXPECT_EQ(jimd::html::render(doc),
	          "<p><img src=\"/a.png\" alt=\"a\" style=\"width: 50%\"></p>\n"
	          "<p><img src=\"/b.png\" alt=\"b\"></p>\n");
}

TEST(html_render, an_inline_target_brings_its_own_width_over_a_definitions)
{
	namespace ast = jimd::ast;
	ast::document doc;
	doc.defs.images.emplace("a"sv, ast::image_def{.target = "/def.png"sv, .width = "50%"sv});
	doc.blocks = {paragraph_of(ast::image{
					  .key = "a"sv, .alt = "a"sv, .target = "/in.png"sv, .width = "25%"sv}),
	              paragraph_of(ast::image{
					  .key = "a"sv, .alt = "a"sv, .target = "/in.png"sv, .width = std::nullopt})};
	EXPECT_EQ(jimd::html::render(doc),
	          "<p><img src=\"/in.png\" alt=\"a\" style=\"width: 25%\"></p>\n"
	          "<p><img src=\"/in.png\" alt=\"a\"></p>\n");
}

TEST(html_render, image_alt_and_target_are_escaped)
{
	namespace ast = jimd::ast;
	EXPECT_EQ(
		render_blocks({paragraph_of(ast::image{
			.key = "k"sv, .alt = "\"x\" & <y>"sv, .target = "/a\"b"sv, .width = std::nullopt})}),
		"<p><img src=\"/a&quot;b\" alt=\"&quot;x&quot; &amp; &lt;y&gt;\"></p>\n");
}

TEST(html_render, unresolved_link_on_a_hand_built_tree)
{
	namespace ast = jimd::ast;
	ast::document doc;
	doc.blocks = {
		paragraph_of(ast::link{.key = "gone"sv, .text = "gone"sv, .target = std::nullopt})};
	EXPECT_EQ(jimd::html::render(doc),
	          std::unexpected(jimd::error{.code = jimd::errc::undefined_reference}));
}

TEST(html_render, unresolved_image_on_a_hand_built_tree)
{
	namespace ast = jimd::ast;
	ast::document doc;
	doc.blocks = {paragraph_of(ast::image{
		.key = "gone"sv, .alt = "gone"sv, .target = std::nullopt, .width = std::nullopt})};
	EXPECT_EQ(jimd::html::render(doc),
	          std::unexpected(jimd::error{.code = jimd::errc::undefined_reference}));
}

TEST(html_render, links_and_images_are_defined_separately)
{
	namespace ast = jimd::ast;
	ast::document doc;
	doc.defs.links.emplace("a"sv, "/a"sv);
	doc.defs.images.emplace("b"sv, ast::image_def{.target = "/b.png"sv, .width = std::nullopt});
	doc.blocks = {paragraph_of(
		ast::image{.key = "a"sv, .alt = "a"sv, .target = std::nullopt, .width = std::nullopt})};
	EXPECT_EQ(jimd::html::render(doc),
	          std::unexpected(jimd::error{.code = jimd::errc::undefined_reference}));
	doc.blocks = {paragraph_of(ast::link{.key = "b"sv, .text = "b"sv, .target = std::nullopt})};
	EXPECT_EQ(jimd::html::render(doc),
	          std::unexpected(jimd::error{.code = jimd::errc::undefined_reference}));
}

TEST(html_render, unresolved_key_reports_its_position_in_the_source)
{
	namespace ast = jimd::ast;
	std::string_view const source = "line one\nsee [@missing] here"sv;
	std::string_view const key = "missing"sv;
	ast::document doc;
	doc.source = source;
	doc.blocks = {paragraph_of(ast::link{
		.key = source.substr(source.find(key), key.size()), .text = key, .target = std::nullopt})};
	EXPECT_EQ(jimd::html::render(doc), std::unexpected(jimd::error{
										   .code = jimd::errc::undefined_reference,
										   .line = 2,
										   .column = 7,
									   }));
}

TEST(html_render, unresolved_key_nested_in_other_nodes_fails_the_render)
{
	namespace ast = jimd::ast;
	ast::document doc;
	doc.blocks = {
		paragraph_of(ast::text{"fine"sv}),
		ast::quote{
			.children = {ast::paragraph{.children = {ast::emphasis{.children = {ast::link{
																	   .key = "gone"sv,
																	   .text = "gone"sv,
																	   .target = std::nullopt,
																   }}}}}}},
	};
	EXPECT_EQ(jimd::html::render(doc),
	          std::unexpected(jimd::error{.code = jimd::errc::undefined_reference}));
}

TEST(html_render, link_text_in_a_heading_feeds_the_slug)
{
	namespace ast = jimd::ast;
	EXPECT_EQ(
		render_blocks({ast::heading{
			.level = 1, .children = {ast::link{.key = "k"sv, .text = "Go"sv, .target = "/go"sv}}}}),
		"<h1 id=\"go\"><a href=\"/go\">Go</a></h1>\n");
}

TEST(html_render, footnote_with_inline_content)
{
	namespace ast = jimd::ast;
	auto const expected = "<p>x" + note_ref(1) + "</p>\n" + note_aside(1, "a &lt; b");
	EXPECT_EQ(
		render_blocks({ast::paragraph{
			.children =
				{
					ast::text{"x"sv},
					ast::reference{.key = "k"sv, .content = ast::inlines{ast::text{"a < b"sv}}},
				}}}),
		expected);
}

TEST(html_render, footnote_with_defined_content)
{
	namespace ast = jimd::ast;
	ast::document doc;
	doc.defs.footnotes.emplace(
		"k"sv, ast::inlines{ast::text{"see "sv}, ast::emphasis{.children = {ast::text{"this"sv}}}});
	doc.blocks = {paragraph_of(ast::reference{.key = "k"sv, .content = std::nullopt})};
	EXPECT_EQ(jimd::html::render(doc),
	          "<p>" + note_ref(1) + "</p>\n" + note_aside(1, "see <em>this</em>"));
}

TEST(html_render, inline_footnote_content_wins_over_a_definition)
{
	namespace ast = jimd::ast;
	ast::document doc;
	doc.defs.footnotes.emplace("k"sv, ast::inlines{ast::text{"defined"sv}});
	doc.blocks = {
		paragraph_of(ast::reference{.key = "k"sv, .content = ast::inlines{ast::text{"inline"sv}}})};
	EXPECT_EQ(jimd::html::render(doc), "<p>" + note_ref(1) + "</p>\n" + note_aside(1, "inline"));
}

TEST(html_render, repeated_footnote_links_to_the_first_note)
{
	namespace ast = jimd::ast;
	ast::document doc;
	doc.defs.footnotes.emplace("k"sv, ast::inlines{ast::text{"note"sv}});
	doc.blocks = {
		paragraph_of(ast::reference{.key = "k"sv, .content = std::nullopt}),
		paragraph_of(ast::reference{.key = "k"sv, .content = std::nullopt}),
	};
	EXPECT_EQ(jimd::html::render(doc), "<p>" + note_ref(1) + "</p>\n" + note_aside(1, "note") +
	                                       "<p>" + note_link(1) + "</p>\n");
}

TEST(html_render, footnotes_are_numbered_by_first_use)
{
	namespace ast = jimd::ast;
	ast::document doc;
	doc.defs.footnotes.emplace("a"sv, ast::inlines{ast::text{"A"sv}});
	doc.defs.footnotes.emplace("b"sv, ast::inlines{ast::text{"B"sv}});
	doc.blocks = {paragraph_of(ast::reference{.key = "b"sv, .content = std::nullopt}),
	              paragraph_of(ast::reference{.key = "a"sv, .content = std::nullopt}),
	              paragraph_of(ast::reference{.key = "b"sv, .content = std::nullopt})};
	EXPECT_EQ(jimd::html::render(doc), "<p>" + note_ref(1) + "</p>\n" + note_aside(1, "B") + "<p>" +
	                                       note_ref(2) + "</p>\n" + note_aside(2, "A") + "<p>" +
	                                       note_link(1) + "</p>\n");
}

TEST(html_render, unreferenced_footnote_definitions_are_not_rendered)
{
	namespace ast = jimd::ast;
	ast::document doc;
	doc.defs.footnotes.emplace("unused"sv, ast::inlines{ast::text{"never shown"sv}});
	doc.blocks = {paragraph_of(ast::text{"body"sv})};
	EXPECT_EQ(jimd::html::render(doc), "<p>body</p>\n"s);
}

TEST(html_render, unresolved_footnote)
{
	namespace ast = jimd::ast;
	ast::document doc;
	doc.blocks = {paragraph_of(ast::reference{.key = "gone"sv, .content = std::nullopt})};
	EXPECT_EQ(jimd::html::render(doc),
	          std::unexpected(jimd::error{.code = jimd::errc::undefined_reference}));
}

TEST(html_render, unresolved_footnote_reports_its_position_in_the_source)
{
	namespace ast = jimd::ast;
	std::string_view const source = "text\n[^gone] here"sv;
	std::string_view const key = "gone"sv;
	ast::document doc;
	doc.source = source;
	doc.blocks = {paragraph_of(ast::reference{.key = source.substr(source.find(key), key.size()),
	                                          .content = std::nullopt})};
	EXPECT_EQ(jimd::html::render(doc), std::unexpected(jimd::error{
										   .code = jimd::errc::undefined_reference,
										   .line = 2,
										   .column = 3,
									   }));
}

TEST(html_render, footnote_note_is_escaped_and_may_hold_a_link)
{
	namespace ast = jimd::ast;
	EXPECT_EQ(
		render_blocks({paragraph_of(ast::reference{
			.key = "k"sv,
			.content = ast::inlines{ast::text{"<&>"sv},
			                        ast::link{.key = "l"sv, .text = "t"sv, .target = "/u"sv}}})}),
		"<p>" + note_ref(1) + "</p>\n" + note_aside(1, "&lt;&amp;&gt;<a href=\"/u\">t</a>"));
}

TEST(html_render, footnote_in_a_heading_is_not_part_of_the_slug)
{
	namespace ast = jimd::ast;
	EXPECT_EQ(render_blocks({ast::heading{
				  .level = 2,
				  .children = {ast::text{"Title"sv},
				               ast::reference{.key = "k"sv,
				                              .content = ast::inlines{ast::text{"n"sv}}}}}}),
	          "<h2 id=\"title\">Title" + note_ref(1) + "</h2>\n" + note_aside(1, "n"));
}

TEST(html_render, the_notes_of_a_block_follow_it_in_the_order_of_their_first_use)
{
	namespace ast = jimd::ast;
	EXPECT_EQ(
		render_blocks({ast::paragraph{
			.children = {ast::text{"a"sv},
			             ast::reference{.key = "x"sv, .content = ast::inlines{ast::text{"X"sv}}},
			             ast::text{"b"sv},
			             ast::reference{.key = "y"sv, .content = ast::inlines{ast::text{"Y"sv}}}}}}),
		"<p>a" + note_ref(1) + "b" + note_ref(2) + "</p>\n" + note_aside(1, "X") +
			note_aside(2, "Y"));
}

TEST(html_render, each_block_is_followed_by_its_own_notes)
{
	namespace ast = jimd::ast;
	EXPECT_EQ(render_blocks({paragraph_with_note("a"sv, "x"sv, "X"sv),
	                         paragraph_of(ast::text{"b"sv}),
	                         paragraph_with_note("c"sv, "y"sv, "Y"sv)}),
	          "<p>a" + note_ref(1) + "</p>\n" + note_aside(1, "X") + "<p>b</p>\n<p>c" +
	              note_ref(2) + "</p>\n" + note_aside(2, "Y"));
}

TEST(html_render, a_note_in_a_list_item_follows_the_list)
{
	namespace ast = jimd::ast;
	EXPECT_EQ(render_blocks(
				  {ast::list{.items = {{ast::text{"one"sv},
				                        ast::reference{.key = "x"sv,
				                                       .content = ast::inlines{ast::text{"X"sv}}}},
				                       {ast::text{"two"sv}}}},
				   paragraph_of(ast::text{"after"sv})}),
	          "<ul>\n<li>one" + note_ref(1) + "</li>\n<li>two</li>\n</ul>\n" + note_aside(1, "X") +
	              "<p>after</p>\n");
}

TEST(html_render, a_note_in_a_quote_follows_the_whole_quote)
{
	namespace ast = jimd::ast;
	EXPECT_EQ(render_blocks({ast::quote{
				  .children = {ast::quote{.children = {paragraph_with_note("a"sv, "x"sv, "X"sv)}},
				               paragraph_with_note("b"sv, "y"sv, "Y"sv)}}}),
	          "<blockquote>\n<blockquote>\n<p>a" + note_ref(1) + "</p>\n</blockquote>\n<p>b" +
	              note_ref(2) + "</p>\n</blockquote>\n" + note_aside(1, "X") + note_aside(2, "Y"));
}

TEST(html_render, a_note_in_a_list_in_a_quote_follows_the_quote)
{
	namespace ast = jimd::ast;
	EXPECT_EQ(render_blocks({ast::quote{
				  .children = {ast::list{
					  .items = {{ast::reference{.key = "x"sv,
					                            .content = ast::inlines{ast::text{"X"sv}}}}}}}}}),
	          "<blockquote>\n<ul>\n<li>" + note_ref(1) + "</li>\n</ul>\n</blockquote>\n" +
	              note_aside(1, "X"));
}

TEST(html_render, a_second_reference_in_the_block_queues_no_second_note)
{
	namespace ast = jimd::ast;
	ast::document doc;
	doc.defs.footnotes.emplace("x"sv, ast::inlines{ast::text{"X"sv}});
	doc.blocks = {
		ast::paragraph{.children = {ast::reference{.key = "x"sv, .content = std::nullopt},
		                            ast::reference{.key = "x"sv, .content = std::nullopt}}}};
	EXPECT_EQ(jimd::html::render(doc),
	          "<p>" + note_ref(1) + note_link(1) + "</p>\n" + note_aside(1, "X"));
}

TEST(html_render, a_second_reference_has_no_id_and_no_anchor)
{
	namespace ast = jimd::ast;
	ast::document doc;
	doc.defs.footnotes.emplace("x"sv, ast::inlines{ast::text{"X"sv}});
	doc.blocks = {paragraph_with_reference("a"sv, "x"sv),
	              paragraph_of(ast::reference{.key = "x"sv, .content = std::nullopt}),
	              ast::list{.items = {{ast::reference{.key = "x"sv, .content = std::nullopt}}}}};
	auto const html = jimd::html::render(doc).value_or("");
	EXPECT_EQ(html, "<p>a" + note_ref(1) + "</p>\n" + note_aside(1, "X") + "<p>" + note_link(1) +
	                    "</p>\n<ul>\n<li>" + note_link(1) + "</li>\n</ul>\n");
	EXPECT_EQ(html.find("noteref-1"), html.rfind("noteref-1"));
	EXPECT_EQ(html.find("anchor-name"), html.rfind("anchor-name"));
}

TEST(html_render, a_note_in_a_heading_after_one_in_a_paragraph_is_numbered_after_it)
{
	namespace ast = jimd::ast;
	EXPECT_EQ(render_blocks(
				  {paragraph_with_note("a"sv, "x"sv, "X"sv),
				   ast::heading{
					   .level = 2,
					   .children = {ast::text{"T"sv},
					                ast::reference{.key = "y"sv,
					                               .content = ast::inlines{ast::text{"Y"sv}}}}}}),
	          "<p>a" + note_ref(1) + "</p>\n" + note_aside(1, "X") + "<h2 id=\"t\">T" +
	              note_ref(2) + "</h2>\n" + note_aside(2, "Y"));
}

TEST(html_render, a_note_is_written_as_the_aside_the_stylesheet_expects)
{
	namespace ast = jimd::ast;
	EXPECT_EQ(render_blocks({paragraph_with_note("x"sv, "k"sv, "n"sv)}),
	          "<p>x"
	          R"(<sup class="note-ref" style="anchor-name: --note-1">)"
	          R"(<a href="#note-1" id="noteref-1">1</a></sup></p>)"
	          "\n"
	          R"(<aside id="note-1" data-note="1" style="position-anchor: --note-1">)"
	          "\n<p>n</p>\n</aside>\n");
}

TEST(html_render, a_failure_inside_a_note_fails_the_render)
{
	namespace ast = jimd::ast;
	ast::document doc;
	doc.blocks = {paragraph_of(
		ast::reference{.key = "k"sv,
		               .content = ast::inlines{
						   ast::link{.key = "gone"sv, .text = "t"sv, .target = std::nullopt}}})};
	EXPECT_EQ(jimd::html::render(doc),
	          std::unexpected(jimd::error{.code = jimd::errc::undefined_reference}));
}

TEST(html_render, note_ids_do_not_collide_with_heading_ids)
{
	namespace ast = jimd::ast;
	ast::document doc;
	doc.defs.footnotes.emplace("k"sv, ast::inlines{ast::text{"n"sv}});
	doc.blocks = {heading_of(1, "note 1"sv), heading_of(1, "noteref 1"sv),
	              paragraph_with_reference("a"sv, "k"sv),
	              paragraph_of(ast::reference{.key = "k"sv, .content = std::nullopt})};
	EXPECT_EQ(jimd::html::render(doc),
	          R"(<h1 id="note-1"><a href="#note-1">note 1</a></h1>)"
	          "\n"
	          R"(<h1 id="noteref-1"><a href="#noteref-1">noteref 1</a></h1>)"
	          "\n"
	          "<p>a"
	          R"(<sup class="note-ref" style="anchor-name: --note-1">)"
	          R"(<a href="#note-1-2" id="noteref-1-2">1</a></sup></p>)"
	          "\n"
	          R"(<aside id="note-1-2" data-note="1" style="position-anchor: --note-1">)"
	          "\n<p>n</p>\n</aside>\n"
	          R"(<p><sup class="note-ref"><a href="#note-1-2">1</a></sup></p>)"
	          "\n");
}

TEST(html_render, a_heading_called_note_1_moves_only_the_note_id)
{
	namespace ast = jimd::ast;
	EXPECT_EQ(render_blocks({heading_of(1, "note 1"sv), paragraph_with_note("a"sv, "k"sv, "n"sv)}),
	          R"(<h1 id="note-1"><a href="#note-1">note 1</a></h1>)"
	          "\n<p>a"
	          R"(<sup class="note-ref" style="anchor-name: --note-1">)"
	          R"(<a href="#note-1-2" id="noteref-1">1</a></sup></p>)"
	          "\n"
	          R"(<aside id="note-1-2" data-note="1" style="position-anchor: --note-1">)"
	          "\n<p>n</p>\n</aside>\n");
}

TEST(html_render, a_heading_after_a_note_does_not_reuse_its_id)
{
	namespace ast = jimd::ast;
	EXPECT_EQ(render_blocks({paragraph_with_note("a"sv, "k"sv, "n"sv), heading_of(1, "note 1"sv)}),
	          "<p>a" + note_ref(1) + "</p>\n" + note_aside(1, "n") +
	              "<h1 id=\"note-1-2\"><a href=\"#note-1-2\">note 1</a></h1>\n");
}

TEST(html_render, a_heading_holding_the_first_reference_keeps_its_id_and_moves_the_note)
{
	namespace ast = jimd::ast;
	EXPECT_EQ(render_blocks({ast::heading{
				  .level = 1,
				  .children = {ast::text{"note 1"sv},
				               ast::reference{.key = "k"sv,
				                              .content = ast::inlines{ast::text{"n"sv}}}}}}),
	          R"(<h1 id="note-1">note 1)"
	          R"(<sup class="note-ref" style="anchor-name: --note-1">)"
	          R"(<a href="#note-1-2" id="noteref-1">1</a></sup></h1>)"
	          "\n"
	          R"(<aside id="note-1-2" data-note="1" style="position-anchor: --note-1">)"
	          "\n<p>n</p>\n</aside>\n");
}

TEST(html_render, a_footnote_inside_a_footnote_takes_the_next_number)
{
	namespace ast = jimd::ast;
	EXPECT_EQ(render_blocks({paragraph_of(ast::reference{
				  .key = "outer"sv,
				  .content = ast::inlines{ast::reference{
					  .key = "inner"sv, .content = ast::inlines{ast::text{"i"sv}}}}})}),
	          "<p>" + note_ref(1) + "</p>\n" + note_aside(1, note_ref(2)) + note_aside(2, "i"));
}

TEST(html_render, a_footnote_naming_itself_terminates)
{
	namespace ast = jimd::ast;
	ast::document doc;
	doc.defs.footnotes.emplace("k"sv,
	                           ast::inlines{ast::reference{.key = "k"sv, .content = std::nullopt}});
	doc.blocks = {paragraph_of(ast::reference{.key = "k"sv, .content = std::nullopt})};
	EXPECT_EQ(jimd::html::render(doc),
	          "<p>" + note_ref(1) + "</p>\n" + note_aside(1, note_link(1)));
}

namespace
{
auto parsed(std::string_view source) -> jimd::ast::document
{
	auto doc = jimd::parse(source);
	EXPECT_TRUE(doc.has_value()) << testing::PrintToString(doc);
	return doc.value_or(jimd::ast::document{});
}

auto render_source(std::string_view source) -> std::string
{
	auto const doc = parsed(source);
	auto const html = jimd::html::render(doc);
	EXPECT_TRUE(html.has_value()) << testing::PrintToString(html);
	return html.value_or("");
}

auto tex_of(std::vector<jimd::html::math_request> const& requests) -> std::vector<std::string>
{
	std::vector<std::string> out;
	out.reserve(requests.size());
	for (auto const& request : requests) {
		out.push_back(request.tex);
	}
	return out;
}

constexpr auto begin_size = jimd::detail::align_begin.size();
constexpr auto join_size = jimd::detail::align_join.size();

struct place {
	std::size_t line;
	std::size_t column;

	auto operator==(place const&) const -> bool = default;
};

auto locate_in(std::string_view source, jimd::html::math_request const& request, std::size_t offset)
	-> place
{
	auto const view = request.locate(offset);
	EXPECT_TRUE(view.empty());
	auto const position = jimd::detail::make_error(jimd::errc::syntax, source, view);
	return {.line = position.line, .column = position.column};
}
}

TEST(html_inline_resources, two_inline_notes_with_one_key_are_two_notes)
{
	EXPECT_EQ(render_source("a[^k](X) b[^k](Y)\n"sv), "<p>a" + note_ref(1) + " b" + note_ref(2) +
	                                                      "</p>\n" + note_aside(1, "X") +
	                                                      note_aside(2, "Y"));
	EXPECT_EQ(render_source("a[^k](X)\n\nb[^k](Y)\n"sv),
	          "<p>a" + note_ref(1) + "</p>\n" + note_aside(1, "X") + "<p>b" + note_ref(2) +
	              "</p>\n" + note_aside(2, "Y"));
}

TEST(html_inline_resources, inline_notes_may_have_empty_keys)
{
	EXPECT_EQ(render_source("a[^](X) b[^](Y)\n"sv), "<p>a" + note_ref(1) + " b" + note_ref(2) +
	                                                    "</p>\n" + note_aside(1, "X") +
	                                                    note_aside(2, "Y"));
}

TEST(html_inline_resources, a_defined_note_used_twice_links_the_second_use_to_the_first)
{
	EXPECT_EQ(render_source("a[^d] b[^d]\n\n[^d]: D\n"sv),
	          "<p>a" + note_ref(1) + " b" + note_link(1) + "</p>\n" + note_aside(1, "D"));
	EXPECT_EQ(render_source("a[^d] b[^d](I) c[^d]\n\n[^d]: D\n"sv),
	          "<p>a" + note_ref(1) + " b" + note_ref(2) + " c" + note_link(1) + "</p>\n" +
	              note_aside(1, "D") + note_aside(2, "I"));
}

TEST(html_inline_resources, an_inline_note_and_a_definition_of_one_key_are_separate_notes)
{
	EXPECT_EQ(render_source("a[^k](I) b[^k]\n\n[^k]: D\n"sv),
	          "<p>a" + note_ref(1) + " b" + note_ref(2) + "</p>\n" + note_aside(1, "I") +
	              note_aside(2, "D"));
}

TEST(html_inline_resources, equal_inline_links_keep_their_own_targets)
{
	EXPECT_EQ(render_source("[@a](u) [@a](v) [@a]\n\n[@a]: w\n"sv),
	          "<p><a href=\"u\">a</a> <a href=\"v\">a</a> <a href=\"w\">a</a></p>\n");
}

TEST(html_inline_resources, equal_inline_images_keep_their_own_targets_and_widths)
{
	EXPECT_EQ(render_source("[!a](u.png) [!a](v.png 50%) [!a]\n\n[!a]: w.png 25%\n"sv),
	          "<p><img src=\"u.png\" alt=\"a\"> <img src=\"v.png\" alt=\"a\" style=\"width: 50%\"> "
	          "<img src=\"w.png\" alt=\"a\" style=\"width: 25%\"></p>\n");
}

TEST(html_inline_resources, an_inline_image_may_have_no_alt_text)
{
	EXPECT_EQ(render_source("[!](u.png)\n"sv), "<p><img src=\"u.png\" alt=\"\"></p>\n");
}

TEST(html_math_requests, a_document_without_maths_has_none)
{
	EXPECT_TRUE(jimd::html::math_requests(parsed("text _em_ `c`\n\n- a\n"sv)).empty());
	EXPECT_TRUE(jimd::html::math_requests(jimd::ast::document{}).empty());
}

TEST(html_math_requests, each_kind_of_node_in_source_order)
{
	using jimd::html::math_mode;
	auto const doc = parsed("# H $a$\n"
	                        "\n"
	                        "p $b$ _$c$ x_ *$d$* [@l](u)\n"
	                        "\n"
	                        "- i $e$\n"
	                        "- $f$\n"
	                        "\n"
	                        ">\n"
	                        "q $g$\n"
	                        ">\n"
	                        "$h$\n"
	                        "<\n"
	                        "<\n"
	                        "\n"
	                        "$$\n"
	                        "i\n"
	                        "$$\n"
	                        "\n"
	                        "$$$\n"
	                        "j\n"
	                        "\n"
	                        "k\n"
	                        "$$$\n"
	                        "```\n"
	                        "$not$\n"
	                        "```\n"
	                        "last $l$\n"sv);
	auto const requests = jimd::html::math_requests(doc);
	EXPECT_EQ(tex_of(requests), (std::vector<std::string>{
									"a", "b", "c", "d", "e", "f", "g", "h", "i\n",
									"\\begin{align*}\nj\n\\\\\n\n\\\\\nk\n\\end{align*}", "l"}));
	for (auto const& request : requests) {
		auto const display = request.tex.size() > 1;
		EXPECT_EQ(request.mode, display ? math_mode::display : math_mode::inline_math)
		    << request.tex;
	}
}

TEST(html_math_requests, origin_is_the_tex_the_body_or_the_span_of_the_rows)
{
	auto const doc = parsed("$a$\n\n$$\r\nb\r\n$$\r\n\n$$$\nc\n\nd\n$$$\n"sv);
	auto const requests = jimd::html::math_requests(doc);
	ASSERT_EQ(requests.size(), 3U);
	EXPECT_EQ(requests.at(0).origin, "a");
	EXPECT_EQ(requests.at(0).origin.data(), doc.source.substr(1).data());
	EXPECT_TRUE(requests.at(0).rows.empty());
	EXPECT_EQ(requests.at(1).origin, "b\r\n");
	EXPECT_TRUE(requests.at(1).rows.empty());
	EXPECT_EQ(requests.at(2).origin, "c\n\nd");
	EXPECT_EQ(requests.at(2).rows, (std::vector<std::string_view>{"c", "", "d"}));
}

TEST(html_math_requests, equal_tex_is_requested_as_often_as_it_is_written)
{
	auto const requests = jimd::html::math_requests(parsed("$x$ $x$\n\n$$\nx\n$$\n\n$x$\n"sv));
	EXPECT_EQ(tex_of(requests), (std::vector<std::string>{"x", "x", "x\n", "x"}));
}

TEST(html_math_requests, a_footnote_definition_is_where_it_was_written)
{
	auto const requests = jimd::html::math_requests(parsed("one $a$ [^n] [^m]\n"
	                                                       "\n"
	                                                       "[^m]: note $b$ and $c$\n"
	                                                       "\n"
	                                                       "two $d$\n"
	                                                       "[^n]: note $e$\n"
	                                                       "three $f$\n"
	                                                       "[^o]: unused $g$\n"sv));
	EXPECT_EQ(tex_of(requests), (std::vector<std::string>{"a", "b", "c", "d", "e", "f", "g"}));
}

TEST(html_math_requests, a_definition_in_a_quote_is_between_the_maths_around_it)
{
	auto const requests = jimd::html::math_requests(parsed("[^n]: first $a$\n"
	                                                       "\n"
	                                                       ">\n"
	                                                       "q $b$ [^n]\n"
	                                                       "[^m]: in the quote $c$\n"
	                                                       "r $d$ [^m]\n"
	                                                       "<\n"
	                                                       "s $e$\n"sv));
	EXPECT_EQ(tex_of(requests), (std::vector<std::string>{"a", "b", "c", "d", "e"}));
}

TEST(html_math_requests, an_inline_footnote_is_visited_once_where_it_is_written)
{
	auto const requests =
		jimd::html::math_requests(parsed("a $x$ b[^m](in $i$ and $j$) c $y$ d[^m](again $k$)\n"
		                                 "\n"
		                                 "- item $z$\n"sv));
	EXPECT_EQ(tex_of(requests), (std::vector<std::string>{"x", "i", "j", "y", "k", "z"}));
}

TEST(html_math_requests, an_inline_footnote_in_a_quote_a_heading_or_emphasis_is_visited_once)
{
	auto const requests = jimd::html::math_requests(parsed("# h[^a](a $a$)\n"
	                                                       "\n"
	                                                       ">\n"
	                                                       "q _e[^b](b $b$) $c$_ f\n"
	                                                       "<\n"
	                                                       "\n"
	                                                       "- *s[^c](c $d$)* g\n"sv));
	EXPECT_EQ(tex_of(requests), (std::vector<std::string>{"a", "b", "c", "d"}));
}

TEST(html_math_requests, inline_notes_with_one_key_are_each_visited_and_so_is_a_definition_of_it)
{
	auto const requests = jimd::html::math_requests(parsed("a[^n](one $a$) b[^n](two $b$) [^n]\n"
	                                                       "\n"
	                                                       "[^n]: def $c$\n"
	                                                       "[^u]: unused $d$\n"sv));
	EXPECT_EQ(tex_of(requests), (std::vector<std::string>{"a", "b", "c", "d"}));
}

TEST(html_math_requests, code_links_and_images_hold_no_maths)
{
	auto const requests = jimd::html::math_requests(parsed("`$a$` [@l](u) [!i](u.png) $d$\n"sv));
	EXPECT_EQ(tex_of(requests), (std::vector<std::string>{"d"}));
}

TEST(html_math_requests, inline_and_defined_footnotes_together)
{
	auto const requests = jimd::html::math_requests(parsed("[^d]: def $a$\n"
	                                                       "text[^i](inline $b$) [^d]\n"
	                                                       "\n"
	                                                       "[^e]: def $c$\n"
	                                                       "after $d$ [^e]\n"sv));
	EXPECT_EQ(tex_of(requests), (std::vector<std::string>{"a", "b", "c", "d"}));
}

TEST(html_math_requests, definitions_of_a_tree_built_by_hand_follow_the_blocks_in_key_order)
{
	namespace ast = jimd::ast;
	ast::document doc;
	doc.blocks = {ast::paragraph{.children = {ast::math{"p"sv}}}, ast::display_math{"q\n"sv}};
	doc.defs.footnotes["b"] = {ast::math{"from b"sv}};
	doc.defs.footnotes["a"] = {ast::text{"x"sv}, ast::math{"from a"sv}};
	EXPECT_EQ(tex_of(jimd::html::math_requests(doc)),
	          (std::vector<std::string>{"p", "q\n", "from a", "from b"}));
}

TEST(html_math_requests, maths_outside_the_source_goes_after_the_maths_in_it)
{
	namespace ast = jimd::ast;
	constexpr auto source = "$y$ and $z$"sv;
	ast::document doc;
	doc.source = source;
	doc.blocks = {ast::paragraph{.children = {ast::math{"outside"sv}}},
	              ast::paragraph{.children = {ast::math{source.substr(source.find('z'), 1)}}}};
	doc.defs.footnotes["n"] = {ast::math{source.substr(source.find('y'), 1)}};
	EXPECT_EQ(tex_of(jimd::html::math_requests(doc)),
	          (std::vector<std::string>{"y", "z", "outside"}));
}

TEST(html_math_requests, the_requests_are_what_render_looks_up)
{
	auto const doc = parsed("a $x$[^n](in $y$) [^m]\n\n[^m]: $z$\n\n$$\nd\n$$\n\n$$$\nr\n$$$\n"sv);
	jimd::html::math_table table;
	for (auto const& request : jimd::html::math_requests(doc)) {
		table.insert_or_assign({request.mode, request.tex}, "<math/>");
	}
	EXPECT_EQ(table.size(), 5U);
	EXPECT_TRUE(jimd::html::render(doc, {.math = &table}).has_value());
}

TEST(html_code_requests, a_document_without_code_has_none)
{
	EXPECT_TRUE(jimd::html::code_requests(parsed("text `c`\n\n$$\nx\n$$\n\n- a\n"sv)).empty());
	EXPECT_TRUE(jimd::html::code_requests(jimd::ast::document{}).empty());
}

TEST(html_code_requests, are_in_source_order_in_top_level_blocks_and_quotes)
{
	auto const doc = parsed("```a\n1\n```\n"
	                        "\n"
	                        ">\n"
	                        "```b x=y\n"
	                        "2\n"
	                        "```\n"
	                        ">\n"
	                        "```c\n"
	                        "3\n"
	                        "```\n"
	                        "<\n"
	                        "```d\n"
	                        "4\n"
	                        "```\n"
	                        "<\n"
	                        "\n"
	                        "```e\n"
	                        "5\n"
	                        "```\n"sv);
	auto const requests = jimd::html::code_requests(doc);
	ASSERT_EQ(requests.size(), 5U);
	constexpr std::array languages{"a"sv, "b"sv, "c"sv, "d"sv, "e"sv};
	constexpr std::array bodies{"1\n"sv, "2\n"sv, "3\n"sv, "4\n"sv, "5\n"sv};
	for (std::size_t index = 0; index < requests.size(); ++index) {
		EXPECT_EQ(requests.at(index).language, languages.at(index));
		EXPECT_EQ(requests.at(index).body, bodies.at(index));
	}
}

TEST(html_code_requests, a_block_with_no_info_string_is_not_requested)
{
	auto const requests = jimd::html::code_requests(parsed("```\nx\n```\n\n```c\ny\n```\n"sv));
	ASSERT_EQ(requests.size(), 1U);
	EXPECT_EQ(requests.front().language, "c");
	EXPECT_EQ(requests.front().body, "y\n");
}

TEST(html_code_requests, the_language_is_the_first_word_and_a_view_into_the_source)
{
	auto const doc = parsed("```cpp title=x\nint x;\n```\n"sv);
	auto const requests = jimd::html::code_requests(doc);
	ASSERT_EQ(requests.size(), 1U);
	EXPECT_EQ(requests.front().language, "cpp");
	EXPECT_TRUE(jimd::detail::contains(doc.source, requests.front().language));
	EXPECT_EQ(requests.front().language.data(), doc.source.substr(3).data());
	auto const tab = jimd::html::code_requests(parsed("```c\tx\n\n```\n"sv));
	ASSERT_EQ(tab.size(), 1U);
	EXPECT_EQ(tab.front().language, "c");
}

TEST(html_code_requests, the_body_has_each_crlf_turned_into_lf)
{
	auto const requests = jimd::html::code_requests(parsed("```c\r\na\r\n\rb\r\n```\r\n"sv));
	ASSERT_EQ(requests.size(), 1U);
	EXPECT_EQ(requests.front().body, "a\n\rb\n");
}

TEST(html_code_requests, equal_blocks_are_requested_as_often_as_they_are_written)
{
	auto const requests =
		jimd::html::code_requests(parsed("```c\nx\n```\n\n>\n```c\nx\n```\n<\n\n```c\nx\n```\n"sv));
	ASSERT_EQ(requests.size(), 3U);
	for (auto const& request : requests) {
		EXPECT_EQ(request.language, "c");
		EXPECT_EQ(request.body, "x\n");
	}
}

TEST(html_code_requests, a_tree_built_by_hand_is_walked_in_order)
{
	namespace ast = jimd::ast;
	ast::document doc;
	doc.blocks = {ast::code_block{.info = "a"sv, .body = "1"sv},
	              ast::quote{.children = {ast::code_block{.info = "b"sv, .body = "2"sv},
	                                      ast::paragraph{},
	                                      ast::code_block{.info = ""sv, .body = "no"sv}}},
	              ast::code_block{.info = "c"sv, .body = "3"sv}};
	auto const requests = jimd::html::code_requests(doc);
	ASSERT_EQ(requests.size(), 3U);
	EXPECT_EQ(requests.at(0).language, "a");
	EXPECT_EQ(requests.at(1).language, "b");
	EXPECT_EQ(requests.at(2).language, "c");
}

TEST(html_code_requests, the_requests_are_what_render_looks_up)
{
	auto const doc = parsed("```a\r\nx\r\n```\r\n\n>\n```b t\n<&>\n```\n<\n\n```\nplain\n```\n"sv);
	jimd::html::code_table table;
	for (auto const& request : jimd::html::code_requests(doc)) {
		table.insert_or_assign({std::string{request.language}, request.body}, "<i/>");
	}
	EXPECT_EQ(table.size(), 2U);
	EXPECT_EQ(jimd::html::render(doc, {.code = &table}),
	          "<pre class=\"highlight\"><code class=\"language-a\"><i/></code></pre>\n"
	          "<blockquote>\n"
	          "<pre class=\"highlight\"><code class=\"language-b\"><i/></code></pre>\n"
	          "</blockquote>\n"
	          "<pre><code>plain\n</code></pre>\n");
}

TEST(html_math_request_locate, inline_maths_maps_into_the_tex_byte_for_byte)
{
	auto const doc = parsed("ab $x^2+y$ c\n"sv);
	auto const requests = jimd::html::math_requests(doc);
	ASSERT_EQ(requests.size(), 1U);
	auto const& request = requests.front();
	EXPECT_EQ(locate_in(doc.source, request, 0), (place{.line = 1, .column = 5}));
	EXPECT_EQ(locate_in(doc.source, request, 2), (place{.line = 1, .column = 7}));
	EXPECT_EQ(request.locate(2).data(), doc.source.substr(6).data());
	EXPECT_EQ(locate_in(doc.source, request, 5), (place{.line = 1, .column = 10}));
	EXPECT_EQ(locate_in(doc.source, request, 6), (place{.line = 1, .column = 10}));
	EXPECT_EQ(locate_in(doc.source, request, 1000), (place{.line = 1, .column = 10}));
}

TEST(html_math_request_locate, a_display_body_skips_the_dropped_carriage_returns)
{
	auto const doc = parsed("$$\r\na\r\nb+\r\n$$\r\n"sv);
	auto const requests = jimd::html::math_requests(doc);
	ASSERT_EQ(requests.size(), 1U);
	auto const& request = requests.front();
	ASSERT_EQ(request.tex, "a\nb+\n");
	EXPECT_EQ(locate_in(doc.source, request, 0), (place{.line = 2, .column = 1}));
	EXPECT_EQ(locate_in(doc.source, request, 1), (place{.line = 2, .column = 3}));
	EXPECT_EQ(locate_in(doc.source, request, 2), (place{.line = 3, .column = 1}));
	EXPECT_EQ(locate_in(doc.source, request, 3), (place{.line = 3, .column = 2}));
	EXPECT_EQ(locate_in(doc.source, request, 4), (place{.line = 3, .column = 4}));
	EXPECT_EQ(locate_in(doc.source, request, 5), (place{.line = 4, .column = 1}));
	EXPECT_EQ(locate_in(doc.source, request, 99), (place{.line = 4, .column = 1}));
}

TEST(html_math_request_locate, a_display_body_with_lf_line_ends_and_a_lone_carriage_return)
{
	auto const doc = parsed("$$\na\rb\nc\n$$\n"sv);
	auto const requests = jimd::html::math_requests(doc);
	ASSERT_EQ(requests.size(), 1U);
	auto const& request = requests.front();
	ASSERT_EQ(request.tex, "a\rb\nc\n");
	EXPECT_EQ(locate_in(doc.source, request, 1), (place{.line = 2, .column = 2}));
	EXPECT_EQ(locate_in(doc.source, request, 4), (place{.line = 3, .column = 1}));
}

TEST(html_math_request_locate, aligned_maths_maps_into_each_row)
{
	auto const doc = parsed("$$$\nab\n\ncd &= e\n$$$\n"sv);
	auto const requests = jimd::html::math_requests(doc);
	ASSERT_EQ(requests.size(), 1U);
	auto const& request = requests.front();
	constexpr auto row_0 = begin_size;
	constexpr auto row_2 = row_0 + 2 + join_size + join_size;
	EXPECT_EQ(locate_in(doc.source, request, row_0), (place{.line = 2, .column = 1}));
	EXPECT_EQ(locate_in(doc.source, request, row_0 + 1), (place{.line = 2, .column = 2}));
	EXPECT_EQ(locate_in(doc.source, request, row_2), (place{.line = 4, .column = 1}));
	EXPECT_EQ(locate_in(doc.source, request, row_2 + 1), (place{.line = 4, .column = 2}));
	EXPECT_EQ(locate_in(doc.source, request, row_2 + 6), (place{.line = 4, .column = 7}));
}

TEST(html_math_request_locate, aligned_maths_in_the_begin_is_the_start_of_the_first_row)
{
	auto const doc = parsed("$$$\nab\ncd\n$$$\n"sv);
	auto const requests = jimd::html::math_requests(doc);
	ASSERT_EQ(requests.size(), 1U);
	for (std::size_t offset = 0; offset < begin_size; ++offset) {
		EXPECT_EQ(locate_in(doc.source, requests.front(), offset), (place{.line = 2, .column = 1}))
		    << offset;
	}
}

TEST(html_math_request_locate, aligned_maths_in_a_joiner_is_the_end_of_the_row_before)
{
	auto const doc = parsed("$$$\nab\n\ncd\n$$$\n"sv);
	auto const requests = jimd::html::math_requests(doc);
	ASSERT_EQ(requests.size(), 1U);
	constexpr auto first_join = begin_size + 2;
	constexpr auto second_join = first_join + join_size;
	constexpr auto last_row = second_join + join_size;
	for (std::size_t offset = first_join; offset < second_join; ++offset) {
		EXPECT_EQ(locate_in(doc.source, requests.front(), offset), (place{.line = 2, .column = 3}))
		    << offset;
	}
	for (std::size_t offset = second_join; offset < last_row; ++offset) {
		EXPECT_EQ(locate_in(doc.source, requests.front(), offset), (place{.line = 3, .column = 1}))
		    << offset;
	}
	EXPECT_EQ(locate_in(doc.source, requests.front(), last_row), (place{.line = 4, .column = 1}));
}

TEST(html_math_request_locate, aligned_maths_in_the_end_is_the_end_of_the_last_row)
{
	auto const doc = parsed("$$$\nab\r\ncd &= e\r\n$$$\r\n"sv);
	auto const requests = jimd::html::math_requests(doc);
	ASSERT_EQ(requests.size(), 1U);
	auto const& request = requests.front();
	auto const rows_end = request.tex.size() - std::string_view{"\n\\end{align*}"}.size();
	EXPECT_EQ(locate_in(doc.source, request, rows_end - 1), (place{.line = 3, .column = 7}));
	for (auto const offset : {rows_end, rows_end + 1, rows_end + 5, request.tex.size(),
	                          request.tex.size() + 1, std::size_t{1000}}) {
		EXPECT_EQ(locate_in(doc.source, request, offset), (place{.line = 3, .column = 8}))
		    << offset;
	}
}

TEST(html_math_request_locate, aligned_maths_with_crlf_line_ends_ends_each_row_before_its_cr)
{
	auto const doc = parsed("$$$\r\nab\r\n\r\ncd\r\n$$$\r\n"sv);
	auto const requests = jimd::html::math_requests(doc);
	ASSERT_EQ(requests.size(), 1U);
	auto const& request = requests.front();
	ASSERT_EQ(request.tex, "\\begin{align*}\nab\n\\\\\n\n\\\\\ncd\n\\end{align*}");
	constexpr auto first_join = begin_size + 2;
	constexpr auto second_join = first_join + join_size;
	constexpr auto last_row = second_join + join_size;
	EXPECT_EQ(locate_in(doc.source, request, begin_size), (place{.line = 2, .column = 1}));
	EXPECT_EQ(locate_in(doc.source, request, first_join - 1), (place{.line = 2, .column = 2}));
	EXPECT_EQ(locate_in(doc.source, request, first_join), (place{.line = 2, .column = 3}));
	EXPECT_EQ(locate_in(doc.source, request, second_join), (place{.line = 3, .column = 1}));
	EXPECT_EQ(locate_in(doc.source, request, last_row), (place{.line = 4, .column = 1}));
	EXPECT_EQ(locate_in(doc.source, request, last_row + 2), (place{.line = 4, .column = 3}));
}

TEST(html_math_request_locate, a_single_aligned_row_has_no_joiner)
{
	auto const doc = parsed("$$$\nx\n$$$\n"sv);
	auto const requests = jimd::html::math_requests(doc);
	ASSERT_EQ(requests.size(), 1U);
	EXPECT_EQ(locate_in(doc.source, requests.front(), begin_size), (place{.line = 2, .column = 1}));
	EXPECT_EQ(locate_in(doc.source, requests.front(), begin_size + 1),
	          (place{.line = 2, .column = 2}));
	EXPECT_EQ(locate_in(doc.source, requests.front(), begin_size + 2),
	          (place{.line = 2, .column = 2}));
}

TEST(html_smarten, a_quote_opens_at_the_start_and_after_a_blank_or_an_opening_character)
{
	EXPECT_EQ(jimd::html::smarten("\"a\" 'b'"sv), "“a” ‘b’");
	EXPECT_EQ(jimd::html::smarten("x \"a\""sv), "x “a”");
	EXPECT_EQ(jimd::html::smarten("x\t'a'"sv), "x\t‘a’");
	EXPECT_EQ(jimd::html::smarten("x\n'a'"sv), "x\n‘a’");
	for (auto const* const opener : {"(", "[", "{", "/", "-", "‘", "“", "—", "–"}) {
		EXPECT_EQ(jimd::html::smarten(std::string{opener} + "'a"), std::string{opener} + "‘a")
		    << opener;
		EXPECT_EQ(jimd::html::smarten(std::string{opener} + "\"a"), std::string{opener} + "“a")
		    << opener;
	}
}

TEST(html_smarten, a_quote_closes_after_anything_else)
{
	for (auto const* const closer :
	     {"a", "Z", "7", ".", ",", ")", "]", "}", "’", "”", "…", "$", "é", "日", "😀", "_", "*"}) {
		EXPECT_EQ(jimd::html::smarten(std::string{closer} + "'"), std::string{closer} + "’")
		    << closer;
		EXPECT_EQ(jimd::html::smarten(std::string{closer} + "\""), std::string{closer} + "”")
		    << closer;
	}
	EXPECT_EQ(jimd::html::smarten("don't"sv), "don’t");
}

TEST(html_smarten, before_is_the_character_that_comes_first)
{
	EXPECT_EQ(jimd::html::smarten("'a"sv), "‘a");
	EXPECT_EQ(jimd::html::smarten("'a"sv, U'\n'), "‘a");
	EXPECT_EQ(jimd::html::smarten("'a"sv, U'x'), "’a");
	EXPECT_EQ(jimd::html::smarten("'a"sv, U'é'), "’a");
	EXPECT_EQ(jimd::html::smarten("'a"sv, U'—'), "‘a");
	EXPECT_EQ(jimd::html::smarten("'a"sv, U'\x2013'), "‘a");
	EXPECT_EQ(jimd::html::smarten("'a"sv, U'\x2018'), "‘a");
	EXPECT_EQ(jimd::html::smarten("'a"sv, U'\x201C'), "‘a");
	EXPECT_EQ(jimd::html::smarten("'a"sv, U'\x2019'), "’a");
	EXPECT_EQ(jimd::html::smarten("'a"sv, U'\r'), "’a");
	EXPECT_EQ(jimd::html::smarten(""sv, U'x'), "");
}

TEST(html_smarten, a_quote_goes_by_the_character_written_before_it)
{
	EXPECT_EQ(jimd::html::smarten("\"'a'\""sv), "“‘a’”");
	EXPECT_EQ(jimd::html::smarten("'\"a\"'"sv), "‘“a”’");
	EXPECT_EQ(jimd::html::smarten("--\"a\""sv), "—“a”");
	EXPECT_EQ(jimd::html::smarten("...\"a\""sv), "…”a”");
}

TEST(html_smarten, dots_and_dashes_go_left_to_right)
{
	EXPECT_EQ(jimd::html::smarten("a...b"sv), "a…b");
	EXPECT_EQ(jimd::html::smarten(".."sv), "..");
	EXPECT_EQ(jimd::html::smarten("...."sv), "….");
	EXPECT_EQ(jimd::html::smarten("....."sv), "…..");
	EXPECT_EQ(jimd::html::smarten("......"sv), "……");
	EXPECT_EQ(jimd::html::smarten("-"sv), "-");
	EXPECT_EQ(jimd::html::smarten("a - b"sv), "a - b");
	EXPECT_EQ(jimd::html::smarten("a--b"sv), "a—b");
	EXPECT_EQ(jimd::html::smarten("---"sv), "—-");
	EXPECT_EQ(jimd::html::smarten("----"sv), "——");
	EXPECT_EQ(jimd::html::smarten(". . ."sv), ". . .");
	EXPECT_EQ(jimd::html::smarten("-.-"sv), "-.-");
}

TEST(html_smarten, the_known_limitations_are_pinned)
{
	EXPECT_EQ(jimd::html::smarten("the '90s"sv), "the ‘90s");
	EXPECT_EQ(jimd::html::smarten("rock 'n' roll"sv), "rock ‘n’ roll");
}

TEST(html_smarten, other_characters_pass_through_as_they_are)
{
	EXPECT_EQ(jimd::html::smarten("<a> & é 日 😀 “ ‘ — … \\ `"sv), "<a> & é 日 😀 “ ‘ — … \\ `");
	EXPECT_EQ(jimd::html::smarten("日'"sv), "日’");
}

TEST(html_smarten, bytes_that_are_not_utf8_are_copied_and_count_as_letters)
{
	EXPECT_EQ(jimd::html::smarten("\xE2\x80'"sv), "\xE2\x80’");
	EXPECT_EQ(jimd::html::smarten("\x80'"sv), "\x80’");
	EXPECT_EQ(jimd::html::smarten("\xF0\x82\x80\x98'"sv), "\xF0\x82\x80\x98’");
	EXPECT_EQ(jimd::html::smarten("\xC0\x80'"sv), "\xC0\x80’");
	EXPECT_EQ(jimd::html::smarten("\xF5\x80\x80\x80'"sv), "\xF5\x80\x80\x80’");
	EXPECT_EQ(jimd::html::smarten("\xE2\x80"sv), "\xE2\x80");
	EXPECT_EQ(jimd::html::smarten("\xE2\x80\x98'"sv), "\xE2\x80\x98‘");
}

TEST(html_typography, text_is_smartened_and_then_escaped)
{
	namespace ast = jimd::ast;
	EXPECT_EQ(render_blocks({paragraph_of(ast::text{"\"a\" & <b> 'c' ... --"sv})}),
	          "<p>“a” &amp; &lt;b&gt; ‘c’ … —</p>\n");
}

TEST(html_typography, emphasis_and_strong_do_not_change_what_comes_before)
{
	namespace ast = jimd::ast;
	auto const render_children = [](ast::inlines children) -> std::string {
		return render_blocks({ast::paragraph{.children = std::move(children)}});
	};
	EXPECT_EQ(render_children({ast::emphasis{.children = {ast::text{"\"What's\""sv}}}}),
	          "<p><em>“What’s”</em></p>\n");
	EXPECT_EQ(render_children({ast::text{"a "sv}, ast::strong{.children = {ast::text{"'b'"sv}}}}),
	          "<p>a <strong>‘b’</strong></p>\n");
	EXPECT_EQ(render_children({ast::text{"a"sv}, ast::emphasis{.children = {ast::text{"'b"sv}}}}),
	          "<p>a<em>’b</em></p>\n");
	EXPECT_EQ(render_children({ast::text{"\""sv}, ast::emphasis{.children = {ast::text{"x"sv}}},
	                           ast::text{"\""sv}}),
	          "<p>“<em>x</em>”</p>\n");
	EXPECT_EQ(render_children({ast::emphasis{.children = {ast::text{"x "sv}}}, ast::text{"'y"sv}}),
	          "<p><em>x </em>‘y</p>\n");
}

TEST(html_typography, code_maths_images_and_note_references_count_as_letters)
{
	namespace ast = jimd::ast;
	jimd::html::math_table const table{{{jimd::html::math_mode::inline_math, "x"}, "<math/>"}};
	ast::document doc;
	doc.defs.footnotes.emplace("k"sv, ast::inlines{ast::text{"n"sv}});
	doc.defs.images.emplace("pic"sv, ast::image_def{.target = "pic.png"sv, .width = std::nullopt});
	auto const quote = ast::text{"\""sv};
	doc.blocks = {
		ast::paragraph{.children = {ast::math{"x"sv}, quote}},
		ast::paragraph{.children = {ast::code_span{"c"sv}, quote}},
		ast::paragraph{.children = {ast::image{.key = "pic"sv,
		                                       .alt = "pic"sv,
		                                       .target = std::nullopt,
		                                       .width = std::nullopt},
		                            quote}},
		ast::paragraph{.children = {ast::reference{.key = "k"sv, .content = std::nullopt}, quote}},
		ast::paragraph{.children = {ast::reference{.key = "k"sv, .content = std::nullopt}, quote}}};
	auto const html = jimd::html::render(doc, {.math = &table});
	ASSERT_TRUE(html.has_value());
	EXPECT_EQ(*html,
	          "<p><math/>”</p>\n<p><code>c</code>”</p>\n<p><img src=\"pic.png\" alt=\"pic\">”</p>\n"
	          "<p>" +
	              note_ref(1) + "”</p>\n" + note_aside(1, "n") + "<p>" + note_link(1) + "”</p>\n");
}

TEST(html_typography, a_soft_break_is_a_line_end_so_a_quote_after_it_opens)
{
	namespace ast = jimd::ast;
	EXPECT_EQ(render_blocks({ast::paragraph{
				  .children = {ast::text{"a'"sv}, ast::soft_break{}, ast::text{"'b'"sv}}}}),
	          "<p>a’\n‘b’</p>\n");
}

TEST(html_typography, each_block_item_heading_and_note_starts_afresh)
{
	namespace ast = jimd::ast;
	EXPECT_EQ(render_blocks({paragraph_of(ast::text{"a"sv}), paragraph_of(ast::text{"'b"sv}),
	                         heading_of(1, "'h"sv),
	                         ast::list{.items = {{ast::text{"x"sv}}, {ast::text{"'i"sv}}}},
	                         ast::quote{.children = {paragraph_of(ast::text{"y"sv}),
	                                                 paragraph_of(ast::text{"'q"sv})}}}),
	          "<p>a</p>\n<p>‘b</p>\n<h1 id=\"h\"><a href=\"#h\">‘h</a></h1>\n"
	          "<ul>\n<li>x</li>\n<li>‘i</li>\n</ul>\n"
	          "<blockquote>\n<p>y</p>\n<p>‘q</p>\n</blockquote>\n");
	EXPECT_EQ(render_blocks({paragraph_with_note("a"sv, "k"sv, "'n'"sv)}),
	          "<p>a" + note_ref(1) + "</p>\n" + note_aside(1, "‘n’"));
}

TEST(html_typography, link_text_is_smartened_and_goes_by_what_comes_before_it)
{
	namespace ast = jimd::ast;
	EXPECT_EQ(render_blocks({ast::paragraph{
				  .children = {ast::text{"\""sv},
				               ast::link{.key = "k"sv, .text = "it's"sv, .target = "/u"sv},
				               ast::text{"\" 'x"sv}}}}),
	          "<p>“<a href=\"/u\">it’s</a>” ‘x</p>\n");
	EXPECT_EQ(render_blocks({ast::paragraph{
				  .children = {ast::text{"x"sv},
				               ast::link{.key = "k"sv, .text = "'y'"sv, .target = "/u"sv}}}}),
	          "<p>x<a href=\"/u\">’y’</a></p>\n");
	EXPECT_EQ(render_blocks({ast::paragraph{
				  .children = {ast::link{.key = "k"sv, .text = "'y'"sv, .target = "/u"sv}}}}),
	          "<p><a href=\"/u\">‘y’</a></p>\n");
}

TEST(html_typography, code_attributes_and_ids_are_left_alone)
{
	namespace ast = jimd::ast;
	EXPECT_EQ(render_blocks({ast::code_block{.info = "c"sv, .body = "'a' \"b\" -- ...\n"sv}}),
	          "<pre><code class=\"language-c\">'a' \"b\" -- ...\n</code></pre>\n");
	EXPECT_EQ(render_blocks({paragraph_of(ast::code_span{"'a' -- ..."sv})}),
	          "<p><code>'a' -- ...</code></p>\n");
	EXPECT_EQ(render_blocks(
				  {paragraph_of(ast::link{.key = "k"sv, .text = "t"sv, .target = "/a'b\"c"sv})}),
	          "<p><a href=\"/a'b&quot;c\">t</a></p>\n");
	EXPECT_EQ(
		render_blocks({paragraph_of(ast::image{
			.key = "k"sv, .alt = "\"a's\" ..."sv, .target = "/i'.png"sv, .width = std::nullopt})}),
		"<p><img src=\"/i'.png\" alt=\"&quot;a's&quot; ...\"></p>\n");
}

TEST(html_typography, a_heading_is_smartened_and_its_slug_is_not)
{
	EXPECT_EQ(render_blocks({heading_of(2, "Let's \"go\" -- now..."sv)}),
	          "<h2 id=\"lets-go-now\"><a href=\"#lets-go-now\">Let’s “go” — now…</a></h2>\n");
}

TEST(html_typography, an_escape_splits_the_text_so_what_it_protects_is_not_joined)
{
	EXPECT_EQ(render_source("-\\-"sv), "<p>--</p>\n");
	EXPECT_EQ(render_source("\\--"sv), "<p>--</p>\n");
	EXPECT_EQ(render_source("\\-\\-"sv), "<p>--</p>\n");
	EXPECT_EQ(render_source("a -- b"sv), "<p>a — b</p>\n");
	EXPECT_EQ(render_source("-_-_"sv), "<p>-<em>-</em></p>\n");
	EXPECT_EQ(render_source("..\n."sv), "<p>..\n.</p>\n");
	EXPECT_EQ(render_source(".._._"sv), "<p>..<em>.</em></p>\n");
	EXPECT_EQ(render_source("\\-\"x\""sv), "<p>-“x”</p>\n");
	EXPECT_EQ(render_source("a\\_\"x\""sv), "<p>a_”x”</p>\n");
	EXPECT_EQ(render_source("a\\$'"sv), "<p>a$’</p>\n");
}

TEST(html_typography, parsed_prose_is_not_split_where_a_rule_needs_it_whole)
{
	EXPECT_EQ(render_source("\"[x]\" wait... a -- b ... see [\"y\"] and snake_case's"sv),
	          "<p>“[x]” wait… a — b … see [“y”] and snake_case’s</p>\n");
	EXPECT_EQ(render_source("a\t'b'\r\n\"c\"\r\n\r\n- \"d\"\r\n"sv),
	          "<p>a\t‘b’\n“c”</p>\n<ul>\n<li>“d”</li>\n</ul>\n");
	EXPECT_EQ(render_source("[@a](u)'s [@b](v) '"sv),
	          "<p><a href=\"u\">a</a>’s <a href=\"v\">b</a> ‘</p>\n");
}

TEST(html_typography, parsed_text_with_quotes_in_every_place_that_has_text)
{
	auto const doc =
		parsed("# It's \"a\" title\n\n- 'one'\n\n>\n\"two\"\n<\n\nx[^n]\n\n[^n]: 'n'\n");
	auto const html = jimd::html::render(doc);
	ASSERT_TRUE(html.has_value());
	EXPECT_EQ(*html,
	          "<h1 id=\"its-a-title\"><a href=\"#its-a-title\">It’s “a” title</a></h1>\n"
	          "<ul>\n<li>‘one’</li>\n</ul>\n<blockquote>\n<p>“two”</p>\n</blockquote>\n<p>x" +
	              note_ref(1) + "</p>\n" + note_aside(1, "‘n’"));
}

namespace
{
using ratio_table = std::flat_map<std::string, std::string, std::less<>>;

auto render_figures(std::string_view source, ratio_table const& ratios) -> jimd::result<std::string>
{
	auto const doc = parsed(source);
	return jimd::html::render(doc, {.image_ratios = &ratios});
}

struct div_fields {
	std::string_view alt;
	std::string_view target;
	std::string_view ratio;
	std::string_view width = ""sv;
};

auto svg_div(div_fields const& fields) -> std::string
{
	auto out = std::format(R"(<div class="ink-mask" role="img" aria-label="{}" )"
	                       R"(style="--src: url('{}'); --ratio: {})",
	                       fields.alt, fields.target, fields.ratio);
	if (!fields.width.empty()) {
		out += std::format("; --width: {}", fields.width);
	}
	return out + "\"></div>\n";
}

auto image_node(std::string_view key, std::string_view target) -> jimd::ast::image
{
	return jimd::ast::image{.key = key, .alt = key, .target = target, .width = std::nullopt};
}
}

TEST(html_figure, a_paragraph_with_one_image_is_a_figure)
{
	namespace ast = jimd::ast;
	EXPECT_TRUE(jimd::html::is_figure(paragraph_of(image_node("a"sv, "x.png"sv))));
	EXPECT_TRUE(jimd::html::is_figure(paragraph_of(image_node("a"sv, "/x.svg"sv))));
	EXPECT_TRUE(jimd::html::is_figure(ast::paragraph{
		.children = {ast::text{" \t"sv}, image_node("a"sv, "x.png"sv), ast::text{"  "sv}}}));
	EXPECT_TRUE(jimd::html::is_figure(
		ast::paragraph{.children = {ast::text{""sv}, image_node("a"sv, "x.png"sv)}}));
}

TEST(html_figure, anything_else_is_a_paragraph)
{
	namespace ast = jimd::ast;
	auto const pic = image_node("a"sv, "x.png"sv);
	EXPECT_FALSE(jimd::html::is_figure(ast::paragraph{}));
	EXPECT_FALSE(jimd::html::is_figure(paragraph_of(ast::text{" "sv})));
	EXPECT_FALSE(jimd::html::is_figure(ast::paragraph{.children = {pic, pic}}));
	EXPECT_FALSE(jimd::html::is_figure(ast::paragraph{.children = {pic, ast::text{" "sv}, pic}}));
	EXPECT_FALSE(jimd::html::is_figure(ast::paragraph{.children = {pic, ast::text{"x"sv}}}));
	EXPECT_FALSE(jimd::html::is_figure(ast::paragraph{.children = {ast::text{"x "sv}, pic}}));
	EXPECT_FALSE(jimd::html::is_figure(ast::paragraph{.children = {pic, ast::soft_break{}}}));
	EXPECT_FALSE(jimd::html::is_figure(ast::paragraph{.children = {ast::soft_break{}, pic}}));
	EXPECT_FALSE(jimd::html::is_figure(paragraph_of(ast::emphasis{.children = {pic}})));
	EXPECT_FALSE(jimd::html::is_figure(ast::paragraph{.children = {pic, ast::text{"\xC2\xA0"sv}}}));
	EXPECT_FALSE(jimd::html::is_figure(ast::paragraph{.children = {pic, ast::code_span{""sv}}}));
}

TEST(html_figure, an_svg_figure_is_a_masked_div_with_its_ratio)
{
	ratio_table const ratios{{"/image/boiler.svg", "600/740"}};
	EXPECT_EQ(render_figures("[!boiler](/image/boiler.svg)\n"sv, ratios),
	          svg_div({"boiler", "/image/boiler.svg", "600/740"}));
	EXPECT_EQ(render_figures("[!boiler](/image/boiler.svg 75%)\n"sv, ratios),
	          svg_div({"boiler", "/image/boiler.svg", "600/740", "75%"}));
}

TEST(html_figure, a_figure_is_a_block_between_other_blocks)
{
	ratio_table const ratios{{"/x.svg", "1/1"}};
	EXPECT_EQ(render_figures("Before\n\n[!a](/x.svg)\n\nAfter\n"sv, ratios),
	          "<p>Before</p>\n" + svg_div({"a", "/x.svg", "1/1"}) + "<p>After</p>\n");
}

TEST(html_figure, a_reference_to_a_defined_image_takes_the_target_and_width_of_its_definition)
{
	ratio_table const ratios{{"/x.svg", "3/2"}};
	EXPECT_EQ(render_figures("[!a]\n\n[!a]: /x.svg 50%\n"sv, ratios),
	          svg_div({"a", "/x.svg", "3/2", "50%"}));
	EXPECT_EQ(render_figures("[!a]\n\n[!a]: /x.svg\n"sv, ratios), svg_div({"a", "/x.svg", "3/2"}));
}

TEST(html_figure, an_empty_alt_hides_the_div_from_assistive_technology)
{
	namespace ast = jimd::ast;
	ratio_table const ratios{{"/x.svg", "1/1"}};
	ast::document doc;
	doc.blocks = {paragraph_of(
		ast::image{.key = "k"sv, .alt = ""sv, .target = "/x.svg"sv, .width = "40%"sv})};
	EXPECT_EQ(jimd::html::render(doc, {.image_ratios = &ratios}),
	          "<div class=\"ink-mask\" aria-hidden=\"true\" "
	          "style=\"--src: url('/x.svg'); --ratio: 1/1; --width: 40%\"></div>\n");
}

TEST(html_figure, the_alt_is_escaped_and_not_smartened)
{
	ratio_table const ratios{{"/x.svg", "1/1"}};
	EXPECT_EQ(render_figures("[!a \"b\" & it's...](/x.svg)\n"sv, ratios),
	          svg_div({"a &quot;b&quot; &amp; it's...", "/x.svg", "1/1"}));
}

TEST(html_figure, the_target_and_ratio_are_escaped_for_the_attribute)
{
	namespace ast = jimd::ast;
	ratio_table const ratios{{"/a&b\"<>.svg", "1\"/2"}};
	ast::document doc;
	doc.blocks = {paragraph_of(image_node("k"sv, "/a&b\"<>.svg"sv))};
	EXPECT_EQ(jimd::html::render(doc, {.image_ratios = &ratios}),
	          svg_div({"k", "/a&amp;b&quot;&lt;&gt;.svg", "1&quot;/2"}));
}

TEST(html_figure, blanks_around_the_image_are_dropped)
{
	namespace ast = jimd::ast;
	ratio_table const ratios{{"/x.svg", "1/1"}};
	ast::document doc;
	doc.blocks = {ast::paragraph{
		.children = {ast::text{"  "sv}, image_node("a"sv, "/x.svg"sv), ast::text{" \t"sv}}}};
	EXPECT_EQ(jimd::html::render(doc, {.image_ratios = &ratios}), svg_div({"a", "/x.svg", "1/1"}));
}

TEST(html_figure, a_figure_of_another_kind_of_image_is_a_paragraph)
{
	ratio_table const ratios{{"/x.svg", "1/1"}};
	EXPECT_EQ(render_figures("[!a](/x.png 75%)\n"sv, ratios),
	          "<p><img src=\"/x.png\" alt=\"a\" style=\"width: 75%\"></p>\n");
	for (auto const* const target : {"/x.SVG", "/x.Svg", "/x.svgz", "/x.svg?v=1", "/x.svg#a",
	                                 "/x.svg ", "/svg", "/x.svg.png"}) {
		ratio_table const all{{target, "1/1"}};
		auto const source = std::string{"[!a]("} + target + ")\n";
		auto const doc = parsed(source);
		auto const html = jimd::html::render(doc, {.image_ratios = &all});
		ASSERT_TRUE(html.has_value()) << target;
		EXPECT_TRUE(html->starts_with("<p><img src=\"")) << target << ": " << *html;
	}
}

TEST(html_figure, an_svg_image_with_other_content_is_an_image_in_a_paragraph)
{
	ratio_table const ratios{{"/x.svg", "1/1"}};
	EXPECT_EQ(render_figures("[!a](/x.svg) and text\n"sv, ratios),
	          "<p><img src=\"/x.svg\" alt=\"a\"> and text</p>\n");
	EXPECT_EQ(render_figures("[!a](/x.svg) [!b](/y.svg)\n"sv, ratios),
	          "<p><img src=\"/x.svg\" alt=\"a\"> <img src=\"/y.svg\" alt=\"b\"></p>\n");
	EXPECT_EQ(render_figures("text\n[!a](/x.svg)\n"sv, ratios),
	          "<p>text\n<img src=\"/x.svg\" alt=\"a\"></p>\n");
	EXPECT_EQ(render_figures("_[!a](/x.svg)_\n"sv, ratios),
	          "<p><em><img src=\"/x.svg\" alt=\"a\"></em></p>\n");
	EXPECT_TRUE(jimd::html::render(parsed("[!a](/x.svg) text\n"sv)).has_value());
}

TEST(html_figure, a_figure_written_normally_is_a_figure_whatever_its_blanks_and_line_ends)
{
	ratio_table const ratios{{"/x.svg", "1/1"}};
	auto const expected = svg_div({.alt = "a", .target = "/x.svg", .ratio = "1/1"});
	for (auto const source :
	     {"[!a](/x.svg)"sv, "[!a](/x.svg)\n"sv, "[!a](/x.svg)\r\n"sv, "[!a](/x.svg)\n\n\n"sv,
	      "[!a](/x.svg)\r\n\r\n"sv, "  [!a](/x.svg)  \n"sv, "\t[!a](/x.svg) \t\r\n"sv,
	      "\n\n[!a](/x.svg)\n"sv, "[!a](/x.svg)  \n\nnext"sv}) {
		auto const html = render_figures(source, ratios);
		ASSERT_TRUE(html.has_value()) << testing::PrintToString(source);
		EXPECT_TRUE(html->starts_with(expected)) << testing::PrintToString(source) << ": " << *html;
	}
	EXPECT_EQ(render_figures("[!a](/x.svg)\nnext\n"sv, ratios),
	          "<p><img src=\"/x.svg\" alt=\"a\">\nnext</p>\n");
}

TEST(html_figure, a_figure_in_a_quote_is_a_figure_and_in_a_list_or_a_note_is_not)
{
	ratio_table const ratios{{"/x.svg", "1/1"}};
	EXPECT_EQ(render_figures(">\n[!a](/x.svg)\n<\n"sv, ratios),
	          "<blockquote>\n" + svg_div({"a", "/x.svg", "1/1"}) + "</blockquote>\n");
	EXPECT_EQ(render_figures("- [!a](/x.svg)\n"sv, ratios),
	          "<ul>\n<li><img src=\"/x.svg\" alt=\"a\"></li>\n</ul>\n");
	EXPECT_EQ(render_figures("x[^n]([!a](/x.svg))\n"sv, ratios),
	          "<p>x" + note_ref(1) + "</p>\n" + note_aside(1, "<img src=\"/x.svg\" alt=\"a\">"));
}

TEST(html_figure, a_figure_forbids_what_could_end_the_url_or_the_attribute)
{
	namespace ast = jimd::ast;
	ratio_table const ratios;
	for (auto const bad : {'\'', '\\', '(', ')', ' ', '\t', '\n', '\r', '\x01', '\x1F', '\x7F'}) {
		auto const target = std::string{"/a"} + bad + "b.svg";
		ast::document doc;
		doc.blocks = {paragraph_of(image_node("k"sv, target))};
		EXPECT_EQ(jimd::html::render(doc, {.image_ratios = &ratios}),
		          std::unexpected(jimd::error{.code = jimd::errc::invalid_target}))
		    << static_cast<int>(bad);
	}
	ratio_table const all{{"/a\"&<%;#?\xC3\xA9.svg", "1/1"}};
	ast::document doc;
	doc.blocks = {paragraph_of(image_node("k"sv, "/a\"&<%;#?\xC3\xA9.svg"sv))};
	EXPECT_TRUE(jimd::html::render(doc, {.image_ratios = &all}).has_value());
}

TEST(html_figure, an_invalid_target_is_reported_at_the_first_bad_character)
{
	ratio_table const ratios;
	EXPECT_EQ(
		render_figures("[!a](/a'b'.svg)\n"sv, ratios),
		std::unexpected(jimd::error{.code = jimd::errc::invalid_target, .line = 1, .column = 8}));
	EXPECT_EQ(
		render_figures("[!a]\n\n[!a]: /ab c.svg\n"sv, ratios),
		std::unexpected(jimd::error{.code = jimd::errc::invalid_target, .line = 3, .column = 10}));
	EXPECT_EQ(
		render_figures("x\n\n[!a](/a(b).svg 50%)\n"sv, ratios),
		std::unexpected(jimd::error{.code = jimd::errc::invalid_target, .line = 3, .column = 8}));
}

TEST(html_figure, the_positions_count_a_crlf_source_as_one_line_end)
{
	ratio_table const ratios{{"/ok.svg", "1/1"}};
	EXPECT_EQ(
		render_figures("x\r\n\r\n[!a](/a'.svg)\r\n"sv, ratios),
		std::unexpected(jimd::error{.code = jimd::errc::invalid_target, .line = 3, .column = 8}));
	EXPECT_EQ(
		render_figures("[!a]\r\n\r\n[!a]: /a(.svg\r\n"sv, ratios),
		std::unexpected(jimd::error{.code = jimd::errc::invalid_target, .line = 3, .column = 9}));
	EXPECT_EQ(render_figures("x\r\n\r\n[!ab](/x.svg)\r\n"sv, ratios),
	          std::unexpected(
				  jimd::error{.code = jimd::errc::unknown_image_size, .line = 3, .column = 3}));
}

TEST(html_figure, only_an_svg_figure_has_its_target_checked)
{
	ratio_table const ratios;
	EXPECT_EQ(render_figures("[!a](/a'b.png)\n"sv, ratios),
	          "<p><img src=\"/a'b.png\" alt=\"a\"></p>\n");
	EXPECT_EQ(render_figures("[!a](/a'b.svg) text\n"sv, ratios),
	          "<p><img src=\"/a'b.svg\" alt=\"a\"> text</p>\n");
}

TEST(html_figure, a_missing_ratio_is_reported_at_the_images_key)
{
	EXPECT_EQ(jimd::html::render(parsed("[!a](/x.svg)\n"sv)),
	          std::unexpected(
				  jimd::error{.code = jimd::errc::unknown_image_size, .line = 1, .column = 3}));
	ratio_table const other{{"/y.svg", "1/1"}, {"/x.svg2", "1/1"}};
	EXPECT_EQ(render_figures("x\n\n[!ab](/x.svg)\n"sv, other),
	          std::unexpected(
				  jimd::error{.code = jimd::errc::unknown_image_size, .line = 3, .column = 3}));
	EXPECT_EQ(render_figures("[!a]\n\n[!a]: /x.svg\n"sv, other),
	          std::unexpected(
				  jimd::error{.code = jimd::errc::unknown_image_size, .line = 1, .column = 3}));
	ratio_table const none;
	EXPECT_EQ(render_figures("[!a](/x.svg)\n"sv, none),
	          std::unexpected(
				  jimd::error{.code = jimd::errc::unknown_image_size, .line = 1, .column = 3}));
}

TEST(html_figure, the_target_is_checked_before_the_ratio_and_the_first_figure_in_the_page_wins)
{
	ratio_table const ratios{{"/ok.svg", "1/1"}};
	EXPECT_EQ(
		render_figures("[!a](/a'.svg)\n"sv, ratios),
		std::unexpected(jimd::error{.code = jimd::errc::invalid_target, .line = 1, .column = 8}));
	EXPECT_EQ(render_figures("[!a](/x.svg)\n\n[!b](/b'.svg)\n"sv, ratios),
	          std::unexpected(
				  jimd::error{.code = jimd::errc::unknown_image_size, .line = 1, .column = 3}));
	EXPECT_EQ(
		render_figures("[!a](/a'.svg)\n\n[!b](/x.svg)\n"sv, ratios),
		std::unexpected(jimd::error{.code = jimd::errc::invalid_target, .line = 1, .column = 8}));
}

TEST(html_figure, an_unresolved_figure_is_an_undefined_reference)
{
	namespace ast = jimd::ast;
	ast::document doc;
	doc.blocks = {paragraph_of(ast::image{
		.key = "gone"sv, .alt = "gone"sv, .target = std::nullopt, .width = std::nullopt})};
	EXPECT_EQ(jimd::html::render(doc),
	          std::unexpected(jimd::error{.code = jimd::errc::undefined_reference}));
}

TEST(html_figure, the_typography_after_a_figure_starts_afresh)
{
	ratio_table const ratios{{"/x.svg", "1/1"}};
	EXPECT_EQ(render_figures("x'\n\n[!a](/x.svg)\n\n'y'\n"sv, ratios),
	          "<p>x’</p>\n" + svg_div({"a", "/x.svg", "1/1"}) + "<p>‘y’</p>\n");
}

TEST(html_svg_figures, are_the_svg_figures_in_source_order_with_their_resolved_targets)
{
	std::string_view const source = "[!one](/a.svg)\n\n"
									"[!two](/b.png)\n\n"
									">\n[!three](/c.svg 50%)\n\n>\n[!four]\n<\n<\n\n"
									"- [!item](/d.svg)\n\n"
									"[!five](/e.svg) text\n\n"
									"[!six]\n\n"
									"[!four]: /f.svg\n[!six]: /g.svg 10%\n";
	auto const doc = parsed(source);
	auto const figures = jimd::html::svg_figures(doc);
	auto const targets = std::ranges::to<std::vector>(
		figures | std::views::transform(&jimd::html::svg_figure::target));
	EXPECT_EQ(targets, (std::vector<std::string_view>{"/a.svg", "/c.svg", "/f.svg", "/g.svg"}));
	ASSERT_EQ(figures.size(), 4U);
	EXPECT_EQ(figures.at(0).key, "one");
	EXPECT_EQ(figures.at(0).key.data(), source.substr(source.find("one")).data());
	EXPECT_EQ(figures.at(2).key, "four");
	EXPECT_EQ(figures.at(2).key.data(), source.substr(source.find("[!four]\n<") + 2).data());
	EXPECT_EQ(figures.at(3).key, "six");
}

TEST(html_svg_figures, are_exactly_the_figures_that_render_writes_as_a_div)
{
	auto const doc = parsed(">\n[!a](/a.svg)\n>\n[!b]\n<\n[!c](/c.svg) t\n<\n\n"
	                        "- [!d](/d.svg)\n\n[!e](/e.png)\n\n  [!f](/f.svg) \n\n"
	                        "x[^n]([!g](/g.svg))\n\n[!h](/h.svg)\n[!i](/i.svg)\n\n"
	                        "[!b]: /b.svg\n");
	auto const figures = jimd::html::svg_figures(doc);
	ratio_table ratios;
	for (auto const& figure : figures) {
		ratios.emplace(std::string{figure.target}, "1/1");
	}
	auto const html = jimd::html::render(doc, {.image_ratios = &ratios});
	ASSERT_TRUE(html.has_value());
	std::size_t divs = 0;
	for (auto at = html->find("<div"); at != std::string::npos; at = html->find("<div", at + 1)) {
		++divs;
	}
	EXPECT_EQ(divs, figures.size());
	auto const targets = std::ranges::to<std::vector>(
		figures | std::views::transform(&jimd::html::svg_figure::target));
	EXPECT_EQ(targets, (std::vector<std::string_view>{"/a.svg", "/b.svg", "/f.svg"}));
}

TEST(html_svg_figures, are_empty_for_a_document_without_any_and_skip_a_hand_built_unresolved_one)
{
	namespace ast = jimd::ast;
	EXPECT_TRUE(jimd::html::svg_figures(parsed("Text\n\n[!a](/x.png)\n"sv)).empty());
	ast::document doc;
	doc.blocks = {paragraph_of(ast::image{
		.key = "gone"sv, .alt = "gone"sv, .target = std::nullopt, .width = std::nullopt})};
	EXPECT_TRUE(jimd::html::svg_figures(doc).empty());
	EXPECT_TRUE(jimd::html::svg_figures(ast::document{}).empty());
}

TEST(html_svg_figures, are_what_the_ratios_are_looked_up_by)
{
	auto const doc = parsed("[!a]\n\n[!b](/b.svg)\n\n[!a]: /a.svg 20%\n");
	ratio_table ratios;
	for (auto const& figure : jimd::html::svg_figures(doc)) {
		ratios.emplace(std::string{figure.target}, "4/3");
	}
	EXPECT_EQ(jimd::html::render(doc, {.image_ratios = &ratios}),
	          svg_div({"a", "/a.svg", "4/3", "20%"}) + svg_div({"b", "/b.svg", "4/3"}));
}
