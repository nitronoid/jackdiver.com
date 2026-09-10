#include <blog/post.hxx>

#include <algorithm>
#include <array>
#include <chrono>
#include <cstddef>
#include <expected>
#include <format>
#include <optional>
#include <ranges>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <tuple>
#include <utility>

#include <blog/error.hxx>
#include <jimd/ast.hxx>
#include <jimd/position.hxx>
#include <jimd/result.hxx>

using namespace std::literals;

namespace blog
{
namespace
{
constexpr auto extension = ".jimd"sv;
constexpr auto date_pattern = "dddd-dd-dd-"sv;
constexpr std::size_t year_size = 4;
constexpr std::size_t month_at = 5;
constexpr std::size_t day_at = 8;
constexpr std::size_t part_size = 2;
constexpr int base = 10;
constexpr std::size_t month_count = 12;

constexpr std::array<std::string_view, month_count> month_names = {
	"January", "February", "March",     "April",   "May",      "June",
	"July",    "August",   "September", "October", "November", "December",
};
constexpr std::array<std::string_view, month_count> short_month_names = {
	"jan", "feb", "mar", "apr", "may", "jun", "jul", "aug", "sep", "oct", "nov", "dec",
};

struct key_set {
	std::string_view kind;
	std::span<std::string_view const> keys;
};

constexpr auto post_key_list = std::to_array<std::string_view>({"title", "series", "description"});
constexpr auto page_key_list = std::to_array<std::string_view>({"title", "description"});
constexpr key_set post_keys{.kind = "post", .keys = post_key_list};
constexpr key_set page_keys{.kind = "page", .keys = page_key_list};

auto is_digit(char chr) -> bool
{
	return chr >= '0' && chr <= '9';
}

auto is_word_char(char chr) -> bool
{
	return is_digit(chr) || (chr >= 'a' && chr <= 'z');
}

auto is_slug_char(char chr) -> bool
{
	return is_word_char(chr) || chr == '-';
}

auto is_slug(std::string_view text) -> bool
{
	return !text.empty() && is_word_char(text.front()) && is_word_char(text.back()) &&
	       std::ranges::all_of(text, is_slug_char) && !text.contains("--"sv);
}

auto is_year(std::string_view slug) -> bool
{
	return slug.size() == year_size && std::ranges::all_of(slug, is_digit);
}

auto number(std::string_view text) -> int
{
	int value = 0;
	for (char const chr : text) {
		value = (value * base) + (chr - '0');
	}
	return value;
}

auto bad_name(errc code, std::string_view reason) -> error
{
	return {code, jimd::position{}, std::string{reason}};
}

constexpr auto extension_rule = "the name must end in .jimd"sv;
constexpr auto slug_rule =
	"the slug must be lower-case letters and digits, joined by single hyphens"sv;

void require_date(std::chrono::year_month_day date)
{
	if (!date.ok()) {
		throw std::out_of_range{"not a date"};
	}
}

auto strip_extension(std::string_view file_name) -> std::optional<std::string_view>
{
	if (!file_name.ends_with(extension)) {
		return std::nullopt;
	}
	file_name.remove_suffix(extension.size());
	return file_name;
}

auto fits_pattern(char pattern, char chr) -> bool
{
	return pattern == 'd' ? is_digit(chr) : chr == pattern;
}

auto has_date_prefix(std::string_view stem) -> bool
{
	return stem.size() >= date_pattern.size() &&
	       std::ranges::equal(date_pattern, stem.substr(0, date_pattern.size()), fits_pattern);
}

auto generated_for(std::string_view slug) -> std::optional<std::string>
{
	if (slug == "archive"sv) {
		return "/archive/ (the archive)"s;
	}
	if (is_year(slug)) {
		return std::format("/{}/ (posts are written under it)", slug);
	}
	return std::nullopt;
}

auto value_of(jimd::ast::document const& doc, std::string_view key) -> std::optional<std::string>
{
	auto const found = doc.metadata.find(key);
	if (found == doc.metadata.end() || found->second.empty()) {
		return std::nullopt;
	}
	return std::string{found->second};
}

auto check_keys(jimd::ast::document const& doc, key_set const& known) -> std::expected<void, error>
{
	std::optional<std::string_view> first;
	jimd::position first_at;
	for (auto const& entry : doc.metadata) {
		auto const key = entry.first;
		if (std::ranges::find(known.keys, key) != known.keys.end()) {
			continue;
		}
		auto const where = jimd::position_of(doc.source, key);
		if (!first ||
		    std::tie(where.line, where.column) < std::tie(first_at.line, first_at.column)) {
			first = key;
			first_at = where;
		}
	}
	if (!first) {
		return {};
	}
	auto const names = known.keys | std::views::join_with(", "sv) | std::ranges::to<std::string>();
	auto detail = std::format("{} (a {} has {})", *first, known.kind, names);
	return std::unexpected(error{errc::unknown_metadata, first_at, std::move(detail)});
}

struct metadata {
	std::string title;
	std::optional<std::string> series;
	std::optional<std::string> description;
};

auto read_metadata(jimd::ast::document const& doc, key_set const& known)
	-> std::expected<metadata, error>
{
	TRY(check_keys(doc, known));
	auto title = value_of(doc, "title");
	if (!title) {
		return std::unexpected(error{errc::missing_title});
	}
	return metadata{
		.title = std::move(*title),
		.series = value_of(doc, "series"),
		.description = value_of(doc, "description"),
	};
}
}

auto check_extension(errc code, std::string_view file_name) -> std::expected<void, error>
{
	if (!strip_extension(file_name)) {
		return std::unexpected(bad_name(code, extension_rule));
	}
	return {};
}

auto parse_post_name(std::string_view file_name) -> std::expected<post_name, error>
{
	constexpr auto code = errc::bad_post_name;
	auto const stem = strip_extension(file_name);
	if (!stem) {
		return std::unexpected(bad_name(code, extension_rule));
	}
	if (!has_date_prefix(*stem)) {
		return std::unexpected(bad_name(code, "the name must look like YYYY-MM-DD-slug.jimd"sv));
	}
	std::chrono::year_month_day const date{
		std::chrono::year{number(stem->substr(0, year_size))},
		std::chrono::month{static_cast<unsigned>(number(stem->substr(month_at, part_size)))},
		std::chrono::day{static_cast<unsigned>(number(stem->substr(day_at, part_size)))},
	};
	if (!date.ok()) {
		return std::unexpected(bad_name(
			code, std::format("{} is not a date", stem->substr(0, date_pattern.size() - 1))));
	}
	auto slug = stem->substr(date_pattern.size());
	if (!is_slug(slug)) {
		return std::unexpected(bad_name(code, slug_rule));
	}
	return post_name{.date = date, .slug = std::string{slug}};
}

auto parse_page_name(std::string_view file_name) -> std::expected<std::string, error>
{
	constexpr auto code = errc::bad_page_name;
	auto const stem = strip_extension(file_name);
	if (!stem) {
		return std::unexpected(bad_name(code, extension_rule));
	}
	if (!is_slug(*stem)) {
		return std::unexpected(bad_name(code, slug_rule));
	}
	return std::string{*stem};
}

auto read_post(std::string_view file_name, jimd::ast::document const& doc)
	-> std::expected<post, error>
{
	auto name = TRY(parse_post_name(file_name));
	auto meta = TRY(read_metadata(doc, post_keys));
	return post{
		.date = name.date,
		.slug = std::move(name.slug),
		.title = std::move(meta.title),
		.series = std::move(meta.series),
		.description = std::move(meta.description),
	};
}

auto read_page(std::string_view file_name, jimd::ast::document const& doc)
	-> std::expected<page, error>
{
	auto slug = TRY(parse_page_name(file_name));
	auto meta = TRY(read_metadata(doc, page_keys));
	if (auto const path = generated_for(slug)) {
		return std::unexpected(error{errc::page_collision, jimd::position{}, *path});
	}
	return page{
		.slug = std::move(slug),
		.title = std::move(meta.title),
		.description = std::move(meta.description),
	};
}

auto url(post const& item) -> std::string
{
	return std::format("/{:04}/{:02}/{:02}/{}/", static_cast<int>(item.date.year()),
	                   static_cast<unsigned>(item.date.month()),
	                   static_cast<unsigned>(item.date.day()), item.slug);
}

auto url(page const& item) -> std::string
{
	return std::format("/{}/", item.slug);
}

auto long_date(std::chrono::year_month_day date) -> std::string
{
	require_date(date);
	return std::format("{} {} {}", static_cast<unsigned>(date.day()),
	                   month_names.at(static_cast<unsigned>(date.month()) - 1),
	                   static_cast<int>(date.year()));
}

auto short_date(std::chrono::year_month_day date) -> std::string
{
	require_date(date);
	return std::format("{} {}", static_cast<unsigned>(date.day()),
	                   short_month_names.at(static_cast<unsigned>(date.month()) - 1));
}
}
