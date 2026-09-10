#include <array>
#include <chrono>
#include <expected>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <tuple>
#include <variant>
#include <gtest/gtest.h>

#include <blog/error.hxx>
#include <blog/post.hxx>
#include <jimd/ast.hxx>
#include <jimd/parse.hxx>
#include <jimd/position.hxx>
#include <support/result_printers.hxx>

using namespace std::literals;

namespace
{
auto ymd(int year, unsigned month, unsigned day) -> std::chrono::year_month_day
{
	return {std::chrono::year{year}, std::chrono::month{month}, std::chrono::day{day}};
}

auto post_name_error(std::string_view reason) -> blog::error
{
	return {blog::errc::bad_post_name, jimd::position{}, std::string{reason}};
}

auto page_name_error(std::string_view reason) -> blog::error
{
	return {blog::errc::bad_page_name, jimd::position{}, std::string{reason}};
}

constexpr auto not_jimd = "the name must end in .jimd"sv;
constexpr auto not_dated = "the name must look like YYYY-MM-DD-slug.jimd"sv;
constexpr auto not_slug =
	"the slug must be lower-case letters and digits, joined by single hyphens"sv;

// NOLINTNEXTLINE(bugprone-easily-swappable-parameters)
void expect_post_name_error(std::string_view name, std::string_view reason)
{
	auto const result = blog::parse_post_name(name);
	ASSERT_FALSE(result.has_value()) << name;
	EXPECT_EQ(result.error(), post_name_error(reason)) << name;
}

// NOLINTNEXTLINE(bugprone-easily-swappable-parameters)
void expect_page_name_error(std::string_view name, std::string_view reason)
{
	auto const result = blog::parse_page_name(name);
	ASSERT_FALSE(result.has_value()) << name;
	EXPECT_EQ(result.error(), page_name_error(reason)) << name;
}

auto post_of(std::string_view name, std::string const& source)
	-> std::expected<blog::post, blog::error>
{
	auto const doc = jimd::parse(source);
	EXPECT_TRUE(doc.has_value());
	return blog::read_post(name, *doc);
}

auto page_of(std::string_view name, std::string const& source)
	-> std::expected<blog::page, blog::error>
{
	auto const doc = jimd::parse(source);
	EXPECT_TRUE(doc.has_value());
	return blog::read_page(name, *doc);
}

auto throws_out_of_range(auto format, std::chrono::year_month_day date) -> bool
{
	try {
		std::ignore = format(date);
	} catch (std::out_of_range const&) {
		return true;
	}
	return false;
}

constexpr unsigned months_in_a_year = 12;
constexpr auto a_post = "2026-09-03-a-post.jimd"sv;
}

TEST(post_name, a_good_name_has_a_date_and_a_slug)
{
	auto const name = blog::parse_post_name("2026-09-03-what-is-a-linear-system-anyway.jimd");
	ASSERT_TRUE(name.has_value());
	EXPECT_EQ(name->date, ymd(2026, 9, 3));
	EXPECT_EQ(name->slug, "what-is-a-linear-system-anyway");
}

TEST(post_name, a_slug_is_words_of_lower_case_letters_and_digits)
{
	for (auto const slug :
	     {"a"sv, "2"sv, "2026"sv, "a1"sv, "a-b"sv, "a-1-b-2"sv, "0-0"sv, "pi-control"sv}) {
		auto const name =
			blog::parse_post_name(std::string{"2026-09-03-"} + std::string{slug} + ".jimd");
		ASSERT_TRUE(name.has_value()) << slug;
		EXPECT_EQ(name->slug, slug);
	}
}

