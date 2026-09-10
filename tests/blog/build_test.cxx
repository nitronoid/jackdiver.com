#include <algorithm>
#include <cstddef>
#include <expected>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <map>
#include <sstream>
#include <string>
#include <string_view>
#include <variant>
#include <vector>
#include <unistd.h>
#include <gtest/gtest.h>

#include <blog/build.hxx>
#include <blog/error.hxx>
#include <highlight/hljs.hxx>
#include <jimd/position.hxx>
#include <math/temml.hxx>
#include <support/result_printers.hxx>

using namespace std::literals;
namespace fs = std::filesystem;

namespace
{
auto engine() -> math::temml&
{
	static auto instance = math::temml::create();
	return *instance;
}

auto highlighter() -> highlight::hljs&
{
	static auto instance = highlight::hljs::create();
	return *instance;
}

class tree {
public:
	tree()
		: root_{fs::path{::testing::TempDir()} /
		        ("blog_" +
		         std::string{::testing::UnitTest::GetInstance()->current_test_info()->name()})}
	{
		fs::remove_all(root_);
		fs::create_directories(content());
		write("style.css", "body { color: red; }\n");
	}

	tree(tree const&) = delete;
	tree(tree&&) = delete;
	auto operator=(tree const&) -> tree& = delete;
	auto operator=(tree&&) -> tree& = delete;

	~tree()
	{
		std::error_code ignored;
		fs::remove_all(root_, ignored);
	}

	[[nodiscard]] auto content() const -> fs::path
	{
		return root_ / "content";
	}
	[[nodiscard]] auto out() const -> fs::path
	{
		return root_ / "out";
	}

	// NOLINTNEXTLINE(bugprone-easily-swappable-parameters)
	auto write(std::string_view rel, std::string_view text) const -> void
	{
		auto const path = root_ / rel;
		fs::create_directories(path.parent_path());
		std::ofstream file{path, std::ios::binary};
		file << text;
	}

	// NOLINTNEXTLINE(bugprone-easily-swappable-parameters)
	auto put(std::string_view rel, std::string_view text) const -> void
	{
		write("content/" + std::string{rel}, text);
	}

	[[nodiscard]] auto path(std::string_view rel) const -> fs::path
	{
		return root_ / rel;
	}

	[[nodiscard]] auto build() const -> std::expected<void, std::vector<blog::file_error>>
	{
		return blog::build({.content = content(), .css = root_ / "style.css", .output = out()},
		                   engine(), highlighter());
	}

private:
	fs::path root_;
};

auto read(fs::path const& path) -> std::string
{
	std::ifstream file{path, std::ios::binary};
	std::ostringstream text;
	text << file.rdbuf();
	return text.str();
}

auto files_under(fs::path const& dir) -> std::vector<std::string>
{
	std::vector<std::string> out;
	if (!fs::exists(dir)) {
		return out;
	}
	for (auto const& entry : fs::recursive_directory_iterator{dir}) {
		if (entry.is_regular_file()) {
			out.push_back(entry.path().lexically_relative(dir).generic_string());
		}
	}
	std::ranges::sort(out);
	return out;
}

auto code_of(blog::file_error const& err) -> blog::errc
{
	return std::get<blog::errc>(err.error.code);
}

using strings = std::vector<std::string>;

constexpr auto post_a = "[%title]: A\n\nBody of A.\n"sv;
}

