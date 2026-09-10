#include <blog/site.hxx>

#include <algorithm>
#include <compare>
#include <cstddef>
#include <expected>
#include <map>
#include <optional>
#include <ranges>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <blog/error.hxx>
#include <blog/post.hxx>

namespace blog
{
namespace
{
auto newer_first(post_entry const& lhs, post_entry const& rhs) -> bool
{
	if (auto const order = lhs.item.date <=> rhs.item.date; order != 0) {
		return order > 0;
	}
	return lhs.item.slug > rhs.item.slug;
}

auto read_posts(std::span<input const> files, std::vector<post_entry>& out,
                std::vector<file_error>& errors) -> void
{
	std::size_t index = 0;
	for (auto const& file : files) {
		auto item = read_post(file.path.filename().string(), file.doc);
		if (item) {
			out.push_back(post_entry{.item = *std::move(item),
			                         .source = index,
			                         .older = std::nullopt,
			                         .newer = std::nullopt,
			                         .series = std::nullopt});
		} else {
			errors.push_back({.path = file.path, .error = std::move(item).error()});
		}
		++index;
	}
}

auto read_pages(std::span<input const> files, std::vector<page_entry>& out,
                std::vector<file_error>& errors) -> void
{
	std::size_t index = 0;
	for (auto const& file : files) {
		auto item = read_page(file.path.filename().string(), file.doc);
		if (item) {
			out.push_back(page_entry{.item = *std::move(item), .source = index});
		} else {
			errors.push_back({.path = file.path, .error = std::move(item).error()});
		}
		++index;
	}
}

auto link_neighbours(std::vector<post_entry>& posts) -> void
{
	for (std::size_t idx = 0; idx < posts.size(); ++idx) {
		if (idx + 1 < posts.size()) {
			posts.at(idx).older = idx + 1;
		}
		if (idx > 0) {
			posts.at(idx).newer = idx - 1;
		}
	}
}

auto group_series(std::vector<post_entry>& posts) -> std::vector<series_group>
{
	std::map<std::string, std::vector<std::size_t>, std::less<>> by_name;
	for (std::size_t idx = posts.size(); idx > 0; --idx) {
		if (auto const& name = posts.at(idx - 1).item.series) {
			by_name[*name].push_back(idx - 1);
		}
	}
	std::vector<series_group> groups;
	for (auto& [name, members] : by_name) {
		if (members.size() < 2) {
			continue;
		}
		for (std::size_t part = 0; part < members.size(); ++part) {
			posts.at(members.at(part)).series =
				series_position{.group = groups.size(), .part = part};
		}
		groups.push_back({.name = name, .posts = std::move(members)});
	}
	return groups;
}
}

auto build_site(site_files const& files) -> std::expected<site_model, std::vector<file_error>>
{
	site_model model;
	std::vector<file_error> errors;
	read_posts(files.posts, model.posts, errors);
	read_pages(files.pages, model.pages, errors);
	if (!errors.empty()) {
		std::ranges::stable_sort(errors, {}, &file_error::path);
		return std::unexpected(std::move(errors));
	}

	std::ranges::stable_sort(model.posts, newer_first);
	std::ranges::stable_sort(model.pages, {}, [](page_entry const& entry) -> std::string_view {
		return entry.item.slug;
	});
	link_neighbours(model.posts);
	model.series = group_series(model.posts);
	return model;
}
}
