#include <array>
#include <cstddef>
#include <expected>
#include <string>
#include <string_view>
#include <utility>
#include <gtest/gtest.h>

#include <highlight/hljs.hxx>

namespace
{
auto make_engine() -> highlight::hljs
{
	auto engine = highlight::hljs::create();
	EXPECT_TRUE(engine.has_value());
	return std::move(engine).value();
}

auto render_ok(highlight::hljs& engine, std::string_view language, std::string_view code)
	-> std::string
{
	auto const result = engine.render(language, code);
	EXPECT_TRUE(result.has_value()) << language;
	if (!result.has_value()) {
		return {};
	}
	return *result;
}

auto render_error(highlight::hljs& engine, std::string_view language, std::string_view code)
	-> highlight::error
{
	auto const result = engine.render(language, code);
	EXPECT_FALSE(result.has_value()) << language;
	if (result.has_value()) {
		return {};
	}
	return result.error();
}

// Longer than a regexp of highlight.js's can match without running out of memory.
constexpr std::size_t identifier_size = 3'000'000;

// The languages of the bundle's listLanguages(), in the README's list.
constexpr std::array<std::string_view, 36> bundled{
	"bash",   "c",          "cpp",          "csharp",    "css",        "diff",
	"go",     "graphql",    "ini",          "java",      "javascript", "json",
	"kotlin", "less",       "lua",          "makefile",  "markdown",   "objectivec",
	"perl",   "php",        "php-template", "plaintext", "python",     "python-repl",
	"r",      "ruby",       "rust",         "scss",      "shell",      "sql",
	"swift",  "typescript", "vbnet",        "wasm",      "xml",        "yaml",
};
}

TEST(hljs, wraps_keywords_and_strings_of_cpp_in_spans)
{
	auto engine = make_engine();
	auto const html = render_ok(engine, "cpp", "return \"x\";");
	EXPECT_NE(html.find("<span class=\"hljs-keyword\">return</span>"), std::string::npos) << html;
	EXPECT_NE(html.find("<span class=\"hljs-string\">&quot;x&quot;</span>"), std::string::npos)
	    << html;
}

TEST(hljs, escapes_angle_brackets_and_ampersands)
{
	auto engine = make_engine();
	auto const html = render_ok(engine, "cpp", "if (a < b && c > d) {}");
	EXPECT_NE(html.find("a &lt; b &amp;&amp; c &gt; d"), std::string::npos) << html;
	EXPECT_EQ(html.find("a < b"), std::string::npos) << html;
	EXPECT_EQ(html.find("&&"), std::string::npos) << html;
}

TEST(hljs, escapes_what_looks_like_markup_in_a_string_of_cpp)
{
	auto engine = make_engine();
	auto const html = render_ok(engine, "cpp", "auto s = \"<script>alert(1)</script>\";");
	EXPECT_EQ(html.find("<script>"), std::string::npos) << html;
	EXPECT_NE(html.find("&lt;script&gt;"), std::string::npos) << html;
}

TEST(hljs, renders_an_alias_like_its_language)
{
	auto engine = make_engine();
	auto const code = std::string_view{"class A { int x; };"};
	auto const expected = render_ok(engine, "cpp", code);
	EXPECT_NE(expected.find("hljs-keyword"), std::string::npos) << expected;
	EXPECT_EQ(render_ok(engine, "c++", code), expected);
	EXPECT_EQ(render_ok(engine, "cc", code), expected);
	EXPECT_EQ(render_ok(engine, "hpp", code), expected);
}

TEST(hljs, ignores_the_case_of_a_language)
{
	auto engine = make_engine();
	EXPECT_EQ(render_ok(engine, "C++", "int x;"), render_ok(engine, "cpp", "int x;"));
}

TEST(hljs, reports_an_unknown_language)
{
	auto engine = make_engine();
	EXPECT_EQ(render_error(engine, "cobol", "int x;"),
	          highlight::error{.message = "unknown language"});
}

TEST(hljs, reports_an_empty_language_as_unknown)
{
	auto engine = make_engine();
	EXPECT_EQ(render_error(engine, "", "int x;"), highlight::error{.message = "unknown language"});
}

TEST(hljs, reports_a_language_that_is_not_in_the_bundle_as_unknown)
{
	auto engine = make_engine();
	// If the bundle changes, so does the list of languages in deps/thirdparty/README.md.
	EXPECT_EQ(render_error(engine, "haskell", "main = pure ()").message, "unknown language");
	EXPECT_EQ(render_error(engine, "lisp", "(f)").message, "unknown language");
}

TEST(hljs, reports_names_of_object_properties_as_unknown_languages)
{
	auto engine = make_engine();
	for (auto const* name : {"__proto__", "constructor", "toString", "hasOwnProperty"}) {
		EXPECT_EQ(render_error(engine, name, "x").message, "unknown language") << name;
	}
}