TEST(post_name, a_date_has_to_exist)
{
	for (auto const date :
	     {"2026-01-01"sv, "2026-12-31"sv, "2026-04-30"sv, "2026-02-28"sv, "2028-02-29"sv,
	      "2024-02-29"sv, "2000-02-29"sv, "0000-02-29"sv, "9999-12-31"sv}) {
		EXPECT_TRUE(blog::parse_post_name(std::string{date} + "-x.jimd").has_value()) << date;
	}
	for (auto const date :
	     {"2026-02-29"sv, "2026-02-30"sv, "2026-04-31"sv, "2026-06-31"sv, "2026-09-31"sv,
	      "2026-11-31"sv, "2026-01-32"sv, "2026-01-00"sv, "2026-00-10"sv, "2026-13-01"sv,
	      "2026-99-99"sv, "1900-02-29"sv, "2100-02-29"sv, "2027-02-29"sv, "0001-02-29"sv}) {
		auto const name = std::string{date} + "-x.jimd";
		expect_post_name_error(name, std::string{date} + " is not a date");
	}
}

TEST(post_name, leap_years_follow_the_gregorian_rule)
{
	EXPECT_TRUE(blog::parse_post_name("2028-02-29-x.jimd").has_value());
	EXPECT_TRUE(blog::parse_post_name("2000-02-29-x.jimd").has_value());
	EXPECT_FALSE(blog::parse_post_name("1900-02-29-x.jimd").has_value());
	EXPECT_FALSE(blog::parse_post_name("2100-02-29-x.jimd").has_value());
	EXPECT_FALSE(blog::parse_post_name("2026-02-29-x.jimd").has_value());
}

TEST(post_name, the_date_is_read_as_decimal)
{
	auto const name = blog::parse_post_name("2026-08-09-x.jimd");
	ASSERT_TRUE(name.has_value());
	EXPECT_EQ(name->date, ymd(2026, 8, 9));
}

TEST(post_name, a_name_that_does_not_look_like_one)
{
	for (auto const name :
	     {"2026-9-03-x.jimd"sv, "2026-09-3-x.jimd"sv, "26-09-03-x.jimd"sv, "2026-09-03.jimd"sv,
	      "2026-09-03x.jimd"sv, "20260903-x.jimd"sv, "2026_09_03-x.jimd"sv, "x2026-09-03-a.jimd"sv,
	      "x.jimd"sv, "a-post.jimd"sv, ".jimd"sv, "2026-09-0-.jimd"sv, "+026-09-03-x.jimd"sv,
	      "-026-09-03-x.jimd"sv, " 026-09-03-x.jimd"sv,
	      "\xef\xbc\x92\xef\xbc\x90\xef\xbc\x92\xef\xbc\x96-09-03-x.jimd"sv,
	      "posts/2026-09-03-x.jimd"sv}) {
		expect_post_name_error(name, not_dated);
	}
}

TEST(post_name, a_name_has_to_end_in_jimd)
{
	for (auto const name :
	     {"2026-09-03-x.md"sv, "2026-09-03-x"sv, "2026-09-03-x.JIMD"sv, "2026-09-03-x.jimd.bak"sv,
	      "2026-09-03-x.jimd~"sv, "2026-09-03-x.jimd "sv, ""sv, "jimd"sv, "2026-09-03-x.jim"sv,
	      "2026-09-03-x.html"sv}) {
		expect_post_name_error(name, not_jimd);
	}
}

TEST(file_name, the_extension_check_is_the_first_rule_of_both_names)
{
	EXPECT_TRUE(blog::check_extension(blog::errc::bad_post_name, "notes.jimd").has_value());
	EXPECT_TRUE(blog::check_extension(blog::errc::bad_page_name, "Not A Slug.jimd").has_value());
	for (auto const name : {"notes.txt"sv, "notes.JIMD"sv, "jimd"sv, ""sv}) {
		auto const post = blog::check_extension(blog::errc::bad_post_name, name);
		ASSERT_FALSE(post.has_value()) << name;
		EXPECT_EQ(post.error(), post_name_error(not_jimd)) << name;
		EXPECT_EQ(post.error(), blog::parse_post_name(name).error()) << name;
		auto const page = blog::check_extension(blog::errc::bad_page_name, name);
		ASSERT_FALSE(page.has_value()) << name;
		EXPECT_EQ(page.error(), page_name_error(not_jimd)) << name;
		EXPECT_EQ(page.error(), blog::parse_page_name(name).error()) << name;
	}
}

