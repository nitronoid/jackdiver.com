#include <cstddef>
#include <stdexcept>
#include <string>
#include <string_view>
#include <tuple>
#include <gtest/gtest.h>

#include <blog/frame.hxx>
#include <blog/site.hxx>
#include <blog_test/corpus.hxx>

using namespace std::literals;

namespace
{
auto main_of(std::string_view page) -> std::string
{
	constexpr auto open = "<main>\n"sv;
	auto const start = page.find(open);
	auto const end = page.find("</main>\n");
	if (start == std::string_view::npos || end == std::string_view::npos) {
		return "<no main>";
	}
	return std::string{page.substr(start + open.size(), end - start - open.size())};
}

auto title_of(std::string_view page) -> std::string
{
	constexpr auto open = "<title>"sv;
	auto const start = page.find(open);
	auto const end = page.find("</title>");
	if (start == std::string_view::npos || end == std::string_view::npos) {
		return "<no title>";
	}
	return std::string{page.substr(start + open.size(), end - start - open.size())};
}

auto count_of(std::string_view haystack, std::string_view needle) -> std::size_t
{
	std::size_t count = 0;
	for (auto pos = haystack.find(needle); pos != std::string_view::npos;
	     pos = haystack.find(needle, pos + needle.size())) {
		++count;
	}
	return count;
}

auto model_of(blog_test::corpus const& files) -> blog::site_model
{
	auto model = files.build();
	EXPECT_TRUE(model.has_value());
	return model.value_or(blog::site_model{});
}

auto index_of(blog::site_model const& model, std::string_view slug) -> std::size_t
{
	for (std::size_t idx = 0; idx < model.posts.size(); ++idx) {
		if (model.posts.at(idx).item.slug == slug) {
			return idx;
		}
	}
	ADD_FAILURE() << slug;
	return 0;
}

auto throws_out_of_range(auto frame, blog::site_model const& model, std::size_t index) -> bool
{
	try {
		std::ignore = frame(model, index, "");
		return false;
	} catch (std::out_of_range const&) {
		return true;
	}
}

constexpr auto head_start = R"(<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="utf-8">
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <title>)"sv;

constexpr auto head_end = R"(</title>
  <script>
    (function () {
      try {
        var t = localStorage.getItem('theme');
        if (t === 'light' || t === 'dark') {
          document.documentElement.style.colorScheme = t;
        }
      } catch (e) {
      }
    })();
  </script>
  <link rel="stylesheet" href="/style/main.css">
  <link rel="preload" as="font" type="font/woff2" crossorigin
        href="/font/source-serif-4-roman.woff2">
  <link rel="icon" href="/favicon.svg" type="image/svg+xml">
  <script src="/theme.js" defer></script>
</head>
<body>
<header class="site-header">
  <a class="site-title" href="/">Jack Diver</a>
  <nav class="site-nav">
    <a href="/archive/">archive</a>
)"sv;

constexpr auto header_end =
	R"(    <button type="button" class="theme-toggle" data-theme-toggle hidden>
      <span class="theme-toggle-label" aria-hidden="true"></span>
    </button>
  </nav>
</header>
<main>
)"sv;

constexpr auto page_end = R"(</main>
</body>
</html>
)"sv;
}

