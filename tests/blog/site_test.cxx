#include <cstddef>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>
#include <gtest/gtest.h>

#include <blog/error.hxx>
#include <blog/post.hxx>
#include <blog/site.hxx>
#include <blog_test/corpus.hxx>
#include <jimd/position.hxx>
#include <support/result_printers.hxx>

using namespace std::literals;

namespace
{
auto slugs(blog::site_model const& model) -> std::vector<std::string>
{
	std::vector<std::string> out;
	out.reserve(model.posts.size());
	for (auto const& entry : model.posts) {
		out.push_back(entry.item.slug);
	}
	return out;
}

auto page_slugs(blog::site_model const& model) -> std::vector<std::string>
{
	std::vector<std::string> out;
	out.reserve(model.pages.size());
	for (auto const& entry : model.pages) {
		out.push_back(entry.item.slug);
	}
	return out;
}

auto code_of(blog::file_error const& err) -> blog::errc
{
	return std::get<blog::errc>(err.error.code);
}

using strings = std::vector<std::string>;
}

TEST(site_model, no_files_make_an_empty_model)
{
	blog_test::corpus files;
	auto const model = files.build();
	ASSERT_TRUE(model.has_value());
	EXPECT_TRUE(model->posts.empty());
	EXPECT_TRUE(model->pages.empty());
	EXPECT_TRUE(model->series.empty());
}

TEST(site_model, posts_are_newest_first)
{
	blog_test::corpus files;
	files.post_title("2026-01-05-b.jimd", "B");
	files.post_title("2026-09-03-d.jimd", "D");
	files.post_title("2025-12-31-a.jimd", "A");
	files.post_title("2026-02-01-c.jimd", "C");
	auto const model = files.build();
	ASSERT_TRUE(model.has_value());
	EXPECT_EQ(slugs(*model), (strings{"d", "c", "b", "a"}));
	EXPECT_EQ(model->posts.front().item.title, "D");
}

TEST(site_model, posts_of_one_day_are_ordered_by_slug_with_the_later_slug_first)
{
	blog_test::corpus files;
	files.post_title("2026-09-03-a.jimd", "A");
	files.post_title("2026-09-03-c.jimd", "C");
	files.post_title("2026-09-03-b.jimd", "B");
	files.post_title("2026-09-02-z.jimd", "Z");
	auto const model = files.build();
	ASSERT_TRUE(model.has_value());
	EXPECT_EQ(slugs(*model), (strings{"c", "b", "a", "z"}));
}

TEST(site_model, a_post_keeps_the_index_of_its_input)
{
	blog_test::corpus files;
	files.post_title("2026-01-01-first.jimd", "First");
	files.post_title("2026-03-01-third.jimd", "Third");
	files.post_title("2026-02-01-second.jimd", "Second");
	auto const model = files.build();
	ASSERT_TRUE(model.has_value());
	ASSERT_EQ(model->posts.size(), 3U);
	EXPECT_EQ(model->posts.at(0).item.slug, "third");
	EXPECT_EQ(model->posts.at(0).source, 1U);
	EXPECT_EQ(model->posts.at(1).source, 2U);
	EXPECT_EQ(model->posts.at(2).source, 0U);
}

TEST(site_model, the_order_does_not_depend_on_the_order_of_the_inputs)
{
	blog_test::corpus forward;
	forward.post_title("2026-01-01-a.jimd", "A", "s");
	forward.post_title("2026-01-01-b.jimd", "B", "s");
	forward.post_title("2026-05-01-c.jimd", "C");
	forward.page_title("zeta.jimd", "Zeta");
	forward.page_title("about.jimd", "About");
	blog_test::corpus backward;
	backward.post_title("2026-05-01-c.jimd", "C");
	backward.post_title("2026-01-01-b.jimd", "B", "s");
	backward.post_title("2026-01-01-a.jimd", "A", "s");
	backward.page_title("about.jimd", "About");
	backward.page_title("zeta.jimd", "Zeta");
	auto const lhs = forward.build();
	auto const rhs = backward.build();
	ASSERT_TRUE(lhs.has_value());
	ASSERT_TRUE(rhs.has_value());
	EXPECT_EQ(slugs(*lhs), slugs(*rhs));
	EXPECT_EQ(page_slugs(*lhs), page_slugs(*rhs));
	ASSERT_EQ(lhs->series.size(), 1U);
	ASSERT_EQ(rhs->series.size(), 1U);
	EXPECT_EQ(lhs->series.front().posts, rhs->series.front().posts);
	for (std::size_t idx = 0; idx < lhs->posts.size(); ++idx) {
		EXPECT_EQ(lhs->posts.at(idx).item, rhs->posts.at(idx).item);
		EXPECT_EQ(lhs->posts.at(idx).older, rhs->posts.at(idx).older);
		EXPECT_EQ(lhs->posts.at(idx).newer, rhs->posts.at(idx).newer);
		EXPECT_EQ(lhs->posts.at(idx).series, rhs->posts.at(idx).series);
	}
}