TEST(post_name, a_slug_has_to_be_a_slug)
{
	for (auto const name :
	     {"2026-09-03-.jimd"sv, "2026-09-03--.jimd"sv, "2026-09-03--a.jimd"sv,
	      "2026-09-03-a-.jimd"sv, "2026-09-03-a--b.jimd"sv, "2026-09-03-A.jimd"sv,
	      "2026-09-03-aB.jimd"sv, "2026-09-03-a_b.jimd"sv, "2026-09-03-a b.jimd"sv,
	      "2026-09-03-a.b.jimd"sv, "2026-09-03-a/b.jimd"sv, "2026-09-03-a\\b.jimd"sv,
	      "2026-09-03-\xc3\xa9.jimd"sv, "2026-09-03-a\nb.jimd"sv, "2026-09-03-a\0b.jimd"sv}) {
		expect_post_name_error(name, not_slug);
	}
}

TEST(post_name, the_first_fault_from_the_left_is_reported)
{
	expect_post_name_error("2026-02-30-Bad.md", not_jimd);
	expect_post_name_error("2026-2-30-Bad.jimd", not_dated);
	expect_post_name_error("2026-02-30-Bad.jimd", "2026-02-30 is not a date");
	expect_post_name_error("2026-02-28-Bad.jimd", not_slug);
}

TEST(post_name, an_error_has_no_place_and_is_formatted_with_the_path)
{
	auto const result = blog::parse_post_name("2026-02-30-x.jimd");
	ASSERT_FALSE(result.has_value());
	EXPECT_EQ(result.error().code,
	          (std::variant<jimd::errc, blog::errc>{blog::errc::bad_post_name}));
	EXPECT_EQ(result.error().line, 0U);
	EXPECT_EQ(result.error().column, 0U);
	EXPECT_EQ(blog::format_error(result.error(), "content/posts/2026-02-30-x.jimd"),
	          "content/posts/2026-02-30-x.jimd: bad post file name: 2026-02-30 is not a date");
	EXPECT_EQ(blog::format_error(blog::parse_post_name("x.md").error(), "content/posts/x.md"),
	          "content/posts/x.md: bad post file name: the name must end in .jimd");
}

TEST(page_name, a_good_name_is_a_slug)
{
	for (auto const slug : {"about"sv, "a"sv, "2"sv, "a-b"sv, "a1-b2"sv, "colophon-2"sv}) {
		auto const name = blog::parse_page_name(std::string{slug} + ".jimd");
		ASSERT_TRUE(name.has_value()) << slug;
		EXPECT_EQ(*name, slug);
	}
}

TEST(page_name, the_slugs_the_site_writes_itself_are_still_names)
{
	for (auto const slug : {"archive"sv, "2026"sv, "index"sv, "style"sv}) {
		EXPECT_TRUE(blog::parse_page_name(std::string{slug} + ".jimd").has_value()) << slug;
	}
}

TEST(page_name, a_bad_name)
{
	for (auto const name :
	     {"about.md"sv, "about"sv, "about.JIMD"sv, "about.jimd.bak"sv, ""sv, "jimd"sv}) {
		expect_page_name_error(name, not_jimd);
	}
	for (auto const name : {".jimd"sv, "About.jimd"sv, "about me.jimd"sv, "-a.jimd"sv, "a-.jimd"sv,
	                        "a--b.jimd"sv, "a_b.jimd"sv, "a.b.jimd"sv, "pages/about.jimd"sv,
	                        "a\\b.jimd"sv, "\xc3\xa9.jimd"sv, "2026-09-03-about.jimd.jimd"sv}) {
		expect_page_name_error(name, not_slug);
	}
}

TEST(page_name, an_error_is_formatted_with_the_path)
{
	EXPECT_EQ(blog::format_error(blog::parse_page_name("About Me.jimd").error(),
	                             "content/pages/About Me.jimd"),
	          "content/pages/About Me.jimd: bad page file name: the slug must be lower-case "
	          "letters and digits, joined by single hyphens");
}

