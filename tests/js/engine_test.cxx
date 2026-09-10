#include <array>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <optional>
#include <span>
#include <string>
#include <utility>
#include <vector>
#include <gtest/gtest.h>

#include <behaviours.h>
#include <echo.h>
#include <js/engine.hxx>
#include <no_render.h>
#include <throws_on_load.h>

namespace
{
auto make_engine(std::span<std::span<std::uint8_t const> const> scripts) -> js::engine
{
	auto engine = js::engine::create(scripts);
	EXPECT_TRUE(engine.has_value());
	return std::move(engine).value();
}

auto make_behaviours() -> js::engine
{
	return make_engine(behaviours());
}

auto create_error(std::span<std::span<std::uint8_t const> const> scripts) -> js::error
{
	auto engine = js::engine::create(scripts);
	EXPECT_FALSE(engine.has_value());
	if (engine.has_value()) {
		return {};
	}
	return engine.error();
}

// What render() makes of `arg`, for the "behaviours" scripts.
auto render_error(js::engine& engine, std::string const& input, std::string const& arg) -> js::error
{
	auto const result = engine.render(input, arg);
	EXPECT_FALSE(result.has_value()) << arg;
	if (result.has_value()) {
		return {};
	}
	return result.error();
}

auto throw_error(js::engine& engine, std::string const& input, std::string const& thrown)
	-> js::error
{
	return render_error(engine, input, "throw " + thrown);
}
}

TEST(engine, round_trips_a_string)
{
	auto engine = make_engine(echo());
	auto const result = engine.render("input", "arg");
	ASSERT_TRUE(result.has_value());
	EXPECT_EQ(*result, "<shim><arg>input");
}

TEST(engine, runs_the_scripts_in_order_in_one_context)
{
	// shim.js calls what library.js defines as it loads.
	auto const scripts = echo();
	auto const reversed = std::array{scripts.at(1), scripts.at(0)};
	auto const err = create_error(reversed);
	EXPECT_EQ(err.message, "decorate is not defined");
	EXPECT_EQ(err.offset, std::nullopt);
}

TEST(engine, renders_again_with_the_same_state)
{
	auto engine = make_engine(echo());
	EXPECT_EQ(engine.render("a", "b"), "<shim><b>a");
	EXPECT_EQ(engine.render("c", "d"), "<shim><d>c");
}

TEST(engine, round_trips_text_that_is_not_ascii)
{
	auto engine = make_behaviours();
	std::string const input = "\xC3\xA9\xE2\x82\xAC\xF0\x9F\x98\x80";
	EXPECT_EQ(engine.render(input, "echo"), input);
}

TEST(engine, round_trips_an_empty_string_and_a_nul)
{
	auto engine = make_behaviours();
	EXPECT_EQ(engine.render("", "echo"), "");
	std::string const input{"a\0b", 3};
	EXPECT_EQ(engine.render(input, "echo"), input);
}

TEST(engine, takes_a_returned_empty_string)
{
	auto engine = make_behaviours();
	EXPECT_EQ(engine.render("x", R"(return "")"), "");
}

TEST(engine, reports_a_message_without_a_position)
{
	auto engine = make_behaviours();
	auto const err = throw_error(engine, "abc", R"({"message": "bad input"})");
	EXPECT_EQ(err.message, "bad input");
	EXPECT_EQ(err.offset, std::nullopt);
}

TEST(engine, reports_a_position_as_a_byte_offset)
{
	auto engine = make_behaviours();
	auto const err = throw_error(engine, "abc", R"({"message": "bad input", "position": 1})");
	EXPECT_EQ(err.message, "bad input");
	EXPECT_EQ(err.offset, std::optional<std::size_t>{1});
}

