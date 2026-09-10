#include <js/engine.hxx>

#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>

#include <quickjs.h>

namespace js
{
namespace
{
constexpr std::size_t memory_limit = std::size_t{64} * 1024 * 1024;
constexpr std::size_t stack_limit = std::size_t{4} * 1024 * 1024;

// NOLINTNEXTLINE(bugprone-signed-bitwise)
constexpr int read_bytecode = JS_READ_OBJ_BYTECODE;
// The largest integer a double holds exactly: beyond it a position is not an index.
constexpr double max_position = 9007199254740992.0;

class value {
public:
	value(JSContext* ctx, JSValue val) noexcept
		: ctx_{ctx}
		, val_{val}
	{
	}
	value(value const&) = delete;
	auto operator=(value const&) -> value& = delete;
	value(value&&) = delete;
	auto operator=(value&&) -> value& = delete;
	~value()
	{
		JS_FreeValue(ctx_, val_);
	}

	[[nodiscard]] auto get() const noexcept -> JSValue
	{
		return val_;
	}

private:
	JSContext* ctx_;
	JSValue val_;
};

auto to_string(JSContext* ctx, JSValueConst val) -> std::optional<std::string>
{
	std::size_t size = 0;
	char const* const text = JS_ToCStringLen(ctx, &size, val);
	if (text == nullptr) {
		return std::nullopt;
	}
	std::string result{text, size};
	JS_FreeCString(ctx, text);
	return result;
}

auto string_of(JSContext* ctx, JSValueConst val) -> std::string
{
	if (auto text = to_string(ctx, val)) {
		return *std::move(text);
	}
	value const pending{ctx, JS_GetException(ctx)};
	return {};
}

// A property of an object, or undefined if reading it throws.
auto property(JSContext* ctx, JSValueConst object, char const* name) -> value
{
	JSValue result = JS_GetPropertyStr(ctx, object, name);
	if (JS_IsException(result)) {
		value const pending{ctx, JS_GetException(ctx)};
		result = JS_UNDEFINED;
	}
	return {ctx, result};
}

struct sequence {
	std::size_t bytes;
	std::size_t units;
};

/*
 * Mirrors QuickJS's utf8_decode(), which accepts surrogates, so that the UTF-16 indices
 * a script reports map back to the bytes QuickJS read them from.
 */
auto sequence_at(std::string_view text, std::size_t index) -> sequence
{
	struct form {
		unsigned char first_lead;
		unsigned char last_lead;
		unsigned char first_second;
		unsigned char last_second;
		std::size_t following;
	};
	constexpr std::array<form, 6> forms{{
		{.first_lead = 0xC2,
		 .last_lead = 0xDF,
		 .first_second = 0x80,
		 .last_second = 0xBF,
		 .following = 1},
		{.first_lead = 0xE0,
		 .last_lead = 0xE0,
		 .first_second = 0xA0,
		 .last_second = 0xBF,
		 .following = 2},
		{.first_lead = 0xE1,
		 .last_lead = 0xEF,
		 .first_second = 0x80,
		 .last_second = 0xBF,
		 .following = 2},
		{.first_lead = 0xF0,
		 .last_lead = 0xF0,
		 .first_second = 0x90,
		 .last_second = 0xBF,
		 .following = 3},
		{.first_lead = 0xF1,
		 .last_lead = 0xF3,
		 .first_second = 0x80,
		 .last_second = 0xBF,
		 .following = 3},
		{.first_lead = 0xF4,
		 .last_lead = 0xF4,
		 .first_second = 0x80,
		 .last_second = 0x8F,
		 .following = 3},
	}};
	constexpr unsigned char first_continuation = 0x80;
	constexpr unsigned char last_continuation = 0xBF;
	constexpr sequence invalid{.bytes = 1, .units = 1};

	auto const byte = [&](std::size_t position) -> unsigned char {
		return position < text.size() ? static_cast<unsigned char>(text.at(position)) : '\0';
	};
	auto const lead = byte(index);
	if (lead < first_continuation) {
		return {.bytes = 1, .units = 1};
	}
	for (auto const& candidate : forms) {
		if (lead < candidate.first_lead || lead > candidate.last_lead) {
			continue;
		}
		if (byte(index + 1) < candidate.first_second || byte(index + 1) > candidate.last_second) {
			return invalid;
		}
		for (std::size_t i = 2; i <= candidate.following; ++i) {
			if (byte(index + i) < first_continuation || byte(index + i) > last_continuation) {
				return invalid;
			}
		}
		return {.bytes = candidate.following + 1, .units = candidate.following == 3 ? 2U : 1U};
	}
	return invalid;
}

auto failure(std::string message) -> std::unexpected<error>
{
	return std::unexpected(error{.message = std::move(message), .offset = std::nullopt});
}

// The index a script's position names, if it is a number that is a non-negative integer.
auto position_unit(JSContext* ctx, JSValueConst position) -> std::optional<std::size_t>
{
	double number = 0;
	if (!JS_IsNumber(position)) {
		return std::nullopt;
	}
	if (JS_ToFloat64(ctx, &number, position) != 0) {
		value const failed{ctx, JS_GetException(ctx)};
		return std::nullopt;
	}
	if (!std::isfinite(number) || number < 0 || number > max_position ||
	    std::trunc(number) != number) {
		return std::nullopt;
	}
	return static_cast<std::size_t>(number);
}

// Takes the pending exception. The offset is only set if there is an input to map it into.
auto pending_error(JSContext* ctx, std::optional<std::string_view> input) -> error
{
	value const exception{ctx, JS_GetException(ctx)};
	// QuickJS throws null when it cannot even allocate the out-of-memory error.
	if (JS_IsNull(exception.get())) {
		return {.message = "out of memory", .offset = std::nullopt};
	}
	if (!JS_IsObject(exception.get())) {
		return {.message = string_of(ctx, exception.get()), .offset = std::nullopt};
	}

	// A thrown object with no message is reported as what String() makes of it.
	auto const message = property(ctx, exception.get(), "message");
	error result{.message = string_of(ctx, JS_IsUndefined(message.get()) ? exception.get() :
	                                                                       message.get()),
	             .offset = std::nullopt};
	if (input) {
		auto const unit = position_unit(ctx, property(ctx, exception.get(), "position").get());
		if (unit) {
			result.offset = detail::utf8_offset(*input, *unit);
		}
	}
	return result;
}
}

namespace detail
{
auto utf8_offset(std::string_view text, std::size_t unit) -> std::optional<std::size_t>
{
	std::size_t units = 0;
	for (std::size_t i = 0; i < text.size();) {
		auto const step = sequence_at(text, i);
		if (unit < units + step.units) {
			return i;
		}
		i += step.bytes;
		units += step.units;
	}
	if (unit == units) {
		return text.size();
	}
	return std::nullopt;
}
}

struct engine::impl {
	impl() = default;
	impl(impl const&) = delete;
	auto operator=(impl const&) -> impl& = delete;
	impl(impl&&) = delete;
	auto operator=(impl&&) -> impl& = delete;
	~impl()
	{
		if (ctx != nullptr) {
			JS_FreeValue(ctx, render);
			JS_FreeContext(ctx);
		}
		if (rt != nullptr) {
			JS_FreeRuntime(rt);
		}
	}

