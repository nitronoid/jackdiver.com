#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>
#include <gtest/gtest.h>

#include <blog_golden/golden_tree.hxx>

namespace
{
namespace fs = std::filesystem;

class trees {
public:
	trees()
		: scratch_{make_scratch()}
	{
	}

	auto expected(std::string_view rel, std::string_view text) const -> void
	{
		put(root() / "expected" / rel, text);
	}

	auto actual(std::string_view rel, std::string_view text) const -> void
	{
		put(root() / "actual" / rel, text);
	}

	[[nodiscard]] auto compare() const -> std::vector<std::string>
	{
		return blog_golden::compare(root() / "expected", root() / "actual");
	}

	[[nodiscard]] auto root() const -> fs::path
	{
		return scratch_.path();
	}

private:
	static auto make_scratch() -> blog_golden::scratch_dir
	{
		auto made = blog_golden::scratch_dir::create(::testing::TempDir(), "golden_tree");
		if (!made) {
			throw std::runtime_error(made.error());
		}
		return std::move(*made);
	}

	static auto put(fs::path const& path, std::string_view text) -> void
	{
		fs::create_directories(path.parent_path());
		std::ofstream file{path, std::ios::binary};
		file << text;
	}

	blog_golden::scratch_dir scratch_;
};
}

TEST(golden_tree, equal_trees_have_no_differences)
{
	trees const files;
	files.expected("index.html", "a\n");
	files.expected("x/y/index.html", "b\r\n");
	files.actual("index.html", "a\n");
	files.actual("x/y/index.html", "b\r\n");
	EXPECT_TRUE(files.compare().empty());
}

TEST(golden_tree, two_empty_or_missing_trees_are_equal)
{
	trees const files;
	EXPECT_TRUE(files.compare().empty());
}

TEST(golden_tree, a_file_only_the_build_writes_is_reported)
{
	trees const files;
	files.expected("a.html", "a");
	files.actual("a.html", "a");
	files.actual("new/b.html", "b");
	auto const problems = files.compare();
	ASSERT_EQ(problems.size(), 1U);
	EXPECT_EQ(problems.front(), "new/b.html: the build writes it and there is no expected file");
}

TEST(golden_tree, an_expected_file_the_build_does_not_write_is_reported)
{
	trees const files;
	files.expected("a.html", "a");
	files.expected("old/b.html", "b");
	files.actual("a.html", "a");
	auto const problems = files.compare();
	ASSERT_EQ(problems.size(), 1U);
	EXPECT_EQ(problems.front(), "old/b.html: the build does not write it");
}

TEST(golden_tree, a_file_that_differs_is_reported_with_the_place)
{
	trees const files;
	files.expected("a.html", "one\ntwo\nthree\n");
	files.actual("a.html", "one\ntwo!\nthree\n");
	files.expected("same.html", "same");
	files.actual("same.html", "same");
	auto const problems = files.compare();
	ASSERT_EQ(problems.size(), 1U);
	EXPECT_TRUE(problems.front().starts_with("a.html: differs from the expected file\n"));
	EXPECT_NE(problems.front().find("line 2"), std::string::npos);
}

TEST(golden_tree, a_line_end_and_a_missing_final_newline_differ)
{
	trees const files;
	files.expected("crlf.html", "a\r\nb\n");
	files.actual("crlf.html", "a\nb\n");
	files.expected("end.html", "a\n");
	files.actual("end.html", "a");
	EXPECT_EQ(files.compare().size(), 2U);
}

TEST(golden_tree, files_under_lists_nested_regular_files_in_order)
{
	trees const files;
	files.expected("b/z.html", "");
	files.expected("a.html", "");
	files.expected("b/a/c.html", "");
	EXPECT_EQ(blog_golden::files_under(files.root() / "expected"),
	          (std::vector<std::string>{"a.html", "b/a/c.html", "b/z.html"}));
	EXPECT_TRUE(blog_golden::files_under(files.root() / "nowhere").empty());
}

TEST(golden_tree,
     files_under_takes_a_link_to_a_file_as_a_file_and_does_not_enter_a_linked_directory)
{
	trees const files;
	files.expected("real/a.html", "a");
	files.expected("b.html", "b");
	auto const dir = files.root() / "expected";
	std::error_code error;
	fs::create_symlink(dir / "b.html", dir / "link.html", error);
	ASSERT_FALSE(error) << error.message();
	fs::create_directory_symlink(dir / "real", dir / "linked_dir", error);
	ASSERT_FALSE(error) << error.message();
	EXPECT_EQ(blog_golden::files_under(dir),
	          (std::vector<std::string>{"b.html", "link.html", "real/a.html"}));
}

TEST(golden_tree, a_scratch_directory_is_new_and_empty_and_is_removed_with_its_content)
{
	trees const files;
	fs::path path;
	{
		auto first = blog_golden::scratch_dir::create(files.root() / "base", "x");
		auto second = blog_golden::scratch_dir::create(files.root() / "base", "x");
		ASSERT_TRUE(first.has_value() && second.has_value());
		EXPECT_NE(first->path(), second->path());
		EXPECT_TRUE(fs::is_empty(first->path()));
		EXPECT_EQ(first->path().parent_path(), files.root() / "base");
		path = first->path();
		std::ofstream{path / "file"} << "x";
		auto moved = std::move(*first);
		EXPECT_EQ(moved.path(), path);
	}
	EXPECT_FALSE(fs::exists(path));
}
