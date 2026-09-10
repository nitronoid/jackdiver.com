#pragma once

#include <cstddef>
#include <expected>
#include <filesystem>
#include <functional>
#include <optional>
#include <span>
#include <string>
#include <vector>

#include <blog/error.hxx>
#include <blog/post.hxx>
#include <jimd/ast.hxx>

namespace blog
{
/**
 * struct input - A parsed source file.
 */
struct input {
	/** @path: Path of the file; its name gives the date and slug. */
	std::filesystem::path path;
	/** @doc: The parsed file, which must outlive this. */
	std::reference_wrapper<jimd::ast::document const> doc;
};

/**
 * struct site_files - The parsed sources of a site.
 */
struct site_files {
	/** @posts: The posts, in any order. */
	std::span<input const> posts;
	/** @pages: The pages, in any order. */
	std::span<input const> pages;
};

/**
 * struct file_error - An error and the file it came from.
 */
struct file_error {
	/** @path: The file at fault. */
	std::filesystem::path path;
	/** @error: What went wrong. */
	blog::error error;

	auto operator==(file_error const&) const -> bool = default;
};

/**
 * struct series_position - Where a post sits in its series.
 */
struct series_position {
	/** @group: Index of the series in site_model::series. */
	std::size_t group{};
	/** @part: 0-based index of the post within the series. */
	std::size_t part{};

	auto operator==(series_position const&) const -> bool = default;
};

/**
 * struct post_entry - A post and its place in the site.
 */
struct post_entry {
	/** @item: The post. */
	blog::post item;
	/** @source: Index of the post in site_files::posts. */
	std::size_t source{};
	/**
	 * @older: Index in site_model::posts of the next older post, if any.
	 * @newer: Index in site_model::posts of the next newer post, if any.
	 */
	std::optional<std::size_t> older, newer;
	/** @series: The post's place in its series, if the series has two or more posts. */
	std::optional<series_position> series;
};

/**
 * struct page_entry - A page and where it came from.
 */
struct page_entry {
	/** @item: The page. */
	blog::page item;
	/** @source: Index of the page in site_files::pages. */
	std::size_t source{};
};

/**
 * struct series_group - The posts of one series.
 */
struct series_group {
	/** @name: The series name, as given in the posts' metadata. */
	std::string name;
	/** @posts: Indices into site_model::posts, oldest first. */
	std::vector<std::size_t> posts;
};

/**
 * struct site_model - Everything needed to lay out the site.
 */
struct site_model {
	/** @posts: The posts, newest first. */
	std::vector<post_entry> posts;
	/** @pages: The pages, sorted by slug. */
	std::vector<page_entry> pages;
	/** @series: The series with two or more posts, sorted by name. */
	std::vector<series_group> series;
};

/**
 * build_site() - Read, order and link the posts and pages.
 * @files: The parsed sources.
 *
 * A page may not use the slug archive or a four-digit year, since those paths are generated.
 *
 * Return: The model, or every error found, sorted by path.
 */
[[nodiscard]] auto build_site(site_files const& files)
	-> std::expected<site_model, std::vector<file_error>>;
}
