#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <gtest/gtest.h>

namespace
{
namespace fs = std::filesystem;

auto site(std::string const& name) -> fs::path
{
	// NOLINTBEGIN(concurrency-mt-unsafe)
	char const* const srcdir = std::getenv("TEST_SRCDIR");
	char const* const workspace = std::getenv("TEST_WORKSPACE");
	// NOLINTEND(concurrency-mt-unsafe)
	if (srcdir == nullptr || workspace == nullptr) {
		return {};
	}
	return fs::path{srcdir} / workspace / "tests" / "www" / name;
}

auto read(fs::path const& path) -> std::string
{
	std::ifstream file{path, std::ios::binary};
	std::ostringstream text;
	text << file.rdbuf();
	return text.str();
}
}

TEST(www, a_site_with_no_posts_has_an_empty_index_an_archive_with_a_heading_and_the_static_files)
{
	auto const dir = site("empty_site");
	ASSERT_TRUE(fs::is_directory(dir)) << dir;

	EXPECT_NE(read(dir / "index.html").find("<main>\n</main>\n"), std::string::npos);

	auto const archive = read(dir / "archive/index.html");
	EXPECT_NE(archive.find("<h1>Archive</h1>"), std::string::npos);
	EXPECT_EQ(archive.find("<table"), std::string::npos);
	EXPECT_NE(read(dir / "404.html").find("<h1>Page not found</h1>"), std::string::npos);

	EXPECT_EQ(read(dir / "hello.txt"), "hello\n");
	EXPECT_EQ(read(dir / "style/main.css"),
	          "/* A stand-in stylesheet for the rule's tests. */\nbody { margin: 0; }\n");
}

TEST(www, the_output_of_the_site_is_only_the_pages_the_css_and_the_static_files)
{
	auto const dir = site("empty_site");
	ASSERT_TRUE(fs::is_directory(dir)) << dir;
	std::size_t count = 0;
	for (auto const& entry : fs::recursive_directory_iterator{dir}) {
		count += entry.is_regular_file() ? 1 : 0;
	}
	EXPECT_EQ(count, 5U);
}

TEST(www, a_site_with_content_is_built_from_links_to_the_sources)
{
	auto const dir = site("small_site");
	ASSERT_TRUE(fs::is_directory(dir)) << dir;

	auto const post = read(dir / "2026/01/02/hello/index.html");
	EXPECT_NE(post.find("<h1>Hello</h1>"), std::string::npos);
	EXPECT_NE(post.find("<annotation encoding=\"application/x-tex\">x^2</annotation>"),
	          std::string::npos);
	EXPECT_NE(post.find("--ratio: 4/3"), std::string::npos);
	EXPECT_NE(read(dir / "about/index.html").find("About this site."), std::string::npos);
	EXPECT_NE(read(dir / "index.html").find("All 1 post…"), std::string::npos);
	EXPECT_NE(read(dir / "index.html").find("<a href=\"/about/\">About</a>"), std::string::npos);
	EXPECT_EQ(read(dir / "image/d.svg"), "<svg viewBox=\"0 0 4 3\"></svg>\n");
}