TEST(post, the_metadata_is_the_title_the_series_and_the_description)
{
	std::string const source = "[%title]: What is a linear system anyway?\n"
							   "[%series]: pi\n"
							   "[%description]: A look at linearity.\n"
							   "\nBody.\n";
	auto const item = post_of("2026-09-03-what-is-a-linear-system-anyway.jimd", source);
	ASSERT_TRUE(item.has_value());
	EXPECT_EQ(item->date, ymd(2026, 9, 3));
	EXPECT_EQ(item->slug, "what-is-a-linear-system-anyway");
	EXPECT_EQ(item->title, "What is a linear system anyway?");
	EXPECT_EQ(item->series, "pi");
	EXPECT_EQ(item->description, "A look at linearity.");
}

TEST(post, the_series_and_the_description_are_optional)
{
	std::string const source = "[%title]: T\n\nBody.\n";
	auto const item = post_of(a_post, source);
	ASSERT_TRUE(item.has_value());
	EXPECT_EQ(item->title, "T");
	EXPECT_EQ(item->series, std::nullopt);
	EXPECT_EQ(item->description, std::nullopt);
}

TEST(post, a_post_with_no_content_is_a_post)
{
	auto const item = post_of(a_post, "[%title]: T\n");
	ASSERT_TRUE(item.has_value());
}

TEST(post, the_order_and_the_blank_lines_of_the_metadata_do_not_matter)
{
	std::string const source = "[%series]: s\n\n[%description]: d\n[%title]: T\n";
	auto const item = post_of(a_post, source);
	ASSERT_TRUE(item.has_value());
	EXPECT_EQ(item->title, "T");
	EXPECT_EQ(item->series, "s");
	EXPECT_EQ(item->description, "d");
}

TEST(post, the_title_is_the_text_as_written)
{
	std::string const source = "[%title]: Let's say \"x\" -- _not_ *this* <b>\n";
	auto const item = post_of(a_post, source);
	ASSERT_TRUE(item.has_value());
	EXPECT_EQ(item->title, "Let's say \"x\" -- _not_ *this* <b>");
}

TEST(post, a_crlf_source_gives_values_without_the_carriage_return)
{
	std::string const source = "[%title]: T\r\n[%series]: s\r\n\r\nBody.\r\n";
	auto const item = post_of(a_post, source);
	ASSERT_TRUE(item.has_value());
	EXPECT_EQ(item->title, "T");
	EXPECT_EQ(item->series, "s");
}

TEST(post, a_post_does_not_depend_on_its_source)
{
	std::optional<blog::post> item;
	{
		auto source =
			std::make_unique<std::string>("[%title]: Title\n[%series]: s\n[%description]: d\n");
		auto doc = jimd::parse(*source);
		ASSERT_TRUE(doc.has_value());
		auto result = blog::read_post(a_post, *doc);
		ASSERT_TRUE(result.has_value());
		item = *std::move(result);
	}
	EXPECT_EQ(item->title, "Title");
	EXPECT_EQ(item->series, "s");
	EXPECT_EQ(item->description, "d");
}

TEST(post, a_missing_title_has_no_place)
{
	for (auto const* source : {"Body.\n", "[%series]: s\n\nBody.\n", "", "[%description]: d\n"}) {
		auto const item = post_of(a_post, source);
		ASSERT_FALSE(item.has_value()) << source;
		EXPECT_EQ(item.error(), blog::error{blog::errc::missing_title}) << source;
	}
	auto const item = post_of(a_post, "[%series]: s\n");
	EXPECT_EQ(blog::format_error(item.error(), "content/posts/a.jimd"),
	          "content/posts/a.jimd: missing title");
}

TEST(post, an_empty_value_is_missing)
{
	jimd::ast::document doc;
	doc.metadata["title"] = "";
	doc.metadata["series"] = "";
	doc.metadata["description"] = "";
	EXPECT_EQ(blog::read_post(a_post, doc).error(), blog::error{blog::errc::missing_title});

	doc.metadata["title"] = "T";
	auto const item = blog::read_post(a_post, doc);
	ASSERT_TRUE(item.has_value());
	EXPECT_EQ(item->series, std::nullopt);
	EXPECT_EQ(item->description, std::nullopt);
}