TEST(build, a_clean_build_writes_every_page_the_css_and_the_static_files)
{
	tree const files;
	files.put("posts/2026-09-03-a.jimd", "[%title]: A\n[%series]: s\n\nBody of A.\n");
	files.put("posts/2026-09-04-b.jimd", "[%title]: B\n[%series]: s\n\nBody of B.\n");
	files.put("pages/about.jimd", "[%title]: About\n\nHello.\n");
	files.put("static/font/f.bin", "\x01\x02\r\n\xff");
	files.put("static/theme.js", "var x = 1;\n");
	files.put("static/.well-known/security.txt", "hi\n");
	auto const built = files.build();
	ASSERT_TRUE(built.has_value()) << built.error().size();
	EXPECT_EQ(files_under(files.out()),
	          (strings{".well-known/security.txt", "2026/09/03/a/index.html",
	                   "2026/09/04/b/index.html", "404.html", "about/index.html",
	                   "archive/index.html", "font/f.bin", "index.html", "style/main.css",
	                   "theme.js"}));
	EXPECT_EQ(read(files.out() / "style/main.css"), "body { color: red; }\n");
	EXPECT_EQ(read(files.out() / "font/f.bin"), "\x01\x02\r\n\xff");
	EXPECT_EQ(read(files.out() / "theme.js"), "var x = 1;\n");

	auto const post = read(files.out() / "2026/09/03/a/index.html");
	EXPECT_NE(post.find("<p>Body of A.</p>"), std::string::npos);
	EXPECT_NE(post.find("Part 1 of 2 in a series"), std::string::npos);
	auto const index = read(files.out() / "index.html");
	EXPECT_NE(index.find("<p>Body of B.</p>"), std::string::npos);
	EXPECT_EQ(index.find("Body of A."), std::string::npos);
	EXPECT_NE(index.find("All 2 posts…"), std::string::npos);
	EXPECT_NE(read(files.out() / "archive/index.html").find("/2026/09/03/a/"), std::string::npos);
	EXPECT_NE(read(files.out() / "about/index.html").find("<p>Hello.</p>"), std::string::npos);
	EXPECT_NE(index.find("<a href=\"/about/\">About</a>"), std::string::npos);
}

TEST(build, a_site_with_nothing_in_it_has_an_empty_index_and_archive)
{
	tree const files;
	auto const built = files.build();
	ASSERT_TRUE(built.has_value());
	EXPECT_EQ(files_under(files.out()),
	          (strings{"404.html", "archive/index.html", "index.html", "style/main.css"}));
	EXPECT_NE(read(files.out() / "index.html").find("<main>\n</main>"), std::string::npos);
	EXPECT_EQ(read(files.out() / "archive/index.html").find("<table"), std::string::npos);
}

TEST(build, maths_and_figures_are_rendered_with_the_static_root)
{
	tree const files;
	files.put("posts/2026-09-03-a.jimd",
	          "[%title]: A\n\nInline $x^2$.\n\n$$\ny = 2x\n$$\n\n[!Diagram](/image/d.svg 50%)\n");
	files.put("static/image/d.svg", "<svg viewBox=\"0 0 300 200\"></svg>");
	auto const built = files.build();
	ASSERT_TRUE(built.has_value());
	auto const post = read(files.out() / "2026/09/03/a/index.html");
	EXPECT_NE(post.find("<annotation encoding=\"application/x-tex\">x^2</annotation>"),
	          std::string::npos);
	EXPECT_NE(post.find("<math display=\"block\""), std::string::npos);
	EXPECT_NE(post.find("--ratio: 300/200; --width: 50%"), std::string::npos);
	EXPECT_TRUE(fs::exists(files.out() / "image/d.svg"));
}

TEST(build, code_is_highlighted_in_posts_and_pages)
{
	tree const files;
	files.put("posts/2026-09-03-a.jimd", "[%title]: A\n\n```cpp\nreturn;\n```\n");
	files.put("pages/about.jimd", "[%title]: About\n\n```sh\nexit\n```\n");
	auto const built = files.build();
	ASSERT_TRUE(built.has_value());
	EXPECT_NE(read(files.out() / "2026/09/03/a/index.html")
	              .find("<pre class=\"highlight\"><code class=\"language-cpp\">"
	                    "<span class=\"hljs-keyword\">return</span>;"),
	          std::string::npos);
	EXPECT_NE(read(files.out() / "about/index.html").find("<span class=\"hljs-built_in\">exit"),
	          std::string::npos);
}

TEST(build, an_unknown_language_is_an_error_of_its_file)
{
	tree const files;
	files.put("posts/2026-09-03-a.jimd", "[%title]: A\n\n```lisp\n(a)\n```\n");
	auto const built = files.build();
	ASSERT_FALSE(built.has_value());
	ASSERT_EQ(built.error().size(), 1U);
	EXPECT_EQ(built.error().front().path, files.content() / "posts/2026-09-03-a.jimd");
	EXPECT_EQ(
		built.error().front().error,
		(blog::error{blog::errc::invalid_code, {.line = 3, .column = 4}, "unknown language"}));
	EXPECT_FALSE(fs::exists(files.out()));
}

