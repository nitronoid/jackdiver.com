#include <blog/image.hxx>

#include <algorithm>
#include <array>
#include <cstddef>
#include <expected>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

#include <blog/error.hxx>
#include <blog/file.hxx>
#include <jimd/position.hxx>

using namespace std::literals;

namespace blog
{
namespace
{
constexpr auto blanks = " \t\n\r\f"sv;
constexpr auto tag_end = "/>"sv;

auto is_blank(char chr) -> bool
{
	return blanks.contains(chr);
}

auto skip_blanks(std::string_view& text) -> void
{
	auto const end = text.find_first_not_of(blanks);
	text.remove_prefix(end == std::string_view::npos ? text.size() : end);
}

auto bad_size(std::string_view reason) -> error
{
	return {errc::image_size, jimd::position{}, std::string{reason}};
}

auto after_svg_name(std::string_view svg) -> std::optional<std::string_view>
{
	std::size_t pos = 0;
	while ((pos = svg.find('<', pos)) != std::string_view::npos) {
		auto const rest = svg.substr(pos);
		if (rest.starts_with("<!--")) {
			auto const end = rest.find("-->", 4);
			if (end == std::string_view::npos) {
				return std::nullopt;
			}
			pos += end + 3;
			continue;
		}
		auto const after = rest.substr(std::min<std::size_t>(4, rest.size()));
		if (rest.starts_with("<svg") &&
		    (after.empty() || is_blank(after.front()) || tag_end.contains(after.front()))) {
			return after;
		}
		++pos;
	}
	return std::nullopt;
}

constexpr auto unclosed_tag = "the <svg> tag is not closed"sv;
constexpr auto bad_attribute = "an attribute of the <svg> tag is not name=\"value\""sv;

auto find_view_box(std::string_view attrs) -> std::expected<std::string_view, std::string_view>
{
	while (true) {
		skip_blanks(attrs);
		if (attrs.empty()) {
			return std::unexpected(unclosed_tag);
		}
		if (attrs.starts_with('>') || attrs.starts_with("/>")) {
			return std::unexpected("no viewBox in the <svg> tag"sv);
		}

		auto const name_size = std::min(attrs.find_first_of(blanks), attrs.find_first_of("=/>"sv));
		if (name_size == 0) {
			return std::unexpected(bad_attribute);
		}
		auto const name = attrs.substr(0, name_size);
		attrs.remove_prefix(name_size == std::string_view::npos ? attrs.size() : name_size);

		skip_blanks(attrs);
		if (!attrs.starts_with('=')) {
			return std::unexpected(bad_attribute);
		}
		attrs.remove_prefix(1);
		skip_blanks(attrs);
		if (!attrs.starts_with('"') && !attrs.starts_with('\'')) {
			return std::unexpected(bad_attribute);
		}
		auto const close = attrs.find(attrs.front(), 1);
		if (close == std::string_view::npos) {
			return std::unexpected(unclosed_tag);
		}
		auto const value = attrs.substr(1, close - 1);
		attrs.remove_prefix(close + 1);

		if (name == "viewBox"sv) {
			return value;
		}
	}
}

struct number {
	std::string text;
	bool negative{};
	bool zero{};
};

auto take_digits(std::string_view& text) -> std::string_view
{
	auto const end = text.find_first_not_of("0123456789"sv);
	auto const size = end == std::string_view::npos ? text.size() : end;
	auto const digits = text.substr(0, size);
	text.remove_prefix(size);
	return digits;
}

auto take_number(std::string_view& text) -> std::optional<number>
{
	auto rest = text;
	number result;
	if (rest.starts_with('-') || rest.starts_with('+')) {
		result.negative = rest.starts_with('-');
		rest.remove_prefix(1);
	}
	auto whole = take_digits(rest);
	std::string_view fraction;
	if (rest.starts_with('.')) {
		rest.remove_prefix(1);
		fraction = take_digits(rest);
	}
	if (whole.empty() && fraction.empty()) {
		return std::nullopt;
	}

	auto const first = whole.find_first_not_of('0');
	whole.remove_prefix(first == std::string_view::npos ? whole.size() : first);
	auto const last = fraction.find_last_not_of('0');
	fraction.remove_suffix(fraction.size() - (last == std::string_view::npos ? 0 : last + 1));

	result.zero = whole.empty() && fraction.empty();
	result.text = whole.empty() ? "0" : std::string{whole};
	if (!fraction.empty()) {
		result.text += '.';
		result.text += fraction;
	}
	text = rest;
	return result;
}

auto take_view_box(std::string_view text) -> std::optional<std::array<number, 4>>
{
	std::array<number, 4> numbers;
	skip_blanks(text);
	for (std::size_t index = 0; index < numbers.size(); ++index) {
		if (index != 0) {
			auto const before = text.size();
			skip_blanks(text);
			if (text.starts_with(',')) {
				text.remove_prefix(1);
			}
			skip_blanks(text);
			if (text.size() == before) {
				return std::nullopt;
			}
		}
		auto taken = take_number(text);
		if (!taken) {
			return std::nullopt;
		}
		numbers.at(index) = *std::move(taken);
	}
	skip_blanks(text);
	if (!text.empty()) {
		return std::nullopt;
	}
	return numbers;
}
}

auto local_path_problem(std::string_view target) -> std::optional<std::string_view>
{
	if (!target.starts_with('/')) {
		return "does not start with /";
	}
	/*
	 * A leading // is protocol-relative, naming another host. Browsers also read \ as /,
	 * which is why it is refused below.
	 */
	if (target.starts_with("//")) {
		return "starts with //";
	}
	auto rest = target.substr(1);
	while (true) {
		auto const end = rest.find('/');
		auto const segment = rest.substr(0, end);
		if (segment.empty()) {
			return "has an empty segment";
		}
		if (segment == "." || segment == "..") {
			return "has a . or .. segment";
		}
		if (end == std::string_view::npos) {
			break;
		}
		rest.remove_prefix(end + 1);
	}
	if (target.find_first_of("?#%") != std::string_view::npos) {
		return "has a ?, # or %";
	}
	auto const is_odd = [](char chr) -> bool {
		auto const byte = static_cast<unsigned char>(chr);
		return chr == '\\' || byte < ' ' || byte == '\x7f';
	};
	if (std::ranges::any_of(target, is_odd)) {
		return "has a \\ or a control character";
	}
	return std::nullopt;
}

auto view_box_ratio(std::string_view svg) -> std::expected<std::string, error>
{
	auto const attrs = after_svg_name(svg);
	if (!attrs) {
		return std::unexpected(bad_size("no <svg> tag"));
	}
	auto const value = find_view_box(*attrs);
	if (!value) {
		return std::unexpected(bad_size(value.error()));
	}
	auto const numbers = take_view_box(*value);
	if (!numbers) {
		return std::unexpected(bad_size("viewBox is not four numbers"));
	}
	auto const& width = numbers->at(2);
	auto const& height = numbers->at(3);
	if (width.negative || width.zero || height.negative || height.zero) {
		return std::unexpected(bad_size("viewBox width and height must be positive"));
	}
	return width.text + '/' + height.text;
}

auto svg_ratio(std::filesystem::path const& path) -> std::expected<std::string, error>
{
	auto const content = read_file(path);
	if (!content) {
		return std::unexpected(content.error());
	}
	return view_box_ratio(*content);
}
}
