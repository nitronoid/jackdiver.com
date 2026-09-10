#pragma once

#include <chrono>
#include <expected>
#include <optional>
#include <string>
#include <string_view>

#include <blog/error.hxx>
#include <jimd/ast.hxx>

namespace blog
{
/**
 * struct post_name - The parts of a post's file name.
 */
struct post_name {
	/** @date: Publication date. */
	std::chrono::year_month_day date;
	/** @slug: Name used in the post's URL. */
	std::string slug;

	auto operator==(post_name const&) const -> bool = default;
};

/**
 * struct post - A blog post, from its file name and metadata.
 */
struct post {
	/** @date: Publication date. */
	std::chrono::year_month_day date;
	/** @slug: Name used in the post's URL. */
	std::string slug;
	/** @title: Title, unescaped. */
	std::string title;
	/** @series: Name of the series the post belongs to, if any. */
	std::optional<std::string> series;
	/** @description: Summary for the description meta tag, if any. */
	std::optional<std::string> description;

	auto operator==(post const&) const -> bool = default;
};

/**
 * struct page - A standalone page, such as About.
 */
struct page {
	/** @slug: Name used in the page's URL. */
	std::string slug;
	/** @title: Title, unescaped; also the navigation link text. */
	std::string title;
	/** @description: Summary for the description meta tag, if any. */
	std::optional<std::string> description;

	auto operator==(page const&) const -> bool = default;
};

/**
 * parse_post_name() - Split a post's file name into its date and slug.
 * @file_name: Name of the form YYYY-MM-DD-slug.jimd.
 *
 * A slug is lower-case letters and digits joined by single hyphens.
 *
 * Return: The date and slug, or an errc::bad_post_name error saying why.
 */
auto parse_post_name(std::string_view file_name) -> std::expected<post_name, error>;

/**
 * parse_page_name() - Get a page's slug from its file name.
 * @file_name: Name of the form slug.jimd.
 *
 * Return: The slug, or an errc::bad_page_name error saying why.
 */
auto parse_page_name(std::string_view file_name) -> std::expected<std::string, error>;

/**
 * check_extension() - Check that a file name ends in .jimd.
 * @code: The code of the error, errc::bad_post_name or errc::bad_page_name.
 * @file_name: The file's name.
 *
 * This is the first rule of parse_post_name() and parse_page_name(), for a caller that must reject
 * a file before reading it.
 *
 * Return: Nothing, or an error of @code saying the name must end in .jimd.
 */
auto check_extension(errc code, std::string_view file_name) -> std::expected<void, error>;

/**
 * read_post() - Build a post from its file name and parsed document.
 * @file_name: The post's file name.
 * @doc: The parsed post.
 *
 * The metadata must have a non-empty title, and only the keys title, series and description.
 *
 * Return: The post, or the first error found.
 */
auto read_post(std::string_view file_name, jimd::ast::document const& doc)
	-> std::expected<post, error>;

/**
 * read_page() - Build a page from its file name and parsed document.
 * @file_name: The page's file name.
 * @doc: The parsed page.
 *
 * The metadata must have a non-empty title, and only the keys title and description. The slug may
 * not be archive or a four-digit year, since the site generates those paths.
 *
 * Return: The page, or the first error found, checking the name, then the metadata, then the slug
 * (errc::page_collision).
 */
auto read_page(std::string_view file_name, jimd::ast::document const& doc)
	-> std::expected<page, error>;

/**
 * url() - Get the URL path of a post.
 * @item: The post.
 *
 * Return: A path of the form /YYYY/MM/DD/slug/.
 */
auto url(post const& item) -> std::string;

/**
 * url() - Get the URL path of a page.
 * @item: The page.
 *
 * Return: A path of the form /slug/.
 */
auto url(page const& item) -> std::string;

/**
 * long_date() - Format a date in full, such as "5 March 2024".
 * @date: A valid date; std::out_of_range is thrown otherwise.
 *
 * Return: The formatted date.
 */
auto long_date(std::chrono::year_month_day date) -> std::string;

/**
 * short_date() - Format a day and month, such as "5 mar".
 * @date: A valid date; std::out_of_range is thrown otherwise.
 *
 * Return: The formatted date.
 */
auto short_date(std::chrono::year_month_day date) -> std::string;
}