TEST(build, the_output_is_the_same_each_time)
{
	tree const files;
	files.put("posts/2026-09-03-a.jimd", "[%title]: A\n\nBody $x$.\n");
	files.put("pages/about.jimd", "[%title]: About\n");
	files.put("static/a.txt", "a");
	ASSERT_TRUE(files.build().has_value());
	auto const first_files = files_under(files.out());
	std::map<std::string, std::string> first;
	for (auto const& name : first_files) {
		first[name] = read(files.out() / name);
	}
	fs::remove_all(files.out());
	ASSERT_TRUE(files.build().has_value());
	EXPECT_EQ(files_under(files.out()), first_files);
	for (auto const& name : first_files) {
		EXPECT_EQ(read(files.out() / name), first[name]) << name;
	}
}

TEST(build, an_existing_empty_output_directory_is_used_and_a_missing_one_is_made)
{
	tree const files;
	fs::create_directories(files.out());
	ASSERT_TRUE(files.build().has_value());
	EXPECT_TRUE(fs::exists(files.out() / "index.html"));

	tree const nested;
	auto const built = blog::build({.content = nested.content(),
	                                .css = nested.path("style.css"),
	                                .output = nested.path("a/b/out")},
	                               engine(), highlighter());
	ASSERT_TRUE(built.has_value());
	EXPECT_TRUE(fs::exists(nested.path("a/b/out/index.html")));
}

TEST(build, a_dotfile_is_not_an_entry_and_a_directory_or_another_file_is)
{
	tree const files;
	files.put("posts/.DS_Store", "junk");
	files.put("posts/.2026-09-03-a.jimd.swp", "junk");
	files.put("posts/notes.txt", "[%title]: Notes\n");
	files.put("posts/2026-09-03-a.md", "[%title]: A\n");
	files.put("posts/drafts/2026-09-03-b.jimd", post_a);
	files.put("pages/.hidden.jimd", "junk");
	files.put("pages/About.jimd", "[%title]: About\n");
	auto const built = files.build();
	ASSERT_FALSE(built.has_value());
	auto const& errors = built.error();
	ASSERT_EQ(errors.size(), 4U);
	EXPECT_EQ(errors.at(0).path, files.content() / "pages/About.jimd");
	EXPECT_EQ(code_of(errors.at(0)), blog::errc::bad_page_name);
	EXPECT_EQ(errors.at(1).path, files.content() / "posts/2026-09-03-a.md");
	EXPECT_EQ(errors.at(1).error.detail, "the name must end in .jimd");
	EXPECT_EQ(errors.at(2).path, files.content() / "posts/drafts");
	EXPECT_EQ(code_of(errors.at(2)), blog::errc::bad_post_name);
	EXPECT_EQ(errors.at(2).error.detail, "not a file");
	EXPECT_EQ(errors.at(3).path, files.content() / "posts/notes.txt");
	EXPECT_EQ(errors.at(3).error.detail, "the name must end in .jimd");
}

TEST(build, a_file_that_is_not_jimd_is_not_parsed)
{
	tree const files;
	files.put("posts/readme.txt", "`unclosed\n");
	files.put("pages/about.md", "`unclosed\n");
	auto const built = files.build();
	ASSERT_FALSE(built.has_value());
	ASSERT_EQ(built.error().size(), 2U);
	EXPECT_EQ(built.error().at(0).path, files.content() / "pages/about.md");
	EXPECT_EQ(code_of(built.error().at(0)), blog::errc::bad_page_name);
	EXPECT_EQ(built.error().at(0).error.detail, "the name must end in .jimd");
	EXPECT_EQ(built.error().at(1).path, files.content() / "posts/readme.txt");
	EXPECT_EQ(code_of(built.error().at(1)), blog::errc::bad_post_name);
	EXPECT_EQ(built.error().at(1).error.detail, "the name must end in .jimd");
}

