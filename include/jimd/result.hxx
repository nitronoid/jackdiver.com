#pragma once

#include <cstddef>
#include <cstdint>
#include <expected>
#include <string_view>
#include <utility>

namespace jimd
{
/**
 * enum errc - Why parsing or rendering a jimd document failed.
 */
enum class errc : std::uint8_t {
	/** @syntax: Input that matches no construct, such as a stray ``<`` line. */
	syntax,
	/** @unclosed_quote: A ``>`` quote with no closing ``<`` line. */
	unclosed_quote,
	/** @unclosed_code_block: A code fence with no closing fence. */
	unclosed_code_block,
	/** @unclosed_emphasis: An ``_`` or ``*`` not closed on its line. */
	unclosed_emphasis,
	/** @empty_emphasis: Emphasis with nothing inside, such as ``**``. */
	empty_emphasis,
	/** @unclosed_resource: A resource key or target not closed on its line. */
	unclosed_resource,
	/** @invalid_escape: A backslash before a character that cannot be escaped. */
	invalid_escape,
	/** @duplicate_definition: A key defined twice. */
	duplicate_definition,
	/** @undefined_reference: A reference to a key that is never defined. */
	undefined_reference,
	/** @empty_key: A blank key on a definition or bare reference. */
	empty_key,
	/** @empty_link: An inline link with blank text. */
	empty_link,
	/** @nested_footnote: A note inside another note. */
	nested_footnote,
	/** @duplicate_metadata: A metadata key given twice. */
	duplicate_metadata,
	/** @unclosed_math: Inline ``$`` maths not closed on its line. */
	unclosed_math,
	/** @unclosed_math_block: A ``$$`` or ``$$$`` block with no closing fence. */
	unclosed_math_block,
	/** @empty_math: Maths that holds only whitespace. */
	empty_math,
	/** @math_row_break: An aligned maths row that ends in ``\\``. */
	math_row_break,
	/**
	 * @math_environment_spans_rows: An aligned maths row with an unmatched ``\begin`` or ``\end``.
	 */
	math_environment_spans_rows,
	/** @invalid_width: An image width outside 1% to 100%. */
	invalid_width,
	/** @invalid_target: An SVG figure target with a character unsafe in a CSS ``url()``. */
	invalid_target,
	/** @unrendered_math: Maths missing from the table given to render(). */
	unrendered_math,
	/** @unknown_image_size: An SVG figure with no known aspect ratio. */
	unknown_image_size,
};

/**
 * struct error - A jimd failure and where it happened.
 */
struct error {
	/** @code: What went wrong. */
	errc code{};
	/** @line: 1-based line in the source, or 0 if there is no position. */
	std::size_t line{};
	/** @column: 1-based byte column in the source, or 0 if there is no position. */
	std::size_t column{};

	auto operator==(error const&) const -> bool = default;
};

/**
 * typedef result - A value of type @T, or the error that prevented it.
 */
template <typename T = void> using result = std::expected<T, error>;

/**
 * describe() - Describe an error code.
 * @code: The code to describe.
 *
 * Return: A short lower-case description, such as "unclosed quote".
 */
auto describe(errc code) noexcept -> std::string_view;
}

#ifdef TRY
#error "TRY already defined"
#endif
/**
 * TRY() - Unwrap a result, or return its error from the enclosing function.
 * @expr: An expression that yields a result.
 *
 * The enclosing function must return a result or another std::expected.
 *
 * Return: The value held by @expr, moved out.
 */
#define TRY(expr) /* NOLINT(cppcoreguidelines-macro-usage) */       \
	__extension__({                                                 \
		auto&& try_result_ = (expr);                                \
		if (!try_result_) {                                         \
			return std::unexpected(std::move(try_result_).error()); \
		}                                                           \
		*std::move(try_result_);                                    \
	})