TEST(post, an_unknown_key_is_an_error_at_the_key)
{
	std::string const source = "[%title]: T\n[%tags]: a b\n\nBody.\n";
	auto const item = post_of(a_post, source);
	ASSERT_FALSE(item.has_value());
	EXPECT_EQ(item.error(),
	          (blog::error{blog::errc::unknown_metadata, jimd::position{.line = 2, .column = 3},
	                       "tags (a post has title, series, description)"}));
	EXPECT_EQ(blog::format_error(item.error(), "content/posts/a.jimd"),
	          "content/posts/a.jimd:2:3: unknown metadata key: tags (a post has title, series, "
	          "description)");
}

TEST(post, the_first_unknown_key_in_the_file_is_the_one_reported)
{
	std::string const source = "[%title]: T\n[%zebra]: 1\n[%apple]: 2\n";
	auto const item = post_of(a_post, source);
	ASSERT_FALSE(item.has_value());
	EXPECT_EQ(item.error().line, 2U);
	EXPECT_EQ(item.error().column, 3U);
	EXPECT_EQ(item.error().detail, "zebra (a post has title, series, description)");
}

TEST(post, an_unknown_key_is_reported_before_a_missing_title)
{
	std::string const source = "[%titel]: T\n";
	auto const item = post_of(a_post, source);
	ASSERT_FALSE(item.has_value());
	EXPECT_EQ(std::get<blog::errc>(item.error().code), blog::errc::unknown_metadata);
	EXPECT_EQ(item.error().line, 1U);
	EXPECT_EQ(item.error().column, 3U);
}

TEST(post, keys_are_case_sensitive)
{
	std::string const source = "[%Title]: T\n";
	auto const item = post_of(a_post, source);
	ASSERT_FALSE(item.has_value());
	EXPECT_EQ(std::get<blog::errc>(item.error().code), blog::errc::unknown_metadata);
	EXPECT_EQ(item.error().detail, "Title (a post has title, series, description)");
}

TEST(post, a_key_after_blank_lines_and_in_a_crlf_source_is_found)
{
	std::string const source = "\n[%title]: T\r\n\r\n[%draft]: yes\r\n";
	auto const item = post_of(a_post, source);
	ASSERT_FALSE(item.has_value());
	EXPECT_EQ(item.error().line, 4U);
	EXPECT_EQ(item.error().column, 3U);
}

TEST(post, an_unknown_key_in_a_tree_not_parsed_from_a_source_has_no_place)
{
	jimd::ast::document doc;
	doc.metadata["title"] = "T";
	doc.metadata["tags"] = "a";
	doc.metadata["draft"] = "b";
	auto const item = blog::read_post(a_post, doc);
	ASSERT_FALSE(item.has_value());
	EXPECT_EQ(item.error().line, 0U);
	EXPECT_EQ(item.error().detail, "draft (a post has title, series, description)");
}

TEST(post, a_bad_name_is_reported_before_the_metadata)
{
	std::string const source = "[%tags]: a\n";
	auto const item = post_of("2026-02-30-x.jimd", source);
	ASSERT_FALSE(item.has_value());
	EXPECT_EQ(std::get<blog::errc>(item.error().code), blog::errc::bad_post_name);
	EXPECT_EQ(item.error().line, 0U);
}

TEST(post, the_url_is_the_date_and_the_slug)
{
	std::string const source = "[%title]: T\n";
	auto const item = post_of("2026-09-03-what-is-a-linear-system-anyway.jimd", source);
	ASSERT_TRUE(item.has_value());
	EXPECT_EQ(blog::url(*item), "/2026/09/03/what-is-a-linear-system-anyway/");
}

TEST(post, the_url_pads_the_date)
{
	std::string const source = "[%title]: T\n";
	EXPECT_EQ(blog::url(*post_of("2026-01-05-x.jimd", source)), "/2026/01/05/x/");
	EXPECT_EQ(blog::url(*post_of("2026-12-31-x.jimd", source)), "/2026/12/31/x/");
	EXPECT_EQ(blog::url(*post_of("0042-02-03-x.jimd", source)), "/0042/02/03/x/");
	EXPECT_EQ(blog::url(*post_of("0000-02-29-x.jimd", source)), "/0000/02/29/x/");
}