TEST(build, a_jimd_file_with_a_bad_name_is_reported_once_and_not_rendered)
{
	tree const files;
	files.put("posts/2026-02-30-x.jimd", "[%title]: X\n\n$\\baz$\n");
	files.put("pages/Bad_Name.jimd", "[%title]: Bad\n\n$\\baz$\n");
	auto const built = files.build();
	ASSERT_FALSE(built.has_value());
	ASSERT_EQ(built.error().size(), 2U);
	EXPECT_EQ(built.error().at(0).path, files.content() / "pages/Bad_Name.jimd");
	EXPECT_EQ(built.error().at(0).error,
	          (blog::error{blog::errc::bad_page_name, jimd::position{},
	                       "the slug must be lower-case letters and digits, joined by single "
	                       "hyphens"}));
	EXPECT_EQ(built.error().at(1).path, files.content() / "posts/2026-02-30-x.jimd");
	EXPECT_EQ(built.error().at(1).error, (blog::error{blog::errc::bad_post_name, jimd::position{},
	                                                  "2026-02-30 is not a date"}));
	EXPECT_FALSE(fs::exists(files.out()));
}

TEST(build, a_jimd_file_with_a_bad_name_and_a_parse_error_reports_the_parse_error)
{
	tree const files;
	files.put("posts/2026-02-30-x.jimd", "[%title]: X\n\n`unclosed\n");
	files.put("pages/Bad_Name.jimd", "[%title]: Bad\n\n`unclosed\n");
	auto const built = files.build();
	ASSERT_FALSE(built.has_value());
	ASSERT_EQ(built.error().size(), 2U);
	EXPECT_EQ(built.error().at(0).path, files.content() / "pages/Bad_Name.jimd");
	EXPECT_EQ(built.error().at(0).error.code,
	          (std::variant<jimd::errc, blog::errc>{jimd::errc::syntax}));
	EXPECT_EQ(built.error().at(1).path, files.content() / "posts/2026-02-30-x.jimd");
	EXPECT_EQ(built.error().at(1).error.code,
	          (std::variant<jimd::errc, blog::errc>{jimd::errc::syntax}));
}

TEST(build, a_bad_name_hides_the_static_collisions_like_any_other_fault_of_the_site)
{
	tree const files;
	files.put("posts/2026-09-03-a.jimd", post_a);
	files.put("posts/Bad_Name.jimd", post_a);
	files.put("static/index.html", "static");
	auto const built = files.build();
	ASSERT_FALSE(built.has_value());
	ASSERT_EQ(built.error().size(), 1U);
	EXPECT_EQ(built.error().front().path, files.content() / "posts/Bad_Name.jimd");
	EXPECT_EQ(code_of(built.error().front()), blog::errc::bad_post_name);
}

TEST(build, a_link_to_a_file_is_followed_and_a_broken_one_is_an_error)
{
	tree const files;
	files.write("elsewhere/2026-09-03-a.jimd", "[%title]: Linked\n\nBody.\n");
	fs::create_directories(files.content() / "posts");
	fs::create_symlink(files.path("elsewhere/2026-09-03-a.jimd"),
	                   files.content() / "posts/2026-09-03-a.jimd");
	ASSERT_TRUE(files.build().has_value());
	EXPECT_NE(read(files.out() / "2026/09/03/a/index.html").find("<h1>Linked</h1>"),
	          std::string::npos);

	fs::create_symlink(files.path("nowhere.jimd"), files.content() / "posts/2026-09-04-b.jimd");
	fs::remove_all(files.out());
	auto const broken = files.build();
	ASSERT_FALSE(broken.has_value());
	ASSERT_EQ(broken.error().size(), 1U);
	EXPECT_EQ(broken.error().front().path, files.content() / "posts/2026-09-04-b.jimd");
	EXPECT_EQ(code_of(broken.error().front()), blog::errc::bad_post_name);
}

TEST(build, the_static_directory_is_copied_as_real_files_following_file_links)
{
	tree const files;
	files.write("shared/real.txt", "real\n");
	files.put("static/.DS_Store", "junk");
	files.put("static/sub/.DS_Store", "junk");
	fs::create_directories(files.content() / "static");
	fs::create_symlink(files.path("shared/real.txt"), files.content() / "static/link.txt");
	ASSERT_TRUE(files.build().has_value());
	EXPECT_EQ(files_under(files.out()), (strings{"404.html", "archive/index.html", "index.html",
	                                             "link.txt", "style/main.css"}));
	EXPECT_FALSE(fs::is_symlink(files.out() / "link.txt"));
	EXPECT_EQ(read(files.out() / "link.txt"), "real\n");
}

