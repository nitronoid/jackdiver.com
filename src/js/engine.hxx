#pragma once

#include <cstddef>
#include <cstdint>
#include <expected>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>

namespace js
{
/**
 * struct error - A failure to start the engine or to run a script's render().
 */
struct error {
	/** @message: The exception's message, or what loading the scripts failed with. */
	std::string message;
	/** @offset: Byte offset into the input, if the script gave a position that maps into it. */
	std::optional<std::size_t> offset;

	auto operator==(error const&) const -> bool = default;
};

namespace detail
{
/**
 * utf8_offset() - Convert a UTF-16 code unit index into a byte offset.
 * @text: UTF-8 text; each invalid byte counts as one code unit.
 * @unit: UTF-16 code unit index into @text, as JavaScript counts them.
 *
 * Return: The byte offset of the sequence holding @unit, @text.size() if @unit
 * is one past the end, or std::nullopt if it is further.
 */
auto utf8_offset(std::string_view text, std::size_t unit) -> std::optional<std::size_t>;
}

/**
 * class engine - Scripts compiled by js_bytecode, running in their own QuickJS runtime.
 *
 * Move-only. A moved-from engine fails every render. The scripts must define a global function
 * render(input, arg) that takes two strings and returns a string. To fail, it throws an object
 * {message, position?}: @message is a string, and @position, if a number, is the UTF-16 index into
 * @input where the failure is. Anything else thrown is an error too: QuickJS's own (stack
 * overflow, out of memory), or a value with no message, which is reported as its string conversion.
 */
class engine {
public:
	/**
	 * create() - Start a runtime and run the scripts in it.
	 * @scripts: QuickJS bytecode, as js_bytecode's accessor returns it. Run in order, as plain
	 * scripts in one context, so later ones see the globals of earlier ones.
	 *
	 * Return: The engine, or an error if the runtime failed to start, a script was not valid
	 * bytecode or threw while running, or none of them defined a global function render().
	 */
	static auto create(std::span<std::span<std::uint8_t const> const> scripts)
		-> std::expected<engine, error>;

	engine(engine const&) = delete;
	auto operator=(engine const&) -> engine& = delete;
	engine(engine&&) noexcept;
	auto operator=(engine&&) noexcept -> engine&;
	~engine();

	/**
	 * render() - Call the script's render().
	 * @input: The first argument, as a string.
	 * @arg: The second argument, as a string.
	 *
	 * Return: What render() returned, or an error if it threw or returned something that is not a
	 * string.
	 */
	auto render(std::string_view input, std::string_view arg) -> std::expected<std::string, error>;

private:
	struct impl;

	explicit engine(std::unique_ptr<impl> state) noexcept;

	std::unique_ptr<impl> state_;
};
}
