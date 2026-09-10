#pragma once

#include <deque>
#include <expected>
#include <filesystem>
#include <string>
#include <string_view>
#include <utility>
#include <vector>
#include <gtest/gtest.h>

#include <blog/site.hxx>
#include <jimd/ast.hxx>
#include <jimd/parse.hxx>

namespace blog_test
{
/**
 * class corpus - An in-memory set of posts and pages to pass to blog::build_site().
 *
 * It owns the sources and parsed documents that the inputs refer to, so it cannot be copied
 * or moved.
 */
class corpus {
public:
	corpus() = default;
	corpus(corpus const&) = delete;
	corpus(corpus&&) = delete;
	auto operator=(corpus const&) -> corpus& = delete;
	auto operator=(corpus&&) -> corpus& = delete;
	~corpus() = default;

	/**
	 * post() - Parse and add a post under ``content/posts/``.
	 * @name: The file name, such as ``2024-01-02-slug.jimd``.
	 * @source: The jimd source. A parse error fails the current test.
	 */
	auto post(std::string_view name, std::string_view source) -> void
	{
		add(posts_, std::string{"content/posts/"} + std::string{name}, source);
	}

	/**
	 * page() - Parse and add a page under ``content/pages/``.
	 * @name: The file name.
	 * @source: The jimd source. A parse error fails the current test.
	 */
	auto page(std::string_view name, std::string_view source) -> void
	{
		add(pages_, std::string{"content/pages/"} + std::string{name}, source);
	}

	/**
	 * post_title() - Add a post whose source is only its metadata.
	 * @name: The file name.
	 * @title: The ``title`` value.
	 * @series: The ``series`` value; empty to leave it out.
	 */
	// NOLINTNEXTLINE(bugprone-easily-swappable-parameters)
	auto post_title(std::string_view name, std::string_view title, std::string_view series = {})
		-> void
	{
		std::string source = "[%title]: " + std::string{title} + "\n";
		if (!series.empty()) {
			source += "[%series]: " + std::string{series} + "\n";
		}
		post(name, source);
	}

	/**
	 * page_title() - Add a page whose source is only its title.
	 * @name: The file name.
	 * @title: The ``title`` value.
	 */
	auto page_title(std::string_view name, std::string_view title) -> void
	{
		page(name, "[%title]: " + std::string{title} + "\n");
	}

	/**
	 * build() - Build the site model from everything added so far.
	 *
	 * Return: What blog::build_site() returns.
	 */
	[[nodiscard]] auto build() const
		-> std::expected<blog::site_model, std::vector<blog::file_error>>
	{
		return blog::build_site({.posts = posts_, .pages = pages_});
	}

private:
	auto add(std::vector<blog::input>& into, std::string path, std::string_view source) -> void
	{
		auto const& text = sources_.emplace_back(source);
		auto doc = jimd::parse(text);
		EXPECT_TRUE(doc.has_value()) << path;
		auto const& kept = docs_.emplace_back(*std::move(doc));
		into.push_back({.path = std::filesystem::path{std::move(path)}, .doc = kept});
	}

	std::deque<std::string> sources_;
	std::deque<jimd::ast::document> docs_;
	std::vector<blog::input> posts_;
	std::vector<blog::input> pages_;
};
}
