#pragma once

#include <expected>
#include <string>
#include <string_view>

#include <js/engine.hxx>

namespace highlight
{
/**
 * struct error - A failure to start the engine or to highlight some code.
 */
struct error {
	/** @message: What failed, such as "unknown language" or highlight.js's own error. */
	std::string message;

	auto operator==(error const&) const -> bool = default;
};

/**
 * class hljs - highlight.js running in a js::engine, turning code into HTML.
 *
 * Move-only. A moved-from engine fails every render.
 */
class hljs {
public:
	/**
	 * create() - Start a js::engine on highlight.js and its shim, compiled in at build time.
	 *
	 * Return: The engine, or an error if the runtime or highlight.js failed to start.
	 */
	static auto create() -> std::expected<hljs, error>;

	hljs(hljs const&) = delete;
	auto operator=(hljs const&) -> hljs& = delete;
	hljs(hljs&&) noexcept = default;
	auto operator=(hljs&&) noexcept -> hljs& = default;
	~hljs() = default;

	/**
	 * render() - Highlight code.
	 * @language: A language name or alias that highlight.js knows, in any case, such as "cpp",
	 * "c++" or "text". It is never detected from @code.
	 * @code: The source text.
	 *
	 * Return: @code as HTML, with every `<`, `>` and `&` escaped and <span class="hljs-...">
	 * around its tokens, or an error if the language is unknown or highlight.js failed.
	 */
	auto render(std::string_view language, std::string_view code)
		-> std::expected<std::string, error>;

private:
	explicit hljs(js::engine engine) noexcept;

	js::engine engine_;
};
}