TEST(frame, a_post_in_the_middle_of_a_series_is_a_whole_page)
{
	blog_test::corpus files;
	files.post("2026-09-02-one.jimd", "[%title]: One\n[%series]: s\n");
	files.post("2026-09-03-two.jimd",
	           "[%title]: What's two?\n[%series]: s\n[%description]: A \"d\" & e\n");
	files.post("2026-09-04-three.jimd", "[%title]: Three\n[%series]: s\n");
	files.page_title("about.jimd", "About");
	auto const model = model_of(files);
	auto const page = blog::frame_post(model, index_of(model, "two"), "<p>Body.</p>\n");

	std::string expected{head_start};
	expected += "What’s two? – Jack Diver";
	expected += "</title>\n  <meta name=\"description\" content=\"A &quot;d&quot; &amp; e\">\n";
	expected += head_end.substr(head_end.find("  <script>"));
	expected += "    <a href=\"/about/\">About</a>\n";
	expected += header_end;
	expected += R"(  <article class="post">
    <h1>What’s two?</h1>
    <nav class="post-meta">
      <span class="date">3 September 2026</span>
    </nav>
    <nav class="series-nav" aria-label="Series navigation">
      <p class="series-label">Part 2 of 3 in a series</p>
      <ol class="series-parts">
        <li><a href="/2026/09/02/one/">One</a></li>
        <li class="current" aria-current="true">What’s two?</li>
        <li><a href="/2026/09/04/three/">Three</a></li>
      </ol>
    </nav>
<p>Body.</p>
    <nav class="series-footer" aria-label="Series parts">
      <a class="previous" href="/2026/09/02/one/">
        <span class="direction">← Previous part</span>
        <span class="title">One</span>
      </a>
      <a class="next" href="/2026/09/04/three/">
        <span class="direction">Next part →</span>
        <span class="title">Three</span>
      </a>
    </nav>
    <nav class="post-nav" aria-label="Adjacent posts">
      <a class="older" href="/2026/09/02/one/">
        <span class="direction">← Older</span>
        <span class="title">One</span>
      </a>
      <a class="newer" href="/2026/09/04/three/">
        <span class="direction">Newer →</span>
        <span class="title">Three</span>
      </a>
    </nav>
  </article>
)";
	expected += page_end;
	EXPECT_EQ(page, expected);
}

TEST(frame, the_first_part_of_a_series_has_only_a_next_part)
{
	blog_test::corpus files;
	files.post_title("2026-09-03-a.jimd", "A", "s");
	files.post_title("2026-09-04-b.jimd", "B", "s");
	auto const model = model_of(files);
	auto const main = main_of(blog::frame_post(model, index_of(model, "a"), "<p>x</p>\n"));
	EXPECT_EQ(main, R"(  <article class="post">
    <h1>A</h1>
    <nav class="post-meta">
      <span class="date">3 September 2026</span>
    </nav>
    <nav class="series-nav" aria-label="Series navigation">
      <p class="series-label">Part 1 of 2 in a series</p>
      <ol class="series-parts">
        <li class="current" aria-current="true">A</li>
        <li><a href="/2026/09/04/b/">B</a></li>
      </ol>
    </nav>
<p>x</p>
    <nav class="series-footer" aria-label="Series parts">
      <a class="next" href="/2026/09/04/b/">
        <span class="direction">Next part →</span>
        <span class="title">B</span>
      </a>
    </nav>
    <nav class="post-nav" aria-label="Adjacent posts">
      <a class="newer" href="/2026/09/04/b/">
        <span class="direction">Newer →</span>
        <span class="title">B</span>
      </a>
    </nav>
  </article>
)");
}

TEST(frame, the_last_part_of_a_series_has_only_a_previous_part)
{
	blog_test::corpus files;
	files.post_title("2026-09-03-a.jimd", "A", "s");
	files.post_title("2026-09-04-b.jimd", "B", "s");
	auto const model = model_of(files);
	auto const main = main_of(blog::frame_post(model, index_of(model, "b"), "<p>x</p>\n"));
	EXPECT_NE(main.find("<p class=\"series-label\">Part 2 of 2 in a series</p>"),
	          std::string::npos);
	EXPECT_NE(main.find("<a class=\"previous\" href=\"/2026/09/03/a/\">"), std::string::npos);
	EXPECT_EQ(main.find("class=\"next\""), std::string::npos);
	EXPECT_NE(main.find("<a class=\"older\" href=\"/2026/09/03/a/\">"), std::string::npos);
	EXPECT_EQ(main.find("class=\"newer\""), std::string::npos);
}

