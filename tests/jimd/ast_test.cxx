#include <gtest/gtest.h>

#include <string_view>

#include <jimd/ast.hxx>
#include <support/ast_printers.hxx>

using namespace std::literals;

TEST(ast, inline_equality)
{
	namespace ast = jimd::ast;

	EXPECT_EQ(ast::text{"hello"sv}, ast::text{"hello"sv});
	EXPECT_NE(ast::text{"hello"sv}, ast::text{"world"sv});

	EXPECT_EQ(ast::soft_break{}, ast::soft_break{});

	EXPECT_EQ(ast::code_span{"x = 1"sv}, ast::code_span{"x = 1"sv});
	EXPECT_NE(ast::code_span{"x = 1"sv}, ast::code_span{"y = 2"sv});

	EXPECT_EQ((ast::link{"key"sv, "text"sv, "url"sv}), (ast::link{"key"sv, "text"sv, "url"sv}));
	EXPECT_NE((ast::link{"key"sv, "text"sv, "url"sv}),
	          (ast::link{"key"sv, "text"sv, std::nullopt}));

	EXPECT_EQ((ast::image{"key"sv, "alt"sv, "img.png"sv, std::nullopt}),
	          (ast::image{"key"sv, "alt"sv, "img.png"sv, std::nullopt}));
	EXPECT_NE((ast::image{"key"sv, "alt"sv, "img.png"sv, std::nullopt}),
	          (ast::image{"key"sv, "other"sv, "img.png"sv, std::nullopt}));

	EXPECT_EQ((ast::image{"key"sv, "alt"sv, "img.png"sv, "75%"sv}),
	          (ast::image{"key"sv, "alt"sv, "img.png"sv, "75%"sv}));
	EXPECT_NE((ast::image{"key"sv, "alt"sv, "img.png"sv, "75%"sv}),
	          (ast::image{"key"sv, "alt"sv, "img.png"sv, "50%"sv}));
	EXPECT_NE((ast::image{"key"sv, "alt"sv, "img.png"sv, "75%"sv}),
	          (ast::image{"key"sv, "alt"sv, "img.png"sv, std::nullopt}));

	EXPECT_EQ((ast::image_def{"img.png"sv, "75%"sv}), (ast::image_def{"img.png"sv, "75%"sv}));
	EXPECT_NE((ast::image_def{"img.png"sv, "75%"sv}), (ast::image_def{"img.png"sv, std::nullopt}));
	EXPECT_NE((ast::image_def{"img.png"sv, "75%"sv}), (ast::image_def{"other.png"sv, "75%"sv}));

	EXPECT_EQ(ast::math{"x^2"sv}, ast::math{"x^2"sv});
	EXPECT_NE(ast::math{"x^2"sv}, ast::math{"x^3"sv});

	EXPECT_EQ((ast::reference{"k"sv, std::nullopt}), (ast::reference{"k"sv, std::nullopt}));
	EXPECT_NE((ast::reference{"k"sv, std::nullopt}),
	          (ast::reference{"k"sv, ast::inlines{ast::text{"footnote content"sv}}}));
}

TEST(ast, nested_inlines)
{
	using namespace jimd::ast;

	emphasis const emp1{.children = {strong{.children = {text{"nested"sv}}}}};
	emphasis const emp2{.children = {strong{.children = {text{"nested"sv}}}}};
	emphasis const emp3{.children = {strong{.children = {text{"different"sv}}}}};

	EXPECT_EQ(emp1, emp2);
	EXPECT_NE(emp1, emp3);

	strong const str1{.children = {emphasis{.children = {code_span{"code"sv}}}}};
	strong const str2{.children = {emphasis{.children = {code_span{"code"sv}}}}};
	strong const str3{.children = {text{"not nested"sv}}};

	EXPECT_EQ(str1, str2);
	EXPECT_NE(str1, str3);
}

TEST(ast, block_equality)
{
	using namespace jimd::ast;

	paragraph const para{.children = {text{"hello"sv}, soft_break{}, text{"world"sv}}};
	paragraph const same_para{.children = {text{"hello"sv}, soft_break{}, text{"world"sv}}};
	paragraph const other_para{.children = {text{"hello"sv}}};

	EXPECT_EQ(para, same_para);
	EXPECT_NE(para, other_para);

	heading const head{.level = 1, .children = {text{"Title"sv}}};
	heading const same_head{.level = 1, .children = {text{"Title"sv}}};
	heading const other_head{.level = 2, .children = {text{"Title"sv}}};

	EXPECT_EQ(head, same_head);
	EXPECT_NE(head, other_head);

	code_block const code{.info = "cpp"sv, .body = "int x = 0;\n"sv};
	code_block const same_code{.info = "cpp"sv, .body = "int x = 0;\n"sv};
	code_block const other_code{.info = ""sv, .body = "int x = 0;\n"sv};

	EXPECT_EQ(code, same_code);
	EXPECT_NE(code, other_code);

	list const items{.items = {{text{"a"sv}}, {text{"b"sv}, soft_break{}}}};
	list const same_items{.items = {{text{"a"sv}}, {text{"b"sv}, soft_break{}}}};
	list const other_items{.items = {{text{"a"sv}}, {text{"b"sv}}}};
	list const fewer_items{.items = {{text{"a"sv}}}};

	EXPECT_EQ(items, same_items);
	EXPECT_NE(items, other_items);
	EXPECT_NE(items, fewer_items);

	display_math const display{.tex = "x\n"sv};
	EXPECT_EQ(display, display_math{.tex = "x\n"sv});
	EXPECT_NE(display, display_math{.tex = "y\n"sv});
	EXPECT_NE(block{display}, block{list{}});

	aligned_math const aligned{.rows = {"a"sv, ""sv}};
	EXPECT_EQ(aligned, (aligned_math{.rows = {"a"sv, ""sv}}));
	EXPECT_NE(aligned, (aligned_math{.rows = {"a"sv}}));
	EXPECT_NE(aligned, (aligned_math{.rows = {"a"sv, " "sv}}));
	EXPECT_NE(block{aligned}, block{display});
	EXPECT_NE(block{items}, block{paragraph{}});
}

