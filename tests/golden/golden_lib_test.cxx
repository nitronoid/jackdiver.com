#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include <golden/golden.hxx>
#include <highlight/hljs.hxx>
#include <math/temml.hxx>

using namespace std::literals;

namespace
{
void write(std::filesystem::path const& path, std::string_view content)
{
	std::ofstream file{path, std::ios::binary};
	file << content;
}

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

class golden_files : public testing::Test {
protected:
	void SetUp() override
	{
		dir_ = std::filesystem::path{testing::TempDir()} /
		       testing::UnitTest::GetInstance()->current_test_info()->name();
		std::filesystem::remove_all(dir_);
		std::filesystem::create_directories(dir_);
	}

	[[nodiscard]] auto dir() const -> std::filesystem::path const&
	{
		return dir_;
	}

private:
	std::filesystem::path dir_;
};
}

TEST(first_difference, equal_text_has_none)
{
	EXPECT_EQ(golden::first_difference("a\nb\n", "a\nb\n"), std::nullopt);
	EXPECT_EQ(golden::first_difference("", ""), std::nullopt);
}

TEST(first_difference, names_the_line_the_column_and_the_byte)
{
	auto const diff = golden::first_difference("one\ntwo\nthree\n", "one\ntwo\nthrEE\n");

	ASSERT_TRUE(diff.has_value());
	EXPECT_EQ(diff.value(), "first difference at line 3, column 4 (byte 11)\n"
	                        "  expected: \"three\\n\"\n"
	                        "  actual:   \"thrEE\\n\"\n"
	                        "  (lengths: expected 14, actual 14)"s);
}

TEST(first_difference, shows_a_missing_final_newline)
{
	auto const diff = golden::first_difference("a\n", "a");

	ASSERT_TRUE(diff.has_value());
	EXPECT_EQ(diff.value(), "first difference at line 1, column 2 (byte 1)\n"
	                        "  expected: \"a\\n\"\n"
	                        "  actual:   \"a\"\n"
	                        "  (lengths: expected 2, actual 1)"s);
}

TEST(first_difference, shows_a_carriage_return)
{
	auto const diff = golden::first_difference("a\nb\n", "a\r\nb\n");

	ASSERT_TRUE(diff.has_value());
	EXPECT_EQ(diff.value(), "first difference at line 1, column 2 (byte 1)\n"
	                        "  expected: \"a\\n\"\n"
	                        "  actual:   \"a\\r\\n\"\n"
	                        "  (lengths: expected 4, actual 5)"s);
}

TEST(first_difference, one_side_ending_early_is_the_end_of_file)
{
	auto const longer = golden::first_difference("a\n", "a\nb\n");
	auto const shorter = golden::first_difference("a\nb\n", "a\n");

	ASSERT_TRUE(longer.has_value());
	ASSERT_TRUE(shorter.has_value());
	EXPECT_EQ(longer.value(), "first difference at line 2, column 1 (byte 2)\n"
	                          "  expected: <end of file>\n"
	                          "  actual:   \"b\\n\"\n"
	                          "  (lengths: expected 2, actual 4)"s);
	EXPECT_EQ(shorter.value(), "first difference at line 2, column 1 (byte 2)\n"
	                           "  expected: \"b\\n\"\n"
	                           "  actual:   <end of file>\n"
	                           "  (lengths: expected 4, actual 2)"s);
}

TEST(first_difference, escapes_quotes_backslashes_and_control_characters)
{
	auto const diff = golden::first_difference("x"s, "\"\\\t\x01\x7f"s);

	ASSERT_TRUE(diff.has_value());
	EXPECT_EQ(diff.value(), "first difference at line 1, column 1 (byte 0)\n"
	                        "  expected: \"x\"\n"
	                        "  actual:   \"\\\"\\\\\\t\\x01\\x7f\"\n"
	                        "  (lengths: expected 1, actual 5)"s);
}

TEST(first_difference, leaves_utf8_readable)
{
	auto const diff = golden::first_difference("caf\xc3\xa9", "cafe");

	ASSERT_TRUE(diff.has_value());
	EXPECT_EQ(diff.value(), "first difference at line 1, column 4 (byte 3)\n"
	                        "  expected: \"caf\xc3\xa9\"\n"
	                        "  actual:   \"cafe\"\n"
	                        "  (lengths: expected 5, actual 4)"s);
}

TEST_F(golden_files, inputs_are_the_jimd_files_in_name_order)
{
	write(dir() / "b.jimd", "");
	write(dir() / "a.jimd", "");
	write(dir() / "a.html", "");
	write(dir() / "notes.txt", "");
	std::filesystem::create_directory(dir() / "directory.jimd");

	EXPECT_EQ(golden::inputs(dir()),
	          (std::vector<std::filesystem::path>{dir() / "a.jimd", dir() / "b.jimd"}));
}

TEST_F(golden_files, orphans_are_the_html_files_with_no_jimd)
{
	write(dir() / "paired.jimd", "");
	write(dir() / "paired.html", "");
	write(dir() / "stray.html", "");
	write(dir() / "directory.html", "");
	std::filesystem::create_directory(dir() / "directory.jimd");

	EXPECT_EQ(golden::orphans(dir()),
	          (std::vector<std::filesystem::path>{dir() / "directory.html", dir() / "stray.html"}));
}

TEST_F(golden_files, the_expected_output_sits_next_to_the_input)
{
	EXPECT_EQ(golden::expected_path(dir() / "page.jimd"), dir() / "page.html");
}

TEST_F(golden_files, read_file_keeps_every_byte)
{
	write(dir() / "crlf.jimd", "a\r\nb\r\n\0c"s);

	EXPECT_EQ(golden::read_file(dir() / "crlf.jimd"), "a\r\nb\r\n\0c"s);
}