TEST(frame, a_post_with_no_neighbours_has_no_navigation)
{
	blog_test::corpus files;
	files.post_title("2026-09-03-a.jimd", "Alone", "pi");
	auto const model = model_of(files);
	auto const page = blog::frame_post(model, 0, "<p>x</p>\n");
	EXPECT_EQ(main_of(page), R"(  <article class="post">
    <h1>Alone</h1>
    <nav class="post-meta">
      <span class="date">3 September 2026</span>
    </nav>
<p>x</p>
  </article>
)");
	EXPECT_EQ(page.find("series-nav"), std::string::npos);
	EXPECT_EQ(page.find("series-footer"), std::string::npos);
	EXPECT_EQ(page.find("post-nav"), std::string::npos);
}

TEST(frame, a_post_outside_a_series_has_a_post_nav_and_no_series_navigation)
{
	blog_test::corpus files;
	files.post_title("2026-09-03-a.jimd", "A");
	files.post_title("2026-09-04-b.jimd", "B", "pi");
	files.post_title("2026-09-05-c.jimd", "C");
	auto const model = model_of(files);
	auto const page = blog::frame_post(model, index_of(model, "b"), "<p>x</p>\n");
	EXPECT_EQ(page.find("series-"), std::string::npos);
	EXPECT_EQ(count_of(page, "<nav class=\"post-nav\""), 1U);
	EXPECT_NE(page.find("<a class=\"older\" href=\"/2026/09/03/a/\">"), std::string::npos);
	EXPECT_NE(page.find("<a class=\"newer\" href=\"/2026/09/05/c/\">"), std::string::npos);
}

TEST(frame, the_oldest_and_the_newest_post_have_one_link_in_the_post_nav)
{
	blog_test::corpus files;
	files.post_title("2026-09-03-a.jimd", "A");
	files.post_title("2026-09-04-b.jimd", "B");
	auto const model = model_of(files);
	auto const oldest = blog::frame_post(model, index_of(model, "a"), "<p>x</p>\n");
	EXPECT_EQ(count_of(oldest, "<a class=\"newer\""), 1U);
	EXPECT_EQ(count_of(oldest, "<a class=\"older\""), 0U);
	auto const newest = blog::frame_post(model, index_of(model, "b"), "<p>x</p>\n");
	EXPECT_EQ(count_of(newest, "<a class=\"older\""), 1U);
	EXPECT_EQ(count_of(newest, "<a class=\"newer\""), 0U);
}

TEST(frame, the_fragment_is_written_as_it_is_on_lines_of_its_own)
{
	blog_test::corpus files;
	files.post_title("2026-09-03-a.jimd", "A");
	auto const model = model_of(files);
	auto const with_line_end = main_of(blog::frame_post(model, 0, "<p>x</p>\n<aside>n</aside>\n"));
	EXPECT_NE(with_line_end.find("    </nav>\n<p>x</p>\n<aside>n</aside>\n  </article>\n"),
	          std::string::npos);
	auto const without = main_of(blog::frame_post(model, 0, "<p>x</p>"));
	EXPECT_EQ(without, with_line_end.substr(0, with_line_end.find("<aside>")) + "  </article>\n");
	auto const empty = main_of(blog::frame_post(model, 0, ""));
	EXPECT_NE(empty.find("    </nav>\n  </article>\n"), std::string::npos);
}

TEST(frame, the_title_of_a_post_is_smartened_and_escaped_everywhere)
{
	blog_test::corpus files;
	files.post_title("2026-09-03-a.jimd", "Let's say \"A\" <b> & ... -- done", "s");
	files.post_title("2026-09-04-b.jimd", "It's B", "s");
	auto const model = model_of(files);
	auto const page = blog::frame_post(model, index_of(model, "b"), "<p>x</p>\n");
	constexpr auto smart = "Let’s say “A” &lt;b&gt; &amp; … — done"sv;
	EXPECT_EQ(title_of(page), "It’s B – Jack Diver");
	EXPECT_EQ(count_of(page, smart), 3U);
	EXPECT_NE(page.find("<li><a href=\"/2026/09/03/a/\">"s + std::string{smart} + "</a></li>"),
	          std::string::npos);
	EXPECT_NE(page.find("<h1>It’s B</h1>"), std::string::npos);
	auto const first = blog::frame_post(model, index_of(model, "a"), "<p>x</p>\n");
	EXPECT_EQ(title_of(first), std::string{smart} + " – Jack Diver");
	EXPECT_NE(first.find("<h1>" + std::string{smart} + "</h1>"), std::string::npos);
	EXPECT_EQ(first.find("<b>"), std::string::npos);
}