TEST(ast, nested_blocks)
{
	using namespace jimd::ast;

	paragraph const para{.children = {text{"quoted text"sv}}};

	quote const q_inner{.children = {para}};
	quote const quoted{.children = {q_inner}};
	quote const same_quoted{
		.children = {quote{.children = {paragraph{.children = {text{"quoted text"sv}}}}}}};
	quote const other_quoted{
		.children = {quote{.children = {paragraph{.children = {text{"other"sv}}}}}}};

	EXPECT_EQ(quoted, same_quoted);
	EXPECT_NE(quoted, other_quoted);
}

TEST(ast, document_and_resources)
{
	using namespace jimd::ast;

	document doc1{
		.source = "content"sv,
		.metadata = {{"title"sv, "My Document"sv}},
		.blocks = {heading{.level = 1, .children = {text{"Title"sv}}},
		           paragraph{.children = {text{"Para"sv}}}},
		.defs =
			{
				.links = {{"k1"sv, "https://example.com"sv}},
				.images = {{"logo"sv, image_def{.target = "/logo.png"sv, .width = std::nullopt}}},
				.footnotes = {{"1"sv, {text{"Note"sv}}}},
			},
	};

	document doc2 = doc1;
	EXPECT_EQ(doc1, doc2);

	doc2.metadata["title"sv] = "Other Document"sv;
	EXPECT_NE(doc1, doc2);
}

TEST(ast_math_tex, inline_math_is_its_tex_as_written)
{
	using namespace jimd::ast;

	EXPECT_EQ(math_tex(math{"x_1^2 \\frac{a}{b}"sv}), "x_1^2 \\frac{a}{b}");
	EXPECT_EQ(math_tex(math{""sv}), "");
}

TEST(ast_math_tex, display_math_turns_each_crlf_into_lf)
{
	using namespace jimd::ast;

	EXPECT_EQ(math_tex(display_math{""sv}), "");
	EXPECT_EQ(math_tex(display_math{"a\nb\n"sv}), "a\nb\n");
	EXPECT_EQ(math_tex(display_math{"a\r\nb\r\n"sv}), "a\nb\n");
	EXPECT_EQ(math_tex(display_math{"a\rb\r\r\nc\r"sv}), "a\rb\r\nc\r");
}

TEST(ast_math_tex, aligned_math_is_wrapped_in_align_star_with_a_line_break_between_rows)
{
	using namespace jimd::ast;

	EXPECT_EQ(math_tex(aligned_math{.rows = {"x"sv}}), "\\begin{align*}\nx\n\\end{align*}");
	EXPECT_EQ(math_tex(aligned_math{.rows = {"a &= b"sv, "c"sv, "d"sv}}),
	          "\\begin{align*}\na &= b\n\\\\\nc\n\\\\\nd\n\\end{align*}");
}

TEST(ast_math_tex, aligned_math_rows_are_joined_as_they_are)
{
	using namespace jimd::ast;

	EXPECT_EQ(math_tex(aligned_math{.rows = {"a"sv, ""sv, " "sv, "b % c"sv}}),
	          "\\begin{align*}\na\n\\\\\n\n\\\\\n \n\\\\\nb % c\n\\end{align*}");
}

TEST(ast_plain_text, empty)
{
	EXPECT_EQ(jimd::ast::plain_text({}), "");
	EXPECT_EQ(jimd::ast::plain_text({jimd::ast::emphasis{}, jimd::ast::strong{}}), "");
}

TEST(ast_plain_text, text_and_code_span_are_kept_verbatim)
{
	using namespace jimd::ast;

	EXPECT_EQ(plain_text({text{"a < b "sv}, code_span{"*x*"sv}}), "a < b *x*");
}

TEST(ast_plain_text, soft_break_is_a_space)
{
	using namespace jimd::ast;

	EXPECT_EQ(plain_text({text{"one"sv}, soft_break{}, text{"two"sv}}), "one two");
}

TEST(ast_plain_text, recurses_into_emphasis_and_strong)
{
	using namespace jimd::ast;

	inlines const nodes{
		text{"a "sv},
		emphasis{.children = {text{"b "sv}, strong{.children = {text{"c"sv}}},
		                      emphasis{.children = {code_span{"d"sv}}}}},
		text{" e"sv},
	};
	EXPECT_EQ(plain_text(nodes), "a b cd e");
}

TEST(ast_plain_text, math_is_its_tex)
{
	using namespace jimd::ast;

	EXPECT_EQ(plain_text({text{"E = "sv}, math{"mc^2"sv}, strong{.children = {math{"x"sv}}}}),
	          "E = mc^2x");
}

TEST(ast_plain_text, link_image_and_reference)
{
	using namespace jimd::ast;

	inlines const nodes{
		jimd::ast::link{.key = "k1"sv, .text = "site text"sv, .target = std::nullopt},
		text{" "sv},
		image{.key = "k2"sv, .alt = "logo alt"sv, .target = "/logo.png"sv, .width = std::nullopt},
		reference{.key = "k3"sv, .content = inlines{text{"note text"sv}}},
		reference{.key = "k4"sv, .content = std::nullopt},
	};
	EXPECT_EQ(plain_text(nodes), "site text logo alt");
}