TEST_F(golden_files, write_file_writes_every_byte_and_makes_the_directories)
{
	EXPECT_TRUE(golden::write_file(dir() / "a" / "b" / "crlf.html", "a\r\nb\r\n\0c"s));
	EXPECT_EQ(golden::read_file(dir() / "a" / "b" / "crlf.html"), "a\r\nb\r\n\0c"s);
	EXPECT_TRUE(golden::write_file(dir() / "a" / "b" / "crlf.html", "new"));
	EXPECT_EQ(golden::read_file(dir() / "a" / "b" / "crlf.html"), "new");
	EXPECT_FALSE(std::filesystem::exists(dir() / "a" / "b" / "crlf.html.tmp"));
}

TEST_F(golden_files, write_file_replaces_a_link_and_does_not_write_through_it)
{
	write(dir() / "target", "kept");
	std::filesystem::create_symlink(dir() / "target", dir() / "link.html");
	std::filesystem::create_symlink(dir() / "target", dir() / "other.html.tmp");
	EXPECT_TRUE(golden::write_file(dir() / "link.html", "x"));
	EXPECT_TRUE(golden::write_file(dir() / "other.html", "y"));
	EXPECT_EQ(golden::read_file(dir() / "target"), "kept");
	EXPECT_EQ(golden::read_file(dir() / "link.html"), "x");
	EXPECT_EQ(golden::read_file(dir() / "other.html"), "y");
}

TEST_F(golden_files, write_file_fails_where_it_cannot_write)
{
	std::filesystem::create_directory(dir() / "directory");
	EXPECT_FALSE(golden::write_file(dir() / "directory", "x"));
	EXPECT_TRUE(std::filesystem::is_directory(dir() / "directory"));
	write(dir() / "file", "x");
	EXPECT_FALSE(golden::write_file(dir() / "file" / "inside", "x"));
}

TEST_F(golden_files, read_file_refuses_what_is_not_a_readable_file)
{
	std::filesystem::create_directory(dir() / "directory");

	EXPECT_EQ(golden::read_file(dir() / "directory"), std::nullopt);
	EXPECT_EQ(golden::read_file(dir() / "missing"), std::nullopt);
}

TEST_F(golden_files, render_gives_a_fragment_and_not_a_page)
{
	write(dir() / "my.fragment.jimd", "[%title]: Metadata\n\nJust text.\n");

	auto const fragment = golden::render(dir() / "my.fragment.jimd", engine(), highlighter());

	ASSERT_TRUE(fragment.has_value());
	EXPECT_EQ(*fragment, "<p>Just text.</p>\n");
}

TEST_F(golden_files, render_names_the_input_and_the_position_of_a_syntax_error)
{
	auto const input = dir() / "unclosed.jimd";
	write(input, "```\nnever closed");

	auto const fragment = golden::render(input, engine(), highlighter());

	ASSERT_FALSE(fragment.has_value());
	EXPECT_EQ(fragment.error(), input.string() + ":2:13: unclosed code block"s);
}

TEST_F(golden_files, render_names_the_input_and_the_position_of_a_reference_error)
{
	auto const input = dir() / "bad.jimd";
	write(input, "fine\n\nsee [^nothing]\n");

	auto const fragment = golden::render(input, engine(), highlighter());

	ASSERT_FALSE(fragment.has_value());
	EXPECT_EQ(fragment.error(), input.string() + ":3:7: undefined reference"s);
}

TEST_F(golden_files, render_reports_a_missing_input)
{
	auto const input = dir() / "missing.jimd";

	auto const fragment = golden::render(input, engine(), highlighter());

	ASSERT_FALSE(fragment.has_value());
	EXPECT_EQ(fragment.error(), input.string() + ": read failed"s);
}

TEST_F(golden_files, render_renders_the_maths)
{
	write(dir() / "m.jimd", "Say $x^2$.\n");

	auto const fragment = golden::render(dir() / "m.jimd", engine(), highlighter());

	ASSERT_TRUE(fragment.has_value());
	EXPECT_NE(fragment->find("<p>Say <math"), std::string::npos);
}

TEST_F(golden_files, render_names_the_input_and_the_position_of_a_maths_error)
{
	auto const input = dir() / "bad_math.jimd";
	write(input, "fine\n\nsee $x + \\foo$\n");

	auto const fragment = golden::render(input, engine(), highlighter());

	ASSERT_FALSE(fragment.has_value());
	EXPECT_EQ(fragment.error(),
	          input.string() + ":3:10: invalid maths: Unsupported function name: \\foo"s);
}

TEST_F(golden_files, render_reads_a_figure_under_the_directory_of_the_input)
{
	std::filesystem::create_directories(dir() / "image");
	write(dir() / "image" / "d.svg", "<svg viewBox=\"0 0 30 20\"></svg>");
	write(dir() / "f.jimd", "[!D](/image/d.svg 50%)\n");

	auto const fragment = golden::render(dir() / "f.jimd", engine(), highlighter());

	ASSERT_TRUE(fragment.has_value());
	EXPECT_NE(fragment->find("url('/image/d.svg'); --ratio: 30/20; --width: 50%"),
	          std::string::npos);
}

TEST_F(golden_files, render_names_the_input_and_the_place_of_a_figure_error)
{
	auto const input = dir() / "bad_figure.jimd";
	write(input, "fine\n\n[!D](/image/none.svg)\n");

	auto const fragment = golden::render(input, engine(), highlighter());

	ASSERT_FALSE(fragment.has_value());
	EXPECT_EQ(fragment.error(), input.string() + ":3:3: file not found: /image/none.svg"s);
}