TEST(frame, the_description_is_written_only_when_there_is_one)
{
	blog_test::corpus files;
	files.post("2026-09-03-a.jimd", "[%title]: A\n[%description]: Short \"one\" & <more>\n");
	files.post_title("2026-09-04-b.jimd", "B");
	files.page("about.jimd", "[%title]: About\n[%description]: About me.\n");
	files.page_title("now.jimd", "Now");
	auto const model = model_of(files);
	auto const with = blog::frame_post(model, index_of(model, "a"), "");
	EXPECT_NE(with.find("  <meta name=\"description\" content=\"Short &quot;one&quot; &amp; "
	                    "&lt;more&gt;\">\n"),
	          std::string::npos);
	EXPECT_EQ(count_of(with, "<meta name=\"description\""), 1U);
	EXPECT_EQ(blog::frame_post(model, index_of(model, "b"), "").find("description"),
	          std::string::npos);
	EXPECT_NE(blog::frame_page(model, 0, "").find("content=\"About me.\""), std::string::npos);
	EXPECT_EQ(blog::frame_page(model, 1, "").find("description"), std::string::npos);
	EXPECT_EQ(blog::frame_index(model, "").find("description"), std::string::npos);
	EXPECT_EQ(blog::frame_archive(model).find("description"), std::string::npos);
}

TEST(frame, a_series_footer_with_no_link_is_left_out)
{
	blog_test::corpus files;
	files.post_title("2026-09-03-a.jimd", "A", "s");
	files.post_title("2026-09-04-b.jimd", "B", "s");
	auto model = model_of(files);
	model.series.front().posts = {index_of(model, "b")};
	model.posts.at(index_of(model, "b")).series = blog::series_position{.group = 0, .part = 0};
	auto const page = blog::frame_post(model, index_of(model, "b"), "<p>x</p>\n");
	EXPECT_NE(page.find("Part 1 of 1 in a series"), std::string::npos);
	EXPECT_EQ(page.find("series-footer"), std::string::npos);
	EXPECT_EQ(count_of(page, "<nav class=\"post-nav\""), 1U);
}

TEST(frame, the_header_links_every_page_in_slug_order)
{
	blog_test::corpus files;
	files.post_title("2026-09-03-a.jimd", "A");
	files.page_title("now.jimd", "Now & \"then\"");
	files.page_title("about.jimd", "About");
	auto const model = model_of(files);
	std::string const expected_nav = "    <a href=\"/archive/\">archive</a>\n"
									 "    <a href=\"/about/\">About</a>\n"
									 "    <a href=\"/now/\">Now &amp; “then”</a>\n"
									 "    <button type=\"button\"";
	for (auto const& page : {blog::frame_post(model, 0, ""), blog::frame_page(model, 0, ""),
	                         blog::frame_index(model, ""), blog::frame_archive(model)}) {
		EXPECT_NE(page.find(expected_nav), std::string::npos);
		EXPECT_NE(page.find("<a class=\"site-title\" href=\"/\">Jack Diver</a>"),
		          std::string::npos);
	}
}

TEST(frame, a_site_with_no_pages_has_just_the_archive_link)
{
	blog_test::corpus files;
	files.post_title("2026-09-03-a.jimd", "A");
	auto const model = model_of(files);
	auto const page = blog::frame_post(model, 0, "");
	EXPECT_NE(
		page.find("<nav class=\"site-nav\">\n    <a href=\"/archive/\">archive</a>\n    <button"),
		std::string::npos);
}