TEST(build, a_broken_link_in_the_static_directory_is_an_error)
{
	tree const files;
	fs::create_directories(files.content() / "static");
	fs::create_symlink(files.path("nowhere"), files.content() / "static/gone.txt");
	auto const built = files.build();
	ASSERT_FALSE(built.has_value());
	ASSERT_EQ(built.error().size(), 1U);
	EXPECT_EQ(built.error().front().path, files.content() / "static/gone.txt");
	EXPECT_EQ(code_of(built.error().front()), blog::errc::read_failed);
	EXPECT_FALSE(fs::exists(files.out()));
}

TEST(build, a_linked_directory_in_the_static_directory_is_an_error_and_not_followed)
{
	tree const files;
	files.write("shared/inner.txt", "inner\n");
	files.put("static/a/f.txt", "f");
	fs::create_directory_symlink(files.path("shared"), files.content() / "static/linked");
	fs::create_directory_symlink("..", files.content() / "static/a/up");
	auto const built = files.build();
	ASSERT_FALSE(built.has_value());
	auto const& errors = built.error();
	ASSERT_EQ(errors.size(), 2U);
	EXPECT_EQ(errors.at(0).path, files.content() / "static/a/up");
	EXPECT_EQ(errors.at(1).path, files.content() / "static/linked");
	for (auto const& err : errors) {
		EXPECT_EQ(code_of(err), blog::errc::read_failed) << err.path;
	}
	EXPECT_FALSE(fs::exists(files.out()));
}

TEST(build, a_static_root_that_is_a_link_is_followed)
{
	tree const files;
	files.write("elsewhere/x.txt", "x");
	fs::create_directory_symlink(files.path("elsewhere"), files.content() / "static");
	ASSERT_TRUE(files.build().has_value());
	EXPECT_TRUE(fs::exists(files.out() / "x.txt"));
}

TEST(build, a_posts_that_is_not_a_directory_is_an_error_and_a_missing_one_is_not)
{
	tree const files;
	files.put("posts", "a file");
	auto const built = files.build();
	ASSERT_FALSE(built.has_value());
	ASSERT_EQ(built.error().size(), 1U);
	EXPECT_EQ(built.error().front().path, files.content() / "posts");
	EXPECT_EQ(code_of(built.error().front()), blog::errc::read_failed);
	EXPECT_FALSE(fs::exists(files.out()));
}

TEST(build, a_directory_that_cannot_be_listed_is_an_error)
{
	tree const files;
	files.put("posts/2026-09-03-a.jimd", post_a);
	files.put("static/a.txt", "a");
	auto const posts = files.content() / "posts";
	auto const stat = files.content() / "static";
	fs::permissions(posts, fs::perms::none);
	fs::permissions(stat, fs::perms::none);
	bool const listable = access(posts.c_str(), R_OK) == 0;
	auto const built = files.build();
	fs::permissions(posts, fs::perms::owner_all);
	fs::permissions(stat, fs::perms::owner_all);
	if (listable) {
		GTEST_SKIP() << "the directory can be read despite its mode";
	}
	ASSERT_FALSE(built.has_value());
	ASSERT_EQ(built.error().size(), 2U);
	EXPECT_EQ(built.error().at(0).path, posts);
	EXPECT_EQ(built.error().at(1).path, stat);
	EXPECT_EQ(code_of(built.error().at(0)), blog::errc::read_failed);
	EXPECT_EQ(code_of(built.error().at(1)), blog::errc::read_failed);
	EXPECT_FALSE(fs::exists(files.out()));
}

TEST(build, a_static_subdirectory_that_cannot_be_listed_is_an_error_on_it)
{
	tree const files;
	files.put("static/a.txt", "a");
	files.put("static/sub/b.txt", "b");
	auto const sub = files.content() / "static/sub";
	fs::permissions(sub, fs::perms::none);
	bool const listable = access(sub.c_str(), R_OK) == 0;
	auto const built = files.build();
	fs::permissions(sub, fs::perms::owner_all);
	if (listable) {
		GTEST_SKIP() << "the directory can be read despite its mode";
	}
	ASSERT_FALSE(built.has_value());
	ASSERT_EQ(built.error().size(), 1U);
	EXPECT_EQ(built.error().front().path, sub);
	EXPECT_EQ(code_of(built.error().front()), blog::errc::read_failed);
	EXPECT_FALSE(fs::exists(files.out()));
}

