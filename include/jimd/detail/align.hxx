#pragma once

#include <string_view>

namespace jimd::detail
{
/**
 * align_begin - Opens the ``align*`` environment around aligned maths rows.
 */
inline constexpr std::string_view align_begin = "\\begin{align*}\n";

/**
 * align_join - Separates two aligned maths rows.
 */
inline constexpr std::string_view align_join = "\n\\\\\n";

/**
 * align_end - Closes the ``align*`` environment.
 */
inline constexpr std::string_view align_end = "\n\\end{align*}";
}