TEST(frame, a_page_has_the_theme_script_the_stylesheet_the_font_the_icon_and_the_theme_toggle)
{
	blog_test::corpus files;
	files.post_title("2026-09-03-a.jimd", "A");
	auto const model = model_of(files);
	auto const page = blog::frame_post(model, 0, "");
	EXPECT_EQ(page.substr(0, head_start.size()), head_start);
	auto const rest = page.substr(page.find("</title>"));
	EXPECT_EQ(rest.substr(0, head_end.size()), head_end);
	EXPECT_NE(page.find(header_end), std::string::npos);
	EXPECT_NE(page.find("<script src=\"/theme.js\" defer></script>"), std::string::npos);
	EXPECT_EQ(page.find("<footer"), std::string::npos);
	EXPECT_EQ(page.find("class=\"tags\""), std::string::npos);
	EXPECT_TRUE(page.ends_with("</main>\n</body>\n</html>\n"));
}

TEST(frame, an_index_that_is_not_in_the_model_throws)
{
	blog_test::corpus files;
	files.post_title("2026-09-03-a.jimd", "A");
	files.page_title("about.jimd", "About");
	auto const model = model_of(files);
	EXPECT_TRUE(throws_out_of_range(blog::frame_post, model, 1));
	EXPECT_TRUE(throws_out_of_range(blog::frame_page, model, 1));
	EXPECT_FALSE(throws_out_of_range(blog::frame_post, model, 0));
	EXPECT_FALSE(throws_out_of_range(blog::frame_page, model, 0));
}

TEST(frame, the_index_with_no_posts_has_an_empty_main)
{
	blog_test::corpus files;
	files.page_title("about.jimd", "About");
	auto const model = model_of(files);
	auto const page = blog::frame_index(model, "ignored");
	EXPECT_EQ(main_of(page), "");
	EXPECT_EQ(title_of(page), "Jack Diver");
	EXPECT_EQ(page.find("ignored"), std::string::npos);
	EXPECT_NE(page.find("<main>\n</main>\n"), std::string::npos);
}

TEST(frame, the_index_with_one_post_shows_it_and_a_link_to_one_post)
{
	blog_test::corpus files;
	files.post_title("2026-09-03-a.jimd", "Only", "s");
	auto const model = model_of(files);
	auto const page = blog::frame_index(model, "<p>Body.</p>\n");
	EXPECT_EQ(main_of(page), R"(  <article class="post">
    <h1>Only</h1>
    <nav class="post-meta">
      <span class="date">3 September 2026</span>
    </nav>
<p>Body.</p>
  </article>
  <p class="more"><a href="/archive/">All 1 post…</a></p>
)");
	EXPECT_EQ(title_of(page), "Jack Diver");
}

TEST(frame, the_index_with_two_posts_shows_the_newest_and_counts_both)
{
	blog_test::corpus files;
	files.post_title("2026-09-03-a.jimd", "Old", "s");
	files.post_title("2026-09-04-b.jimd", "New", "s");
	auto const model = model_of(files);
	auto const page = blog::frame_index(model, "<p>Body.</p>");
	EXPECT_EQ(main_of(page), R"(  <article class="post">
    <h1>New</h1>
    <nav class="post-meta">
      <span class="date">4 September 2026</span>
    </nav>
<p>Body.</p>
  </article>
  <p class="more"><a href="/archive/">All 2 posts…</a></p>
)");
	EXPECT_EQ(page.find("series-"), std::string::npos);
	EXPECT_EQ(page.find("post-nav"), std::string::npos);
	EXPECT_EQ(page.find("Old"), std::string::npos);
}

