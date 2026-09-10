#pragma once

#include <cstddef>
#include <string_view>

namespace jimd
{
/**
 * struct position - A place in a source text.
 */
struct position {
	/** @line: 1-based line, or 0 if unknown. */
	std::size_t line{};
	/** @column: 1-based byte column, or 0 if unknown. */
	std::size_t column{};

	auto operator==(position const&) const -> bool = default;
};

/**
 * position_of() - Find where a view into a source text starts.
 * @source: The whole source text.
 * @part: A view into @source.
 *
 * Return: The position of the first byte of @part, or {0, 0} if @part does not lie
 * within @source.
 */
[[nodiscard]] auto position_of(std::string_view source, std::string_view part) noexcept -> position;
}
