#pragma once

#include <expected>
#include <span>
#include <string_view>

#include <blog/error.hxx>

namespace blog
{
/**
 * struct options - Command-line options of the blog tool.
 */
struct options {
	/** @content: Directory given by --content. */
	std::string_view content;

	/** @css: Stylesheet given by --css. */
	std::string_view css;

	/** @output: Output directory given by -o. */
	std::string_view output;

	auto operator==(options const&) const -> bool = default;
};

/**
 * parse_args() - Parse the blog tool's arguments.
 * @args: Arguments, without the program name.
 *
 * Each of --content, --css and -o must be given exactly once, with a non-empty value.
 *
 * Return: The options, or an errc::usage error. The views point into @args.
 */
auto parse_args(std::span<char const* const> args) -> std::expected<options, error>;
}