TEST(hljs, does_not_detect_the_language)
{
	auto engine = make_engine();
	auto const error = render_error(engine, "auto", "#include <vector>\nint main() {}");
	EXPECT_EQ(error.message, "unknown language");
}

TEST(hljs, renders_an_empty_body)
{
	auto engine = make_engine();
	EXPECT_EQ(render_ok(engine, "cpp", ""), "");
	EXPECT_EQ(render_ok(engine, "text", ""), "");
}

TEST(hljs, gives_the_same_output_each_time)
{
	auto engine = make_engine();
	auto const code = std::string_view{"#include <vector>\nint main() { return 0; } // done\n"};
	auto const first = render_ok(engine, "cpp", code);
	EXPECT_EQ(render_ok(engine, "cpp", code), first);
	EXPECT_EQ(render_ok(engine, "python", "print(1)"), render_ok(engine, "python", "print(1)"));
	EXPECT_EQ(render_ok(engine, "cpp", code), first);

	auto other = make_engine();
	EXPECT_EQ(render_ok(other, "cpp", code), first);
}

TEST(hljs, leaves_plain_text_escaped_and_unhighlighted)
{
	auto engine = make_engine();
	auto const code = std::string_view{"<a href=\"x\">if (a) return 'b' && 1;</a>"};
	auto const expected = std::string_view{
		"&lt;a href=&quot;x&quot;&gt;if (a) return &#x27;b&#x27; &amp;&amp; 1;&lt;/a&gt;"};
	EXPECT_EQ(render_ok(engine, "text", code), expected);
	EXPECT_EQ(render_ok(engine, "plaintext", code), expected);
	EXPECT_EQ(render_ok(engine, "txt", code), expected);
}

TEST(hljs, keeps_newlines_and_non_ascii_text)
{
	auto engine = make_engine();
	EXPECT_EQ(render_ok(engine, "text", "a\n\n\tb\r\n\xC3\xA9 \xE2\x9C\x93 \xF0\x9F\x98\x80\n"),
	          "a\n\n\tb\r\n\xC3\xA9 \xE2\x9C\x93 \xF0\x9F\x98\x80\n");
	auto const html = render_ok(engine, "cpp", "// \xC3\xA9 \xF0\x9F\x98\x80\n");
	EXPECT_NE(html.find("\xC3\xA9 \xF0\x9F\x98\x80"), std::string::npos) << html;
}

TEST(hljs, highlights_every_language_of_the_bundle)
{
	// Compiling a grammar is where highlight.js writes to `console`, which the engine lacks,
	// and where it swallows an error if it can: this proves neither happens.
	auto engine = make_engine();
	for (auto const language : bundled) {
		auto const result = engine.render(language, "x = 1 < 2 && \"s\"; // c\n");
		ASSERT_TRUE(result.has_value()) << language << ": " << result.error().message;
		EXPECT_EQ(result->find("< 2"), std::string::npos) << language << ": " << *result;
		EXPECT_EQ(result->find("&&"), std::string::npos) << language << ": " << *result;
	}
}

TEST(hljs, reports_an_error_inside_highlight_js_instead_of_plain_text)
{
	// highlight.js would swallow this one and return the text unhighlighted.
	auto engine = make_engine();
	auto const error = render_error(engine, "cpp", std::string(identifier_size, 'a'));
	EXPECT_NE(error.message.find("out of memory"), std::string::npos) << error.message;
}

TEST(hljs, renders_again_after_a_failure)
{
	auto engine = make_engine();
	auto const code = std::string_view{"int x;"};
	auto const expected = render_ok(engine, "cpp", code);
	render_error(engine, "cobol", code);
	EXPECT_EQ(render_ok(engine, "cpp", code), expected);
	render_error(engine, "cpp", std::string(identifier_size, 'a'));
	EXPECT_EQ(render_ok(engine, "cpp", code), expected);
}

TEST(hljs, renders_after_a_move)
{
	auto engine = make_engine();
	auto moved = std::move(engine);
	EXPECT_TRUE(moved.render("cpp", "int x;").has_value());

	auto assigned = make_engine();
	assigned = std::move(moved);
	EXPECT_TRUE(assigned.render("cpp", "int x;").has_value());
}

TEST(hljs, reports_a_moved_from_engine_as_an_error)
{
	auto engine = make_engine();
	auto const moved = std::move(engine);
	// NOLINTNEXTLINE(bugprone-use-after-move,clang-analyzer-cplusplus.Move)
	auto const result = engine.render("cpp", "int x;");
	ASSERT_FALSE(result.has_value());
	EXPECT_EQ(result.error().message, "the engine was moved from");
}
