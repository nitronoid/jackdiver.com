#pragma once

#include <functional>
#include <string_view>

#include <jimd/position.hxx>
#include <jimd/result.hxx>

namespace jimd::detail
{
/**
 * contains() - Check whether one view lies within another.
 * @source: The enclosing view.
 * @part: The view to look for.
 *
 * Compares addresses, not contents. An empty @part at the end of @source counts.
 *
 * Return: True if @part lies within a non-empty @source.
 */
[[nodiscard]] inline auto contains(std::string_view source, std::string_view part) noexcept -> bool
{
	if (source.empty()) {
		return false;
	}
	auto const* const src_begin = source.data();
	// NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic)
	auto const* const src_end = src_begin + source.size();
	auto const* const part_begin = part.data();
	// NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic)
	auto const* const part_end = part_begin + part.size();
	return !std::less<char const*>{}(part_begin, src_begin) &&
	       !std::greater<char const*>{}(part_end, src_end);
}

/**
 * make_error() - Make an error located at a view into the source.
 * @code: What went wrong.
 * @source: The whole source text.
 * @part: The view into @source to point at.
 *
 * Return: An error at the start of @part, or with no position if @part is not in @source.
 */
[[nodiscard]] inline auto make_error(errc code, std::string_view source,
                                     std::string_view part) noexcept -> error
{
	auto const pos = position_of(source, part);
	return error{.code = code, .line = pos.line, .column = pos.column};
}
}