TEST(frame, the_archive_with_no_posts_has_just_its_heading)
{
	blog_test::corpus files;
	auto const model = model_of(files);
	auto const page = blog::frame_archive(model);
	EXPECT_EQ(main_of(page), R"(  <article class="archive-index">
    <h1>Archive</h1>
  </article>
)");
	EXPECT_EQ(title_of(page), "Archive – Jack Diver");
	EXPECT_EQ(page.find("<table"), std::string::npos);
}

TEST(frame, the_archive_groups_posts_by_year)
{
	blog_test::corpus files;
	files.post_title("2025-12-31-old.jimd", "Old");
	files.post_title("2026-01-05-a.jimd", "It's A");
	files.post_title("2026-09-03-b.jimd", "B & C");
	files.post_title("2026-09-03-c.jimd", "C");
	auto const model = model_of(files);
	EXPECT_EQ(main_of(blog::frame_archive(model)), R"(  <article class="archive-index">
    <h1>Archive</h1>
    <table class="archive">
      <tbody>
        <tr class="year-start">
          <th class="year" scope="rowgroup">2026</th>
          <td class="title"><a href="/2026/09/03/c/">C</a></td>
          <td class="date">3 sep</td>
        </tr>
        <tr>
          <th class="year" scope="row"></th>
          <td class="title"><a href="/2026/09/03/b/">B &amp; C</a></td>
          <td class="date">3 sep</td>
        </tr>
        <tr>
          <th class="year" scope="row"></th>
          <td class="title"><a href="/2026/01/05/a/">It’s A</a></td>
          <td class="date">5 jan</td>
        </tr>
        <tr class="year-start">
          <th class="year" scope="rowgroup">2025</th>
          <td class="title"><a href="/2025/12/31/old/">Old</a></td>
          <td class="date">31 dec</td>
        </tr>
      </tbody>
    </table>
  </article>
)");
}

TEST(frame, the_not_found_page_links_home_and_to_the_archive_from_the_root)
{
	blog_test::corpus files;
	files.page_title("about.jimd", "About");
	auto const model = model_of(files);
	auto const page = blog::frame_not_found(model);
	EXPECT_EQ(main_of(page), R"(  <article class="page">
    <h1>Page not found</h1>
    <p>There is nothing here. Try the <a href="/">home page</a> or the <a href="/archive/">archive</a>.</p>
  </article>
)");
	EXPECT_EQ(title_of(page), "Page not found – Jack Diver");
	EXPECT_NE(page.find("<a href=\"/about/\">About</a>"), std::string::npos);
	EXPECT_EQ(page.find("description"), std::string::npos);
}

TEST(frame, a_page_is_a_heading_and_its_fragment)
{
	blog_test::corpus files;
	files.page_title("about.jimd", "About \"me\"");
	auto const model = model_of(files);
	auto const page = blog::frame_page(model, 0, "<p>Hello.</p>\n");
	EXPECT_EQ(main_of(page), R"(  <article class="page">
    <h1>About “me”</h1>
<p>Hello.</p>
  </article>
)");
	EXPECT_EQ(title_of(page), "About “me” – Jack Diver");
	EXPECT_EQ(page.find("post-meta"), std::string::npos);
}

TEST(frame, the_pages_are_the_same_each_time_and_have_no_build_dependent_parts)
{
	blog_test::corpus files;
	files.post_title("2026-09-03-a.jimd", "A", "s");
	files.post_title("2026-09-04-b.jimd", "B", "s");
	files.page_title("about.jimd", "About");
	auto const model = model_of(files);
	EXPECT_EQ(blog::frame_post(model, 0, "x"), blog::frame_post(model, 0, "x"));
	EXPECT_EQ(blog::frame_index(model, "x"), blog::frame_index(model, "x"));
	EXPECT_EQ(blog::frame_archive(model), blog::frame_archive(model));
	EXPECT_EQ(blog::frame_page(model, 0, "x"), blog::frame_page(model, 0, "x"));
	auto const page = blog::frame_post(model, 0, "x");
	EXPECT_EQ(page.find("http"), std::string::npos);
	EXPECT_EQ(page.find("?v="), std::string::npos);
}
