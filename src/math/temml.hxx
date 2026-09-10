#pragma once

#include <cstddef>
#include <cstdint>
#include <expected>
#include <optional>
#include <string>
#include <string_view>

#include <js/engine.hxx>

namespace math
{
/**
 * enum mode - How Temml lays out a formula.
 */
enum class mode : std::uint8_t {
	/** @inline_math: Inline, in text style. */
	inline_math,
	/** @display: Display style, as a block of its own. */
	display,
};

/**
 * struct error - A failure to start the engine or to render a formula.
 */
struct error {
	/** @message: Temml's message, without its position suffix. */
	std::string message;
	/** @offset: Byte offset of the failure in the TeX, if Temml gave one. */
	std::optional<std::size_t> offset;

	auto operator==(error const&) const -> bool = default;
};

/**
 * class temml - Temml running in a js::engine, rendering TeX to MathML.
 *
 * Move-only. A moved-from engine fails every render.
 */
class temml {
public:
	/**
	 * create() - Start a js::engine on Temml and its shim, compiled in at build time.
	 *
	 * Return: The engine, or an error if the runtime or Temml failed to start.
	 */
	static auto create() -> std::expected<temml, error>;

	temml(temml const&) = delete;
	auto operator=(temml const&) -> temml& = delete;
	temml(temml&&) noexcept = default;
	auto operator=(temml&&) noexcept -> temml& = default;
	~temml() = default;

	/**
	 * render() - Render TeX as MathML.
	 * @tex: The TeX source, without delimiters.
	 * @style: Inline or display layout.
	 *
	 * Return: The MathML markup, or Temml's error.
	 */
	auto render(std::string_view tex, mode style) -> std::expected<std::string, error>;

private:
	explicit temml(js::engine engine) noexcept;

	js::engine engine_;
};
}