TEST(engine, reports_a_position_in_text_that_is_not_ascii)
{
	// UTF-16 units: e-acute 0, euro 1, the emoji 2 and 3, x 4. Bytes: 0, 2, 5, 9.
	auto engine = make_behaviours();
	std::string const input = "\xC3\xA9\xE2\x82\xAC\xF0\x9F\x98\x80x";
	auto const offset_of = [&](std::string const& unit) -> std::optional<std::size_t> {
		return throw_error(engine, input, R"({"message": "m", "position": )" + unit + "}").offset;
	};
	EXPECT_EQ(offset_of("0"), std::optional<std::size_t>{0});
	EXPECT_EQ(offset_of("1"), std::optional<std::size_t>{2});
	EXPECT_EQ(offset_of("2"), std::optional<std::size_t>{5});
	EXPECT_EQ(offset_of("3"), std::optional<std::size_t>{5});
	EXPECT_EQ(offset_of("4"), std::optional<std::size_t>{9});
	EXPECT_EQ(offset_of("5"), std::optional<std::size_t>{10});
	EXPECT_EQ(offset_of("6"), std::nullopt);
}

TEST(engine, reports_a_position_of_an_error_object)
{
	auto engine = make_behaviours();
	auto const err = render_error(engine, "abc", "error");
	EXPECT_EQ(err.message, "boom");
	EXPECT_EQ(err.offset, std::optional<std::size_t>{2});
}

TEST(engine, gives_no_offset_for_a_position_that_is_not_an_index)
{
	auto engine = make_behaviours();
	for (auto const* position : {"-1", "1.5", R"("1")", "null", "true", "[1]", "1e300", "-0.5"}) {
		auto const err = throw_error(
			engine, "abc", std::string{R"({"message": "m", "position": )"} + position + "}");
		EXPECT_EQ(err.message, "m") << position;
		EXPECT_EQ(err.offset, std::nullopt) << position;
	}
	auto const nan = render_error(engine, "abc", "nan");
	EXPECT_EQ(nan.message, "not a number");
	EXPECT_EQ(nan.offset, std::nullopt);
}

TEST(engine, takes_a_whole_number_written_as_a_float)
{
	auto engine = make_behaviours();
	auto const err = throw_error(engine, "abc", R"({"message": "m", "position": 2.0})");
	EXPECT_EQ(err.offset, std::optional<std::size_t>{2});
}

TEST(engine, reports_an_object_with_no_message_as_its_string_conversion)
{
	auto engine = make_behaviours();
	auto const plain = throw_error(engine, "abc", "{}");
	EXPECT_EQ(plain.message, "[object Object]");
	EXPECT_EQ(plain.offset, std::nullopt);
	// The position is still taken.
	auto const positioned = throw_error(engine, "abc", R"({"position": 1})");
	EXPECT_EQ(positioned.message, "[object Object]");
	EXPECT_EQ(positioned.offset, std::optional<std::size_t>{1});
}

TEST(engine, survives_a_message_that_throws_when_read)
{
	auto engine = make_behaviours();
	auto const err = render_error(engine, "abc", "getter");
	EXPECT_EQ(err.message, "[object Object]");
	EXPECT_EQ(err.offset, std::optional<std::size_t>{1});
	EXPECT_TRUE(engine.render("abc", "echo").has_value());
}

TEST(engine, reports_a_thrown_value_that_is_not_an_object_as_a_string)
{
	auto engine = make_behaviours();
	auto const text = throw_error(engine, "abc", R"("oops")");
	EXPECT_EQ(text.message, "oops");
	EXPECT_EQ(text.offset, std::nullopt);
	EXPECT_EQ(throw_error(engine, "abc", "42").message, "42");
	EXPECT_EQ(throw_error(engine, "abc", "true").message, "true");
}

TEST(engine, reports_a_thrown_null_as_out_of_memory)
{
	auto engine = make_behaviours();
	auto const err = throw_error(engine, "abc", "null");
	EXPECT_EQ(err.message, "out of memory");
	EXPECT_EQ(err.offset, std::nullopt);
}

TEST(engine, reports_an_error_of_the_script_itself)
{
	auto engine = make_behaviours();
	auto const err = render_error(engine, "abc", "type error");
	EXPECT_NE(err.message.find("null"), std::string::npos) << err.message;
	EXPECT_EQ(err.offset, std::nullopt);
}