TEST(page, the_metadata_is_the_title_and_the_description)
{
	std::string const source = "[%title]: About\n[%description]: Who I am.\n\nHello.\n";
	auto const item = page_of("about.jimd", source);
	ASSERT_TRUE(item.has_value());
	EXPECT_EQ(item->slug, "about");
	EXPECT_EQ(item->title, "About");
	EXPECT_EQ(item->description, "Who I am.");
	EXPECT_EQ(blog::url(*item), "/about/");
}

TEST(page, the_description_is_optional)
{
	auto const item = page_of("about.jimd", "[%title]: About\n");
	ASSERT_TRUE(item.has_value());
	EXPECT_EQ(item->description, std::nullopt);
}

TEST(page, a_missing_title_has_no_place)
{
	auto const item = page_of("about.jimd", "Hello.\n");
	ASSERT_FALSE(item.has_value());
	EXPECT_EQ(item.error(), blog::error{blog::errc::missing_title});
}

TEST(page, a_series_is_an_unknown_key)
{
	std::string const source = "[%title]: About\n[%series]: s\n";
	auto const item = page_of("about.jimd", source);
	ASSERT_FALSE(item.has_value());
	EXPECT_EQ(item.error(),
	          (blog::error{blog::errc::unknown_metadata, jimd::position{.line = 2, .column = 3},
	                       "series (a page has title, description)"}));
}

TEST(page, the_first_unknown_key_in_the_file_is_the_one_reported)
{
	std::string const source = "[%title]: About\n[%zebra]: 1\n[%apple]: 2\n";
	auto const item = page_of("about.jimd", source);
	ASSERT_FALSE(item.has_value());
	EXPECT_EQ(item.error().line, 2U);
	EXPECT_EQ(item.error().detail, "zebra (a page has title, description)");
}

TEST(page, a_bad_name_is_reported_before_the_metadata)
{
	auto const item = page_of("About.jimd", "Hello.\n");
	ASSERT_FALSE(item.has_value());
	EXPECT_EQ(std::get<blog::errc>(item.error().code), blog::errc::bad_page_name);
}

TEST(page, a_page_cannot_take_a_path_the_site_writes)
{
	struct example {
		std::string_view name;
		std::string_view detail;
	};
	for (auto const& [name, detail] : {
			 example{.name = "archive.jimd", .detail = "/archive/ (the archive)"},
			 example{.name = "2026.jimd", .detail = "/2026/ (posts are written under it)"},
			 example{.name = "0000.jimd", .detail = "/0000/ (posts are written under it)"},
			 example{.name = "9999.jimd", .detail = "/9999/ (posts are written under it)"},
		 }) {
		auto const item = page_of(name, "[%title]: T\n");
		ASSERT_FALSE(item.has_value()) << name;
		EXPECT_EQ(item.error(),
		          (blog::error{blog::errc::page_collision, jimd::position{}, std::string{detail}}))
		    << name;
	}
}

TEST(page, a_collision_is_formatted_with_its_path)
{
	auto const item = page_of("archive.jimd", "[%title]: Archive\n");
	ASSERT_FALSE(item.has_value());
	EXPECT_EQ(blog::format_error(item.error(), "content/pages/archive.jimd"),
	          "content/pages/archive.jimd: page collides with a generated path: /archive/ (the "
	          "archive)");
}

TEST(page, names_that_look_like_but_are_not_generated_paths)
{
	for (auto const name :
	     {"index.jimd"sv, "style.jimd"sv, "font.jimd"sv, "image.jimd"sv, "2026a.jimd"sv,
	      "20260.jimd"sv, "202.jimd"sv, "archive-2.jimd"sv, "styles.jimd"sv, "my-archive.jimd"sv}) {
		EXPECT_TRUE(page_of(name, "[%title]: T\n").has_value()) << name;
	}
}

