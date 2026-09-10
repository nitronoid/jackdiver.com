#include <gtest/gtest.h>

#include <flat_map>
#include <string_view>
#include <vector>

#include <jimd/ast.hxx>
#include <support/ast_printers.hxx>
#include <support/dump.hxx>

using namespace std::literals;

TEST(dump, empty_document)
{
	jimd::ast::document doc{};
	EXPECT_EQ(support::dump(doc), "document\n");
}

TEST(dump, document_snapshot)
{
	jimd::ast::document doc{};
	doc.defs.links["site"sv] = "https://example.com"sv;
	doc.defs.images["logo"sv] = {.target = "logo.png"sv, .width = "50%"sv};
	doc.defs.images["plain"sv] = {.target = "plain.png"sv, .width = std::nullopt};
	doc.defs.footnotes["fn"sv] = {jimd::ast::text{.value = "A note"sv}};

	doc.blocks.emplace_back(jimd::ast::heading{
		.level = 1,
		.children = {jimd::ast::text{.value = "Title"sv}},
	});

	doc.blocks
		.emplace_back(
			jimd::ast::paragraph{
				.children =
					{
						jimd::ast::text{.value = "Hello "sv},
						jimd::ast::emphasis{.children = {jimd::ast::text{.value = "world"sv}}},
						jimd::ast::strong{.children = {jimd::ast::text{.value = "bold"sv}}},
						jimd::ast::text{.value = "!"sv},
						jimd::ast::code_span{.value = "code"sv},
						jimd::ast::soft_break{},
						jimd::ast::link{
							.key = "ref"sv,
							.text = "ref"sv,
							.target = "https://ref.com"sv,
						},
						jimd::ast::link{
							.key = "site"sv,
							.text = "site"sv,
							.target = std::nullopt,
						},
						jimd::ast::image{
							.key = "logo"sv,
							.alt = "logo"sv,
							.target = std::nullopt,
							.width = std::nullopt,
						},
						jimd::ast::image{
							.key = "pic"sv,
							.alt = "pic"sv,
							.target = "pic.png"sv,
							.width = "75%"sv,
						},
						jimd::ast::image{
							.key = "bare"sv,
							.alt = "bare"sv,
							.target = "bare.png"sv,
							.width = std::nullopt,
						},
						jimd::ast::reference{
							.key = "fn"sv,
							.content = std::nullopt,
						},
						jimd::ast::math{.tex = "x^2 \\alpha"sv},
					},
			});

	doc.blocks.emplace_back(jimd::ast::quote{
		.children =
			{
				jimd::ast::paragraph{
					.children = {jimd::ast::text{.value = "Quote line"sv}},
				},
			},
	});

	doc.blocks.emplace_back(jimd::ast::code_block{
		.info = "cpp"sv,
		.body = "int x = 1;\n"sv,
	});

	doc.blocks.emplace_back(jimd::ast::code_block{
		.info = ""sv,
		.body = "plain block\n"sv,
	});

	doc.blocks.emplace_back(jimd::ast::list{
		.items =
			{
				{jimd::ast::text{.value = "first"sv}},
				{jimd::ast::text{.value = "second "sv},
				 jimd::ast::emphasis{.children = {jimd::ast::text{.value = "item"sv}}}},
			},
	});

	doc.blocks.emplace_back(jimd::ast::display_math{.tex = "a\\b\nc\n"sv});

	doc.blocks.emplace_back(jimd::ast::aligned_math{.rows = {"a &= b"sv, ""sv}});

	constexpr auto expected = "document\n"
							  "  defs\n"
							  "    link \"site\": \"https://example.com\"\n"
							  "    image \"logo\": \"logo.png\" width=\"50%\"\n"
							  "    image \"plain\": \"plain.png\"\n"
							  "    footnote \"fn\":\n"
							  "      text \"A note\"\n"
							  "  heading level=1\n"
							  "    text \"Title\"\n"
							  "  paragraph\n"
							  "    text \"Hello \"\n"
							  "    emphasis\n"
							  "      text \"world\"\n"
							  "    strong\n"
							  "      text \"bold\"\n"
							  "    text \"!\"\n"
							  "    code_span \"code\"\n"
							  "    soft_break\n"
							  "    link key=\"ref\" text=\"ref\" target=\"https://ref.com\"\n"
							  "    link key=\"site\" text=\"site\"\n"
							  "    image key=\"logo\" alt=\"logo\"\n"
							  "    image key=\"pic\" alt=\"pic\" target=\"pic.png\" width=\"75%\"\n"
							  "    image key=\"bare\" alt=\"bare\" target=\"bare.png\"\n"
							  "    reference key=\"fn\"\n"
							  "    math \"x^2 \\\\alpha\"\n"
							  "  quote\n"
							  "    paragraph\n"
							  "      text \"Quote line\"\n"
							  "  code_block info=\"cpp\" \"int x = 1;\\n\"\n"
							  "  code_block \"plain block\\n\"\n"
							  "  list\n"
							  "    item\n"
							  "      text \"first\"\n"
							  "    item\n"
							  "      text \"second \"\n"
							  "      emphasis\n"
							  "        text \"item\"\n"
							  "  display_math \"a\\\\b\\nc\\n\"\n"
							  "  aligned_math\n"
							  "    row \"a &= b\"\n"
							  "    row \"\"\n";

	EXPECT_EQ(support::dump(doc), expected);
}

