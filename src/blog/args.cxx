#include <blog/args.hxx>

#include <expected>
#include <optional>
#include <span>
#include <string_view>

#include <blog/error.hxx>

namespace blog
{
namespace
{
using arg_span = std::span<char const* const>;

auto take_value(arg_span& rest, std::optional<std::string_view>& slot) -> std::expected<void, error>
{
	if (slot || rest.empty()) {
		return std::unexpected(errc::usage);
	}
	std::string_view const value{rest.front()};
	rest = rest.subspan(1);
	if (value.empty()) {
		return std::unexpected(errc::usage);
	}
	slot = value;
	return {};
}
}

auto parse_args(std::span<char const* const> args) -> std::expected<options, error>
{
	std::optional<std::string_view> content;
	std::optional<std::string_view> css;
	std::optional<std::string_view> output;

	while (!args.empty()) {
		std::string_view const arg{args.front()};
		args = args.subspan(1);
		std::expected<void, error> taken;
		if (arg == "--content") {
			taken = take_value(args, content);
		} else if (arg == "--css") {
			taken = take_value(args, css);
		} else if (arg == "-o") {
			taken = take_value(args, output);
		} else {
			return std::unexpected(errc::usage);
		}
		if (!taken) {
			return std::unexpected(taken.error());
		}
	}

	if (!content || !css || !output) {
		return std::unexpected(errc::usage);
	}
	return options{.content = *content, .css = *css, .output = *output};
}
}