TEST(site_model, older_and_newer_are_the_neighbours_in_time)
{
	blog_test::corpus files;
	files.post_title("2026-03-01-c.jimd", "C");
	files.post_title("2026-01-01-a.jimd", "A");
	files.post_title("2026-02-01-b.jimd", "B");
	auto const model = files.build();
	ASSERT_TRUE(model.has_value());
	EXPECT_EQ(model->posts.at(0).newer, std::nullopt);
	EXPECT_EQ(model->posts.at(0).older, 1U);
	EXPECT_EQ(model->posts.at(1).newer, 0U);
	EXPECT_EQ(model->posts.at(1).older, 2U);
	EXPECT_EQ(model->posts.at(2).newer, 1U);
	EXPECT_EQ(model->posts.at(2).older, std::nullopt);
}

TEST(site_model, a_lone_post_has_no_neighbours)
{
	blog_test::corpus files;
	files.post_title("2026-01-01-a.jimd", "A");
	auto const model = files.build();
	ASSERT_TRUE(model.has_value());
	EXPECT_EQ(model->posts.front().older, std::nullopt);
	EXPECT_EQ(model->posts.front().newer, std::nullopt);
}

TEST(site_model, posts_of_one_day_are_neighbours_in_the_same_order)
{
	blog_test::corpus files;
	files.post_title("2026-09-03-a.jimd", "A");
	files.post_title("2026-09-03-b.jimd", "B");
	auto const model = files.build();
	ASSERT_TRUE(model.has_value());
	EXPECT_EQ(slugs(*model), (strings{"b", "a"}));
	EXPECT_EQ(model->posts.at(0).older, 1U);
	EXPECT_EQ(model->posts.at(1).newer, 0U);
}

TEST(site_model, a_post_alone_in_its_series_has_none)
{
	blog_test::corpus files;
	files.post_title("2026-01-01-a.jimd", "A", "pi");
	files.post_title("2026-02-01-b.jimd", "B");
	auto const model = files.build();
	ASSERT_TRUE(model.has_value());
	EXPECT_TRUE(model->series.empty());
	for (auto const& entry : model->posts) {
		EXPECT_EQ(entry.series, std::nullopt);
	}
	EXPECT_EQ(model->posts.at(1).item.series, "pi");
}

TEST(site_model, two_posts_make_a_series_from_the_oldest)
{
	blog_test::corpus files;
	files.post_title("2026-05-01-two.jimd", "Two", "pi");
	files.post_title("2026-01-01-one.jimd", "One", "pi");
	auto const model = files.build();
	ASSERT_TRUE(model.has_value());
	ASSERT_EQ(model->series.size(), 1U);
	EXPECT_EQ(model->series.front().name, "pi");
	EXPECT_EQ(model->series.front().posts, (std::vector<std::size_t>{1, 0}));
	EXPECT_EQ(model->posts.at(1).series, (blog::series_position{.group = 0, .part = 0}));
	EXPECT_EQ(model->posts.at(0).series, (blog::series_position{.group = 0, .part = 1}));
}

TEST(site_model, a_series_among_other_posts)
{
	blog_test::corpus files;
	files.post_title("2026-01-01-a.jimd", "A", "pi");
	files.post_title("2026-02-01-x.jimd", "X");
	files.post_title("2026-03-01-b.jimd", "B", "pi");
	files.post_title("2026-04-01-y.jimd", "Y", "other");
	files.post_title("2026-05-01-c.jimd", "C", "pi");
	auto const model = files.build();
	ASSERT_TRUE(model.has_value());
	EXPECT_EQ(slugs(*model), (strings{"c", "y", "b", "x", "a"}));
	ASSERT_EQ(model->series.size(), 1U);
	EXPECT_EQ(model->series.front().posts, (std::vector<std::size_t>{4, 2, 0}));
	EXPECT_EQ(model->posts.at(4).series, (blog::series_position{.group = 0, .part = 0}));
	EXPECT_EQ(model->posts.at(2).series, (blog::series_position{.group = 0, .part = 1}));
	EXPECT_EQ(model->posts.at(0).series, (blog::series_position{.group = 0, .part = 2}));
	EXPECT_EQ(model->posts.at(1).series, std::nullopt);
	EXPECT_EQ(model->posts.at(3).series, std::nullopt);
	EXPECT_EQ(model->posts.at(2).older, 3U);
	EXPECT_EQ(model->posts.at(2).newer, 1U);
}

