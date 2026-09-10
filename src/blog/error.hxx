#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <utility>
#include <variant>

#include <jimd/position.hxx>
#include <jimd/result.hxx>

namespace blog
{
/**
 * enum errc - Failures of the blog tool, beyond jimd's own.
 */
enum class errc : std::uint8_t {
	/** @file_not_found: An input file does not exist. */
	file_not_found,
	/** @read_failed: An input file could not be read. */
	read_failed,
	/** @write_failed: An output file could not be written. */
	write_failed,
	/** @image_size: An SVG figure has no usable viewBox. */
	image_size,
	/** @invalid_math: Temml rejected a formula. */
	invalid_math,
	/** @invalid_code: highlight.js rejected a code block, or its language is unknown. */
	invalid_code,
	/** @image_not_local: A figure target is not a safe site-root path. */
	image_not_local,
	/** @bad_post_name: A post's file name is not a dated slug. */
	bad_post_name,
	/** @bad_page_name: A page's file name is not a valid slug. */
	bad_page_name,
	/** @missing_title: A post or page has no title metadata. */
	missing_title,
	/** @unknown_metadata: A post or page has a metadata key it may not. */
	unknown_metadata,
	/** @page_collision: A page's path clashes with a generated one. */
	page_collision,
	/** @usage: Bad blog command line; prints the usage. */
	usage,
	/** @output_not_empty: The blog's output directory already has files. */
	output_not_empty,
	/** @static_collision: A static file clashes with a file the blog writes. */
	static_collision,
};

/**
 * struct error - A jimd or blog failure, with an optional position and detail.
 */
struct error {
	/**
	 * error() - Wrap a jimd error, keeping its position.
	 * @err: The jimd error.
	 */
	// NOLINTNEXTLINE(cppcoreguidelines-explicit-constructor,google-explicit-constructor)
	constexpr error(jimd::error err) noexcept
		: code{err.code}
		, line{err.line}
		, column{err.column}
	{
	}

	/**
	 * error() - Make an error with no position or detail.
	 * @value: The code.
	 */
	// NOLINTNEXTLINE(cppcoreguidelines-explicit-constructor,google-explicit-constructor)
	constexpr error(errc value) noexcept
		: code{value}
	{
	}

	/**
	 * error() - Make a positioned error.
	 * @value: The code.
	 * @where: Its position; a zero line means none.
	 * @message: Detail appended after the description.
	 */
	error(std::variant<jimd::errc, errc> value, jimd::position where, std::string message = {})
		: code{value}
		, line{where.line}
		, column{where.column}
		, detail{std::move(message)}
	{
	}

	/** @code: What went wrong. */
	std::variant<jimd::errc, errc> code;
	/** @line: 1-based line, or 0 for no position. */
	std::size_t line{};
	/** @column: 1-based column; unused when @line is 0. */
	std::size_t column{};
	/** @detail: Extra context, such as a path or Temml's message; may be empty. */
	std::string detail;

	auto operator==(error const&) const -> bool = default;
};

/**
 * describe() - Describe a blog error code.
 * @code: The code.
 *
 * Return: A short lower-case phrase, or the usage line for errc::usage.
 */
auto describe(errc code) noexcept -> std::string_view;

/**
 * format_error() - Render an error as a one-line diagnostic.
 * @err: The error.
 * @path: File to prefix the message with; may be empty.
 *
 * The form is "path:line:column: description: detail", dropping the parts
 * that are absent. A usage error is never prefixed.
 *
 * Return: The message, without a trailing newline.
 */
auto format_error(error const& err, std::string_view path = {}) -> std::string;

/**
 * handle_error() - Print format_error() and a newline to stderr.
 * @err: The error.
 * @path: As for format_error().
 */
auto handle_error(error const& err, std::string_view path = {}) -> void;
}