TEST(build, two_broken_posts_report_two_errors_and_write_nothing)
{
	tree const files;
	files.put("posts/2026-09-03-a.jimd", "[%title]: A\n\nFine.\n");
	files.put("posts/2026-09-04-b.jimd", "[%title]: B\n\n$\\baz$\n");
	files.put("posts/2026-09-05-c.jimd", "[%title]: C\n[%tags]: x\n");
	files.put("static/a.txt", "a");
	auto const built = files.build();
	ASSERT_FALSE(built.has_value());
	auto const& errors = built.error();
	ASSERT_EQ(errors.size(), 2U);
	EXPECT_EQ(errors.at(0).path, files.content() / "posts/2026-09-04-b.jimd");
	EXPECT_EQ(code_of(errors.at(0)), blog::errc::invalid_math);
	EXPECT_EQ(errors.at(0).error.line, 3U);
	EXPECT_EQ(errors.at(0).error.column, 2U);
	EXPECT_EQ(errors.at(1).path, files.content() / "posts/2026-09-05-c.jimd");
	EXPECT_EQ(code_of(errors.at(1)), blog::errc::unknown_metadata);
	EXPECT_EQ(errors.at(1).error.line, 2U);
	EXPECT_FALSE(fs::exists(files.out()));
}

TEST(build, a_fault_of_every_stage_is_reported_in_the_order_of_the_paths)
{
	tree const files;
	files.put("posts/2026-01-01-parse.jimd", "[%title]: P\n\n`unclosed\n");
	files.put("posts/2026-01-02-title.jimd", "Body.\n");
	files.put("posts/2026-01-03-math.jimd", "[%title]: M\n\n$\\baz$\n");
	files.put("posts/2026-01-04-figure.jimd", "[%title]: F\n\n[!Gone](/image/gone.svg)\n");
	files.put("posts/2026-02-30-date.jimd", "[%title]: D\n");
	files.put("posts/2026-01-05-good.jimd", "[%title]: G\n");
	files.put("pages/archive.jimd", "[%title]: Archive\n");
	files.put("pages/bad name.jimd", "[%title]: Bad\n");
	files.put("pages/good.jimd", "[%title]: Good\n");
	auto const built = files.build();
	ASSERT_FALSE(built.has_value());
	auto const& errors = built.error();
	ASSERT_EQ(errors.size(), 7U);
	EXPECT_EQ(errors.at(0).path.filename(), "archive.jimd");
	EXPECT_EQ(code_of(errors.at(0)), blog::errc::page_collision);
	EXPECT_EQ(errors.at(1).path.filename(), "bad name.jimd");
	EXPECT_EQ(code_of(errors.at(1)), blog::errc::bad_page_name);
	EXPECT_EQ(errors.at(2).path.filename(), "2026-01-01-parse.jimd");
	EXPECT_EQ(errors.at(2).error.code, (std::variant<jimd::errc, blog::errc>{jimd::errc::syntax}));
	EXPECT_EQ(errors.at(3).path.filename(), "2026-01-02-title.jimd");
	EXPECT_EQ(code_of(errors.at(3)), blog::errc::missing_title);
	EXPECT_EQ(errors.at(4).path.filename(), "2026-01-03-math.jimd");
	EXPECT_EQ(code_of(errors.at(4)), blog::errc::invalid_math);
	EXPECT_EQ(errors.at(5).path.filename(), "2026-01-04-figure.jimd");
	EXPECT_EQ(code_of(errors.at(5)), blog::errc::file_not_found);
	EXPECT_EQ(errors.at(5).error.detail, "/image/gone.svg");
	EXPECT_EQ(errors.at(6).path.filename(), "2026-02-30-date.jimd");
	EXPECT_EQ(code_of(errors.at(6)), blog::errc::bad_post_name);
	EXPECT_FALSE(fs::exists(files.out()));
}

TEST(build, the_errors_are_formatted_with_the_path_and_the_place)
{
	tree const files;
	files.put("posts/2026-09-04-b.jimd", "[%title]: B\n\nText $\\baz$ here.\n");
	auto const built = files.build();
	ASSERT_FALSE(built.has_value());
	auto const& err = built.error().front();
	EXPECT_EQ(blog::format_error(err.error, err.path.string()),
	          files.content().string() + "/posts/2026-09-04-b.jimd:3:7: invalid maths: "
	                                     "Unsupported function name: \\baz");
}