TEST(site_model, a_series_of_one_day_follows_the_slugs)
{
	blog_test::corpus files;
	files.post_title("2026-09-03-part-2.jimd", "Two", "s");
	files.post_title("2026-09-03-part-1.jimd", "One", "s");
	auto const model = files.build();
	ASSERT_TRUE(model.has_value());
	EXPECT_EQ(slugs(*model), (strings{"part-2", "part-1"}));
	ASSERT_EQ(model->series.size(), 1U);
	EXPECT_EQ(model->series.front().posts, (std::vector<std::size_t>{1, 0}));
}

TEST(site_model, slugs_of_one_day_are_in_plain_string_order)
{
	blog_test::corpus files;
	files.post_title("2026-09-03-part-2.jimd", "Two", "s");
	files.post_title("2026-09-03-part-10.jimd", "Ten", "s");
	auto const model = files.build();
	ASSERT_TRUE(model.has_value());
	EXPECT_EQ(slugs(*model), (strings{"part-2", "part-10"}));
	ASSERT_EQ(model->series.size(), 1U);
	EXPECT_EQ(model->series.front().posts, (std::vector<std::size_t>{1, 0}));
}

TEST(site_model, the_model_does_not_refer_to_the_inputs)
{
	std::optional<blog::site_model> kept;
	{
		blog_test::corpus files;
		files.post_title("2026-01-01-a.jimd", "A", "s");
		files.post_title("2026-01-02-b.jimd", "B", "s");
		files.page_title("about.jimd", "About");
		auto model = files.build();
		ASSERT_TRUE(model.has_value());
		kept = *std::move(model);
	}
	ASSERT_EQ(kept->posts.size(), 2U);
	EXPECT_EQ(kept->posts.front().item.title, "B");
	EXPECT_EQ(kept->series.front().name, "s");
	EXPECT_EQ(kept->pages.front().item.title, "About");
}

TEST(site_model, series_are_told_apart_by_their_exact_name)
{
	blog_test::corpus files;
	files.post_title("2026-01-01-a.jimd", "A", "Pi");
	files.post_title("2026-01-02-b.jimd", "B", "pi");
	files.post_title("2026-01-03-c.jimd", "C", "pi ");
	files.post_title("2026-01-04-d.jimd", "D", "pi");
	auto const model = files.build();
	ASSERT_TRUE(model.has_value());
	ASSERT_EQ(model->series.size(), 1U);
	EXPECT_EQ(model->series.front().name, "pi");
	EXPECT_EQ(model->series.front().posts.size(), 3U);
	EXPECT_EQ(model->posts.at(3).series, std::nullopt);
}

TEST(site_model, several_series_are_listed_by_name)
{
	blog_test::corpus files;
	files.post_title("2026-01-01-a.jimd", "A", "zeta");
	files.post_title("2026-01-02-b.jimd", "B", "alpha");
	files.post_title("2026-01-03-c.jimd", "C", "zeta");
	files.post_title("2026-01-04-d.jimd", "D", "alpha");
	auto const model = files.build();
	ASSERT_TRUE(model.has_value());
	ASSERT_EQ(model->series.size(), 2U);
	EXPECT_EQ(model->series.at(0).name, "alpha");
	EXPECT_EQ(model->series.at(1).name, "zeta");
	EXPECT_EQ(model->series.at(0).posts, (std::vector<std::size_t>{2, 0}));
	EXPECT_EQ(model->series.at(1).posts, (std::vector<std::size_t>{3, 1}));
	EXPECT_EQ(model->posts.at(0).series, (blog::series_position{.group = 0, .part = 1}));
	EXPECT_EQ(model->posts.at(1).series, (blog::series_position{.group = 1, .part = 1}));
}

TEST(site_model, pages_are_ordered_by_slug)
{
	blog_test::corpus files;
	files.page_title("colophon.jimd", "Colophon");
	files.page_title("about.jimd", "About");
	files.page_title("now.jimd", "Now");
	auto const model = files.build();
	ASSERT_TRUE(model.has_value());
	EXPECT_EQ(page_slugs(*model), (strings{"about", "colophon", "now"}));
	EXPECT_EQ(model->pages.front().item.title, "About");
	EXPECT_EQ(model->pages.front().source, 1U);
	EXPECT_EQ(model->pages.back().source, 2U);
}