	JSRuntime* rt = nullptr;
	JSContext* ctx = nullptr;
	JSValue render = JS_UNDEFINED;
};

engine::engine(std::unique_ptr<impl> state) noexcept
	: state_{std::move(state)}
{
}

engine::engine(engine&&) noexcept = default;
auto engine::operator=(engine&&) noexcept -> engine& = default;
engine::~engine() = default;

auto engine::create(std::span<std::span<std::uint8_t const> const> scripts)
	-> std::expected<engine, error>
{
	auto state = std::make_unique<impl>();
	state->rt = JS_NewRuntime();
	if (state->rt == nullptr) {
		return failure("could not create a JavaScript runtime");
	}
	JS_SetMemoryLimit(state->rt, memory_limit);
	JS_SetMaxStackSize(state->rt, stack_limit);
	state->ctx = JS_NewContext(state->rt);
	if (state->ctx == nullptr) {
		return failure("could not create a JavaScript context");
	}

	JSContext* const ctx = state->ctx;
	for (auto const script : scripts) {
		// JS_EvalFunction takes the function, so there is nothing to free once it has it.
		JSValue const function = JS_ReadObject(ctx, script.data(), script.size(), read_bytecode);
		if (JS_IsException(function)) {
			return std::unexpected(pending_error(ctx, std::nullopt));
		}
		value const done{ctx, JS_EvalFunction(ctx, function)};
		if (JS_IsException(done.get())) {
			return std::unexpected(pending_error(ctx, std::nullopt));
		}
	}

	value const global{ctx, JS_GetGlobalObject(ctx)};
	state->render = JS_GetPropertyStr(ctx, global.get(), "render");
	if (!JS_IsFunction(ctx, state->render)) {
		return failure("render is not a global function");
	}
	return engine{std::move(state)};
}

auto engine::render(std::string_view input, std::string_view arg)
	-> std::expected<std::string, error>
{
	if (state_ == nullptr) {
		return failure("the engine was moved from");
	}
	JSContext* const ctx = state_->ctx;
	/*
	 * QuickJS measures the stack limit from the frame that created the runtime, not
	 * from this one.
	 */
	JS_UpdateStackTop(state_->rt);

	value const first{ctx, JS_NewStringLen(ctx, input.data(), input.size())};
	value const second{ctx, JS_NewStringLen(ctx, arg.data(), arg.size())};
	if (JS_IsException(first.get()) || JS_IsException(second.get())) {
		return std::unexpected(pending_error(ctx, input));
	}

	std::array<JSValue, 2> arguments{first.get(), second.get()};
	value const result{ctx, JS_Call(ctx, state_->render, JS_UNDEFINED,
	                                static_cast<int>(arguments.size()), arguments.data())};
	if (JS_IsException(result.get())) {
		return std::unexpected(pending_error(ctx, input));
	}
	if (!JS_IsString(result.get())) {
		return failure("render did not return a string");
	}
	if (auto text = to_string(ctx, result.get())) {
		return *std::move(text);
	}
	return std::unexpected(pending_error(ctx, input));
}
}