TEST(build, a_file_that_does_not_parse_is_not_rendered_too)
{
	tree const files;
	files.put("posts/2026-09-03-a.jimd", "[%title]: A\n\n`unclosed $\\baz$\n");
	auto const built = files.build();
	ASSERT_FALSE(built.has_value());
	ASSERT_EQ(built.error().size(), 1U);
}

TEST(build, the_other_files_are_still_rendered_for_their_errors)
{
	tree const files;
	files.put("posts/2026-09-03-a.jimd", "Body.\n");
	files.put("posts/2026-09-04-b.jimd", "[%title]: B\n\n$\\baz$\n");
	auto const built = files.build();
	ASSERT_FALSE(built.has_value());
	ASSERT_EQ(built.error().size(), 2U);
	EXPECT_EQ(code_of(built.error().at(0)), blog::errc::missing_title);
	EXPECT_EQ(code_of(built.error().at(1)), blog::errc::invalid_math);
}

TEST(build, a_missing_content_directory_or_stylesheet_is_an_error)
{
	tree const files;
	auto const no_content = blog::build(
		{.content = files.path("nowhere"), .css = files.path("style.css"), .output = files.out()},
		engine(), highlighter());
	ASSERT_FALSE(no_content.has_value());
	ASSERT_EQ(no_content.error().size(), 1U);
	EXPECT_EQ(no_content.error().front().path, files.path("nowhere"));
	EXPECT_EQ(code_of(no_content.error().front()), blog::errc::file_not_found);

	auto const no_css = blog::build(
		{.content = files.content(), .css = files.path("missing.css"), .output = files.out()},
		engine(), highlighter());
	ASSERT_FALSE(no_css.has_value());
	ASSERT_EQ(no_css.error().size(), 1U);
	EXPECT_EQ(no_css.error().front().path, files.path("missing.css"));
	EXPECT_EQ(code_of(no_css.error().front()), blog::errc::file_not_found);
	EXPECT_FALSE(fs::exists(files.out()));
}

TEST(build, a_non_empty_output_directory_is_refused_and_left_alone)
{
	tree const files;
	files.write("out/keep.txt", "keep");
	files.put("posts/2026-09-03-a.jimd", "Body.\n");
	auto const built = files.build();
	ASSERT_FALSE(built.has_value());
	ASSERT_EQ(built.error().size(), 1U);
	EXPECT_EQ(built.error().front().path, files.out());
	EXPECT_EQ(code_of(built.error().front()), blog::errc::output_not_empty);
	EXPECT_EQ(files_under(files.out()), (strings{"keep.txt"}));
}

TEST(build, an_output_that_is_a_file_is_refused)
{
	tree const files;
	files.write("out", "a file");
	auto const built = files.build();
	ASSERT_FALSE(built.has_value());
	EXPECT_EQ(code_of(built.error().front()), blog::errc::output_not_empty);
	EXPECT_EQ(read(files.out()), "a file");
}

TEST(build, a_second_build_into_the_same_directory_is_refused)
{
	tree const files;
	ASSERT_TRUE(files.build().has_value());
	auto const again = files.build();
	ASSERT_FALSE(again.has_value());
	EXPECT_EQ(code_of(again.error().front()), blog::errc::output_not_empty);
}

namespace
{
auto collision(std::string_view rel) -> blog::file_error
{
	tree const files;
	files.put("posts/2026-09-03-a.jimd", post_a);
	files.put("pages/font.jimd", "[%title]: Font\n");
	files.put(std::string{"static/"} + std::string{rel}, "static");
	auto const built = files.build();
	EXPECT_FALSE(built.has_value()) << rel;
	if (built.has_value()) {
		return {.path = {}, .error = blog::errc::usage};
	}
	EXPECT_EQ(built.error().size(), 1U) << rel;
	EXPECT_FALSE(fs::exists(files.out())) << rel;
	auto err = built.error().front();
	constexpr auto marker = "/static/"sv;
	auto const text = err.path.generic_string();
	err.path = text.substr(text.find(marker) + marker.size());
	return err;
}
}