TEST(site_model, a_page_cannot_take_a_path_the_site_writes)
{
	struct example {
		std::string_view name;
		std::string_view message;
	};
	for (auto const& [name, message] : {
			 example{.name = "archive.jimd",
			         .message = "page collides with a generated path: /archive/ (the archive)"},
			 example{.name = "2024.jimd",
			         .message = "page collides with a generated path: /2024/ (posts are written "
			                    "under it)"},
		 }) {
		blog_test::corpus files;
		files.page_title("about.jimd", "About");
		files.page_title(name, "T");
		auto const model = files.build();
		ASSERT_FALSE(model.has_value()) << name;
		ASSERT_EQ(model.error().size(), 1U) << name;
		auto const& err = model.error().front();
		auto const path = "content/pages/"s + std::string{name};
		EXPECT_EQ(err.path, std::filesystem::path{path});
		EXPECT_EQ(code_of(err), blog::errc::page_collision) << name;
		EXPECT_EQ(blog::format_error(err.error, err.path.string()),
		          path + ": " + std::string{message});
	}
}

TEST(site_model, every_faulty_file_is_reported)
{
	blog_test::corpus files;
	files.post_title("2026-09-03-fine.jimd", "Fine");
	files.post_title("2026-02-30-bad-date.jimd", "Bad date");
	files.post("2026-09-04-no-title.jimd", "Body.\n");
	files.post("2026-09-05-tags.jimd", "[%title]: T\n[%tags]: a\n");
	files.page_title("About Me.jimd", "About");
	files.page_title("archive.jimd", "Archive");
	files.page("empty.jimd", "Body.\n");
	files.page_title("fine.jimd", "Fine");
	auto const model = files.build();
	ASSERT_FALSE(model.has_value());
	auto const& errors = model.error();
	ASSERT_EQ(errors.size(), 6U);
	EXPECT_EQ(errors.at(0).path, "content/pages/About Me.jimd");
	EXPECT_EQ(code_of(errors.at(0)), blog::errc::bad_page_name);
	EXPECT_EQ(errors.at(1).path, "content/pages/archive.jimd");
	EXPECT_EQ(code_of(errors.at(1)), blog::errc::page_collision);
	EXPECT_EQ(errors.at(2).path, "content/pages/empty.jimd");
	EXPECT_EQ(code_of(errors.at(2)), blog::errc::missing_title);
	EXPECT_EQ(errors.at(3).path, "content/posts/2026-02-30-bad-date.jimd");
	EXPECT_EQ(code_of(errors.at(3)), blog::errc::bad_post_name);
	EXPECT_EQ(errors.at(4).path, "content/posts/2026-09-04-no-title.jimd");
	EXPECT_EQ(code_of(errors.at(4)), blog::errc::missing_title);
	EXPECT_EQ(errors.at(5).path, "content/posts/2026-09-05-tags.jimd");
	EXPECT_EQ(errors.at(5).error,
	          (blog::error{blog::errc::unknown_metadata, jimd::position{.line = 2, .column = 3},
	                       "tags (a post has title, series, description)"}));
}

TEST(site_model, the_errors_do_not_depend_on_the_order_of_the_inputs)
{
	blog_test::corpus forward;
	forward.post("2026-09-04-b.jimd", "Body.\n");
	forward.post("2026-09-03-a.jimd", "Body.\n");
	blog_test::corpus backward;
	backward.post("2026-09-03-a.jimd", "Body.\n");
	backward.post("2026-09-04-b.jimd", "Body.\n");
	auto const lhs = forward.build();
	auto const rhs = backward.build();
	ASSERT_FALSE(lhs.has_value());
	ASSERT_FALSE(rhs.has_value());
	EXPECT_EQ(lhs.error(), rhs.error());
	EXPECT_EQ(lhs.error().front().path, "content/posts/2026-09-03-a.jimd");
}

TEST(site_model, a_faulty_file_does_not_hide_another_fault)
{
	blog_test::corpus files;
	files.post("2026-09-03-a.jimd", "Body.\n");
	files.page_title("archive.jimd", "Archive");
	auto const model = files.build();
	ASSERT_FALSE(model.has_value());
	EXPECT_EQ(model.error().size(), 2U);
}

TEST(site_model, the_error_of_a_file_is_the_one_read_post_gives)
{
	blog_test::corpus files;
	files.post("2026-09-03-a.jimd", "[%titel]: T\n");
	auto const model = files.build();
	ASSERT_FALSE(model.has_value());
	ASSERT_EQ(model.error().size(), 1U);
	EXPECT_EQ(model.error().front().error,
	          (blog::error{blog::errc::unknown_metadata, jimd::position{.line = 1, .column = 3},
	                       "titel (a post has title, series, description)"}));
}