TEST(engine, reports_a_result_that_is_not_a_string_as_an_error)
{
	auto engine = make_behaviours();
	for (auto const* json : {"1", "null", "true", "{}", R"(["a"])", R"({"message": "m"})"}) {
		auto const err = render_error(engine, "abc", std::string{"return "} + json);
		EXPECT_EQ(err.message, "render did not return a string") << json;
		EXPECT_EQ(err.offset, std::nullopt) << json;
	}
	EXPECT_TRUE(engine.render("abc", "echo").has_value());
}

TEST(engine, reports_a_stack_overflow_and_carries_on)
{
	auto engine = make_behaviours();
	auto const err = render_error(engine, "abc", "recurse");
	EXPECT_NE(err.message.find("call stack"), std::string::npos) << err.message;
	EXPECT_EQ(err.offset, std::nullopt);
	EXPECT_EQ(engine.render("abc", "echo"), "abc");
	EXPECT_FALSE(engine.render("abc", "recurse").has_value());
}

TEST(engine, reports_running_out_of_memory_and_carries_on)
{
	auto engine = make_behaviours();
	auto const err = render_error(engine, "abc", "allocate");
	EXPECT_EQ(err.message, "out of memory");
	EXPECT_EQ(err.offset, std::nullopt);
	EXPECT_EQ(engine.render("abc", "echo"), "abc");
	EXPECT_FALSE(engine.render("abc", "allocate").has_value());
}

TEST(engine, fails_to_create_without_a_render_function)
{
	auto const err = create_error(no_render());
	EXPECT_EQ(err.message, "render is not a global function");
	EXPECT_EQ(err.offset, std::nullopt);
}

TEST(engine, fails_to_create_with_no_scripts)
{
	auto const err = create_error({});
	EXPECT_EQ(err.message, "render is not a global function");
}

TEST(engine, fails_to_create_when_a_script_throws_as_it_loads)
{
	auto const err = create_error(throws_on_load());
	EXPECT_EQ(err.message, "failed to load");
	EXPECT_EQ(err.offset, std::nullopt);
}

TEST(engine, fails_to_create_from_bytes_that_are_not_bytecode)
{
	std::array<std::uint8_t, 3> const junk{1, 2, 3};
	std::array<std::span<std::uint8_t const>, 1> const scripts{junk};
	EXPECT_FALSE(create_error(scripts).message.empty());
}

TEST(engine, fails_to_create_from_bytecode_of_another_version)
{
	auto const original = behaviours().at(0);
	std::vector<std::uint8_t> bytes{original.begin(), original.end()};
	++bytes.at(0);
	std::array<std::span<std::uint8_t const>, 1> const scripts{bytes};
	EXPECT_FALSE(create_error(scripts).message.empty());
}

TEST(engine, fails_to_create_from_truncated_bytecode)
{
	auto const original = behaviours().at(0);
	for (std::size_t size :
	     {std::size_t{0}, std::size_t{1}, original.size() / 2, original.size() - 1}) {
		std::vector<std::uint8_t> const bytes{original.begin(),
		                                      original.begin() + static_cast<std::ptrdiff_t>(size)};
		std::array<std::span<std::uint8_t const>, 1> const scripts{bytes};
		EXPECT_FALSE(create_error(scripts).message.empty()) << size;
	}
}

TEST(engine, runs_two_engines_independently)
{
	auto first = make_engine(echo());
	auto second = make_engine(echo());
	EXPECT_EQ(first.render("a", "b"), "<shim><b>a");
	EXPECT_EQ(second.render("c", "d"), "<shim><d>c");
}

TEST(engine, moves)
{
	auto engine = make_engine(echo());
	auto moved = std::move(engine);
	EXPECT_EQ(moved.render("a", "b"), "<shim><b>a");

	auto other = make_engine(behaviours());
	other = std::move(moved);
	EXPECT_EQ(other.render("a", "b"), "<shim><b>a");
}

TEST(engine, reports_a_moved_from_engine_as_an_error)
{
	auto engine = make_engine(echo());
	auto const moved = std::move(engine);
	// NOLINTNEXTLINE(bugprone-use-after-move,clang-analyzer-cplusplus.Move)
	auto const result = engine.render("a", "b");
	ASSERT_FALSE(result.has_value());
	EXPECT_EQ(result.error().message, "the engine was moved from");
	EXPECT_EQ(result.error().offset, std::nullopt);
}