TEST(build, a_static_file_at_a_path_the_site_writes_is_a_collision)
{
	struct example {
		std::string_view rel;
		std::string_view detail;
	};
	for (auto const& [rel, detail] : {
			 example{.rel = "index.html", .detail = "/index.html is also written by the site"},
			 example{.rel = "style/main.css",
			         .detail = "/style/main.css is also written by the site"},
			 example{.rel = "archive/index.html",
			         .detail = "/archive/index.html is also written by the site"},
			 example{.rel = "404.html", .detail = "/404.html is also written by the site"},
			 example{.rel = "2026/09/03/a/index.html",
			         .detail = "/2026/09/03/a/index.html is also written by the site"},
			 example{.rel = "font/index.html",
			         .detail = "/font/index.html is also written by the site"},
		 }) {
		auto const err = collision(rel);
		EXPECT_EQ(err.path, fs::path{rel}) << rel;
		EXPECT_EQ(err.error, (blog::error{blog::errc::static_collision, jimd::position{},
		                                  std::string{detail}}))
		    << rel;
	}
}

TEST(build, a_static_file_that_is_the_directory_of_a_written_file_is_a_collision)
{
	EXPECT_EQ(collision("archive").error.detail, "/archive is a directory of /archive/index.html");
	EXPECT_EQ(collision("2026").error.detail, "/2026 is a directory of /2026/09/03/a/index.html");
	EXPECT_EQ(collision("2026/09/03/a").error.detail,
	          "/2026/09/03/a is a directory of /2026/09/03/a/index.html");
	EXPECT_EQ(collision("style").error.detail, "/style is a directory of /style/main.css");
	EXPECT_EQ(collision("font").error.detail, "/font is a directory of /font/index.html");
}

TEST(build, a_static_file_inside_a_written_file_is_a_collision)
{
	EXPECT_EQ(collision("index.html/x.txt").error.detail,
	          "/index.html/x.txt is inside /index.html, a file the site writes");
	EXPECT_EQ(collision("style/main.css/x.txt").error.detail,
	          "/style/main.css/x.txt is inside /style/main.css, a file the site writes");
	EXPECT_EQ(collision("archive/index.html/a/b").error.detail,
	          "/archive/index.html/a/b is inside /archive/index.html, a file the site writes");
}

TEST(build, a_static_file_beside_the_written_ones_is_not_a_collision)
{
	tree const files;
	files.put("posts/2026-09-03-a.jimd", post_a);
	files.put("pages/font.jimd", "[%title]: Font\n");
	files.put("static/font/f.woff2", "f");
	files.put("static/style/extra.css", "e");
	files.put("static/archive/x.txt", "x");
	files.put("static/2026/09/03/a/figure.svg", "<svg viewBox=\"0 0 1 1\"></svg>");
	files.put("static/2026/other.txt", "o");
	files.put("static/index.htm", "i");
	ASSERT_TRUE(files.build().has_value());
	EXPECT_EQ(files_under(files.out()),
	          (strings{"2026/09/03/a/figure.svg", "2026/09/03/a/index.html", "2026/other.txt",
	                   "404.html", "archive/index.html", "archive/x.txt", "font/f.woff2",
	                   "font/index.html", "index.htm", "index.html", "style/extra.css",
	                   "style/main.css"}));
}

TEST(build, a_collision_is_reported_with_the_other_faults_of_files_that_were_read)
{
	tree const files;
	files.put("posts/2026-09-03-a.jimd", "[%title]: A\n\n`unclosed\n");
	files.put("static/index.html", "static");
	auto const built = files.build();
	ASSERT_FALSE(built.has_value());
	ASSERT_EQ(built.error().size(), 2U);
	EXPECT_EQ(built.error().at(0).path, files.content() / "posts/2026-09-03-a.jimd");
	EXPECT_EQ(built.error().at(1).path, files.content() / "static/index.html");
	EXPECT_EQ(code_of(built.error().at(1)), blog::errc::static_collision);
}

TEST(build, the_collision_error_is_formatted_with_the_path_of_the_static_file)
{
	tree const files;
	files.put("static/archive", "static");
	files.put("posts/2026-09-03-a.jimd", post_a);
	auto const built = files.build();
	ASSERT_FALSE(built.has_value());
	auto const& err = built.error().front();
	EXPECT_EQ(blog::format_error(err.error, err.path.string()),
	          files.content().string() +
	              "/static/archive: static file collides with a file the site writes: /archive is "
	              "a directory of /archive/index.html");
}
