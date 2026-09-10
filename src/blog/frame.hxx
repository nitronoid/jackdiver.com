#pragma once

#include <cstddef>
#include <string>
#include <string_view>

#include <blog/site.hxx>

namespace blog
{
/**
 * frame_post() - Wrap a post's body in a full HTML document.
 * @model: The site, for navigation.
 * @index: Index of the post in @model's posts.
 * @fragment: The post's rendered body.
 *
 * Adds the title, date, series navigation and links to the adjacent posts.
 *
 * Return: The HTML document.
 */
[[nodiscard]] auto frame_post(site_model const& model, std::size_t index, std::string_view fragment)
	-> std::string;

/**
 * frame_page() - Wrap a page's body in a full HTML document.
 * @model: The site, for navigation.
 * @index: Index of the page in @model's pages.
 * @fragment: The page's rendered body.
 *
 * Return: The HTML document.
 */
[[nodiscard]] auto frame_page(site_model const& model, std::size_t index, std::string_view fragment)
	-> std::string;

/**
 * frame_index() - Build the home page.
 * @model: The site.
 * @fragment: The newest post's rendered body; ignored if there are no posts.
 *
 * Return: The HTML document, showing the newest post and a link to the archive.
 */
[[nodiscard]] auto frame_index(site_model const& model, std::string_view fragment) -> std::string;

/**
 * frame_archive() - Build the archive page.
 * @model: The site.
 *
 * Return: The HTML document, listing every post newest first, grouped by year.
 */
[[nodiscard]] auto frame_archive(site_model const& model) -> std::string;

/**
 * frame_not_found() - Build the page shown for a missing URL.
 * @model: The site, for navigation.
 *
 * Every link in it is from the root, so it works served at any path.
 *
 * Return: The HTML document, with links to the home page and the archive.
 */
[[nodiscard]] auto frame_not_found(site_model const& model) -> std::string;
}