TEST(page, a_metadata_error_is_reported_before_a_collision)
{
	auto const no_title = page_of("archive.jimd", "Hello.\n");
	ASSERT_FALSE(no_title.has_value());
	EXPECT_EQ(no_title.error(), blog::error{blog::errc::missing_title});

	auto const unknown_key = page_of("2026.jimd", "[%title]: T\n[%series]: s\n");
	ASSERT_FALSE(unknown_key.has_value());
	EXPECT_EQ(std::get<blog::errc>(unknown_key.error().code), blog::errc::unknown_metadata);
}

TEST(post, a_post_may_use_the_slug_archive_or_a_year)
{
	EXPECT_TRUE(post_of("2026-09-03-archive.jimd", "[%title]: T\n").has_value());
	EXPECT_TRUE(post_of("2026-09-03-2024.jimd", "[%title]: T\n").has_value());
}

TEST(page, a_page_does_not_depend_on_its_source)
{
	std::optional<blog::page> item;
	{
		auto source = std::make_unique<std::string>("[%title]: Title\n[%description]: d\n");
		auto doc = jimd::parse(*source);
		ASSERT_TRUE(doc.has_value());
		auto result = blog::read_page("about.jimd", *doc);
		ASSERT_TRUE(result.has_value());
		item = *std::move(result);
	}
	EXPECT_EQ(item->title, "Title");
	EXPECT_EQ(item->description, "d");
}

TEST(date, the_long_form_names_the_month)
{
	EXPECT_EQ(blog::long_date(ymd(2026, 9, 3)), "3 September 2026");
	EXPECT_EQ(blog::long_date(ymd(2028, 2, 29)), "29 February 2028");
	EXPECT_EQ(blog::long_date(ymd(2026, 1, 1)), "1 January 2026");
	EXPECT_EQ(blog::long_date(ymd(2026, 12, 31)), "31 December 2026");
	EXPECT_EQ(blog::long_date(ymd(2026, 10, 14)), "14 October 2026");
}

TEST(date, the_short_form_is_lower_case)
{
	EXPECT_EQ(blog::short_date(ymd(2026, 9, 3)), "3 sep");
	EXPECT_EQ(blog::short_date(ymd(2028, 2, 29)), "29 feb");
	EXPECT_EQ(blog::short_date(ymd(2026, 1, 1)), "1 jan");
	EXPECT_EQ(blog::short_date(ymd(2026, 12, 31)), "31 dec");
}

TEST(date, every_month_has_a_name)
{
	constexpr std::array<std::string_view, months_in_a_year> long_names = {
		"January", "February", "March",     "April",   "May",      "June",
		"July",    "August",   "September", "October", "November", "December"};
	constexpr std::array<std::string_view, months_in_a_year> short_names = {
		"jan", "feb", "mar", "apr", "may", "jun", "jul", "aug", "sep", "oct", "nov", "dec"};
	for (unsigned month = 1; month <= months_in_a_year; ++month) {
		EXPECT_EQ(blog::long_date(ymd(2026, month, 15)),
		          "15 " + std::string{long_names.at(month - 1)} + " 2026");
		EXPECT_EQ(blog::short_date(ymd(2026, month, 15)),
		          "15 " + std::string{short_names.at(month - 1)});
	}
}

TEST(date, a_date_that_does_not_exist_is_refused)
{
	for (auto const date : {ymd(2026, 13, 1), ymd(2026, 0, 1), ymd(2026, 2, 30), ymd(2026, 1, 0),
	                        ymd(2026, 4, 31), ymd(1900, 2, 29)}) {
		EXPECT_TRUE(throws_out_of_range(blog::long_date, date));
		EXPECT_TRUE(throws_out_of_range(blog::short_date, date));
	}
	EXPECT_EQ(blog::long_date(ymd(2000, 2, 29)), "29 February 2000");
}

TEST(date, a_date_read_from_a_name_formats_as_it_was_written)
{
	auto const name = blog::parse_post_name("2028-02-29-leap.jimd");
	ASSERT_TRUE(name.has_value());
	EXPECT_EQ(blog::long_date(name->date), "29 February 2028");
	EXPECT_EQ(blog::short_date(name->date), "29 feb");
}