TEST(dump, individual_nodes)
{
	jimd::ast::paragraph const para{
		.children = {jimd::ast::text{.value = "text in para"sv}},
	};
	EXPECT_EQ(support::dump(para), "paragraph\n  text \"text in para\"\n");

	jimd::ast::code_block const block{
		.info = ""sv,
		.body = "hello\n"sv,
	};
	EXPECT_EQ(support::dump(block), "code_block \"hello\\n\"\n");

	jimd::ast::list const items{.items = {{jimd::ast::text{.value = "a"sv}}, {}}};
	EXPECT_EQ(support::dump(items), "list\n  item\n    text \"a\"\n  item\n");

	jimd::ast::text const txt{.value = "single text"sv};
	EXPECT_EQ(support::dump(txt), "text \"single text\"\n");

	jimd::ast::display_math const display{.tex = "x\n"sv};
	EXPECT_EQ(support::dump(display), "display_math \"x\\n\"\n");

	jimd::ast::aligned_math const aligned{.rows = {"x"sv}};
	EXPECT_EQ(support::dump(aligned), "aligned_math\n  row \"x\"\n");

	jimd::ast::math const tex{.tex = "a_b"sv};
	EXPECT_EQ(support::dump(tex), "math \"a_b\"\n");

	jimd::ast::reference const ref{
		.key = "note"sv,
		.content = jimd::ast::inlines{jimd::ast::text{.value = "content"sv}},
	};
	EXPECT_EQ(support::dump(ref), "reference key=\"note\"\n  text \"content\"\n");
}

TEST(dump, gtest_print_to)
{
	jimd::ast::document doc{};
	doc.blocks.emplace_back(jimd::ast::paragraph{
		.children = {jimd::ast::text{.value = "sample"sv}},
	});

	EXPECT_EQ(testing::PrintToString(doc), support::dump(doc));

	jimd::ast::paragraph const para{
		.children = {jimd::ast::text{.value = "p"sv}},
	};
	EXPECT_EQ(testing::PrintToString(para), support::dump(para));

	jimd::ast::list const items{.items = {{jimd::ast::text{.value = "i"sv}}}};
	EXPECT_EQ(testing::PrintToString(items), support::dump(items));

	jimd::ast::text const txt{.value = "t"sv};
	EXPECT_EQ(testing::PrintToString(txt), support::dump(txt));

	jimd::ast::math const tex{.tex = "m"sv};
	EXPECT_EQ(testing::PrintToString(tex), support::dump(tex));

	jimd::ast::display_math const display{.tex = "d\n"sv};
	EXPECT_EQ(testing::PrintToString(display), support::dump(display));

	jimd::ast::aligned_math const aligned{.rows = {"r"sv}};
	EXPECT_EQ(testing::PrintToString(aligned), support::dump(aligned));

	jimd::ast::resources defs{};
	defs.links["k"sv] = "v"sv;
	EXPECT_EQ(testing::PrintToString(defs), support::dump(defs));
}
