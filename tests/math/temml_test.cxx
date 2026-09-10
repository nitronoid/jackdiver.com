#include <cstddef>
#include <expected>
#include <optional>
#include <string>
#include <utility>
#include <gtest/gtest.h>

#include <math/temml.hxx>

namespace
{
auto make_engine() -> math::temml
{
	auto engine = math::temml::create();
	EXPECT_TRUE(engine.has_value());
	return std::move(engine).value();
}

auto render_error(math::temml& engine, std::string const& tex) -> math::error
{
	auto const result = engine.render(tex, math::mode::inline_math);
	EXPECT_FALSE(result.has_value()) << tex;
	if (result.has_value()) {
		return {};
	}
	return result.error();
}
}

TEST(temml, renders_inline_math)
{
	auto engine = make_engine();
	auto const result = engine.render("x^2", math::mode::inline_math);
	ASSERT_TRUE(result.has_value());
	EXPECT_TRUE(result->starts_with("<math>")) << *result;
	EXPECT_NE(result->find("<msup>"), std::string::npos);
}

TEST(temml, renders_display_style_as_a_block)
{
	auto engine = make_engine();
	auto const result = engine.render("x^2", math::mode::display);
	ASSERT_TRUE(result.has_value());
	EXPECT_TRUE(result->starts_with("<math display=\"block\"")) << *result;
}

TEST(temml, keeps_the_tex_as_an_annotation)
{
	auto engine = make_engine();
	auto const result = engine.render("x^2", math::mode::inline_math);
	ASSERT_TRUE(result.has_value());
	EXPECT_NE(result->find("<annotation encoding=\"application/x-tex\">x^2</annotation>"),
	          std::string::npos)
	    << *result;
}

TEST(temml, renders_an_empty_string)
{
	auto engine = make_engine();
	auto const result = engine.render("", math::mode::inline_math);
	ASSERT_TRUE(result.has_value());
	EXPECT_TRUE(result->starts_with("<math>")) << *result;
}

TEST(temml, renders_an_expression_with_underbraces)
{
	auto engine = make_engine();
	auto const result = engine.render(
		R"(f(t) = \underbrace{f_{steady}}_{\text{holds the target}} + \underbrace{f_{steady} \cdot \left(\frac{\tau}{\tau_{fast}} - 1\right) \cdot e^{-t/\tau_{fast}}}_{\text{boost that fades away}})",
		math::mode::display);
	ASSERT_TRUE(result.has_value());
	EXPECT_TRUE(result->starts_with("<math display=\"block\"")) << *result;
	EXPECT_NE(result->find("holds the target"), std::string::npos);
	EXPECT_NE(result->find("boost that fades away"), std::string::npos);
}

TEST(temml, reports_an_unknown_macro_at_the_start)
{
	auto engine = make_engine();
	auto const err = render_error(engine, R"(\foo)");
	EXPECT_EQ(err.message, R"(Unsupported function name: \foo)");
	EXPECT_EQ(err.offset, std::optional<std::size_t>{0});
}

TEST(temml, reports_the_offset_in_bytes_after_ascii)
{
	auto engine = make_engine();
	auto const err = render_error(engine, R"(a + \foo)");
	EXPECT_EQ(err.message, R"(Unsupported function name: \foo)");
	EXPECT_EQ(err.offset, std::optional<std::size_t>{4});
}

TEST(temml, reports_the_offset_in_bytes_after_a_two_byte_character)
{
	auto engine = make_engine();
	auto const err = render_error(engine, "30\xC2\xB0"
	                                      R"(C + \foo)");
	EXPECT_EQ(err.offset, std::optional<std::size_t>{8});
}

TEST(temml, reports_the_offset_in_bytes_after_a_surrogate_pair)
{
	auto engine = make_engine();
	auto const err = render_error(engine, "\xF0\x9F\x98\x80"
	                                      R"( + \foo)");
	EXPECT_EQ(err.offset, std::optional<std::size_t>{7});
}

TEST(temml, reports_the_offset_after_a_byte_that_is_not_utf8)
{
	auto engine = make_engine();
	auto const err = render_error(engine, "\xFF"
	                                      R"( + \foo)");
	EXPECT_EQ(err.offset, std::optional<std::size_t>{4});
}

TEST(temml, reports_the_end_of_the_input)
{
	auto engine = make_engine();
	auto const err = render_error(engine, "{");
	EXPECT_EQ(err.message, "Expected '}', got 'EOF'");
	EXPECT_EQ(err.offset, std::optional<std::size_t>{1});
}

TEST(temml, reports_the_end_of_the_input_after_a_multibyte_character)
{
	auto engine = make_engine();
	auto const err = render_error(engine, "\xF0\x9F\x98\x80"
	                                      "{");
	EXPECT_EQ(err.message, "Expected '}', got 'EOF'");
	EXPECT_EQ(err.offset, std::optional<std::size_t>{5});
}

TEST(temml, keeps_the_message_clear_of_the_excerpt_of_the_tex)
{
	auto engine = make_engine();
	auto const err = render_error(engine, R"(\foo at position 1: )");
	EXPECT_EQ(err.message, R"(Unsupported function name: \foo)");
	EXPECT_EQ(err.offset, std::optional<std::size_t>{0});
}

TEST(temml, keeps_a_message_that_holds_a_position)
{
	auto engine = make_engine();
	auto const err = render_error(engine, R"(\color{x at position 1: })");
	EXPECT_EQ(err.message, "Invalid color: 'x at position 1: '");
	EXPECT_EQ(err.offset, std::optional<std::size_t>{0});
}

TEST(temml, keeps_a_message_that_holds_the_end_of_the_input)
{
	auto engine = make_engine();
	auto const err = render_error(engine, R"(\color{x at end of input: })");
	EXPECT_EQ(err.message, "Invalid color: 'x at end of input: '");
	EXPECT_EQ(err.offset, std::optional<std::size_t>{0});
}

TEST(temml, keeps_a_message_that_holds_a_position_and_a_newline)
{
	auto engine = make_engine();
	auto const err = render_error(engine, "\\color{x at position 1: \n}");
	EXPECT_EQ(err.message, "Invalid color: 'x at position 1: '");
	EXPECT_EQ(err.offset, std::optional<std::size_t>{0});
}

TEST(temml, reports_an_exception_that_is_not_a_parse_error_without_an_offset)
{
	auto engine = make_engine();
	auto const err = render_error(engine, "x^");
	EXPECT_EQ(err.message, "TypeError: cannot read property 'type' of null");
	EXPECT_EQ(err.offset, std::nullopt);
}

TEST(temml, reports_a_parse_error_without_a_position_and_no_offset)
{
	auto engine = make_engine();
	auto const err = render_error(engine, R"(\def\a{\a}\a)");
	EXPECT_EQ(err.message,
	          "Too many expansions: infinite loop or need to increase maxExpand setting");
	EXPECT_EQ(err.offset, std::nullopt);
}

TEST(temml, reports_stack_exhaustion_as_an_error)
{
	auto engine = make_engine();
	auto const err = render_error(engine, std::string(100'000, '{'));
	EXPECT_EQ(err.message, "RangeError: Maximum call stack size exceeded");
	EXPECT_EQ(err.offset, std::nullopt);
	EXPECT_TRUE(engine.render("x", math::mode::inline_math).has_value());
}

TEST(temml, renders_again_after_a_failure)
{
	auto engine = make_engine();
	EXPECT_FALSE(engine.render(R"(\foo)", math::mode::inline_math).has_value());
	EXPECT_FALSE(engine.render("x^", math::mode::inline_math).has_value());
	auto const result = engine.render("x^2", math::mode::inline_math);
	ASSERT_TRUE(result.has_value());
	EXPECT_TRUE(result->starts_with("<math>")) << *result;
}

TEST(temml, gives_the_same_output_each_time)
{
	auto engine = make_engine();
	auto const first = engine.render(R"(\frac{a}{b})", math::mode::display);
	auto const second = engine.render(R"(\frac{a}{b})", math::mode::display);
	ASSERT_TRUE(first.has_value());
	EXPECT_EQ(first, second);
}

TEST(temml, reports_the_offset_after_sequences_that_quickjs_rejects)
{
	auto engine = make_engine();
	EXPECT_EQ(render_error(engine, "\xC0\x80"
	                               R"( + \foo)")
	              .offset,
	          std::optional<std::size_t>{5});
	EXPECT_EQ(render_error(engine, "\xF4\x90\x80\x80"
	                               R"( + \foo)")
	              .offset,
	          std::optional<std::size_t>{7});
}

TEST(temml, reports_a_formula_too_large_for_a_string_as_an_error)
{
	auto engine = make_engine();
	// The string fails before Temml runs, so the shim can't add QuickJS's "InternalError: ".
	auto const err = render_error(engine, std::string(std::size_t{80} * 1024 * 1024, 'x'));
	EXPECT_EQ(err.message, "out of memory");
	EXPECT_EQ(err.offset, std::nullopt);
	EXPECT_TRUE(engine.render("x", math::mode::inline_math).has_value());
}

TEST(temml, reports_running_out_of_memory_inside_temml_as_an_error)
{
	auto engine = make_engine();
	// QuickJS has nothing left to build its own error from, and throws null.
	constexpr std::size_t pairs = 100'000;
	std::string tex;
	for (std::size_t i = 0; i < pairs; ++i) {
		tex += "{}";
	}
	auto const err = render_error(engine, tex);
	EXPECT_EQ(err.message, "out of memory");
	EXPECT_EQ(err.offset, std::nullopt);
	EXPECT_TRUE(engine.render("x", math::mode::inline_math).has_value());
}

TEST(temml, renders_after_a_move)
{
	auto engine = make_engine();
	auto moved = std::move(engine);
	EXPECT_TRUE(moved.render("x", math::mode::inline_math).has_value());

	auto assigned = make_engine();
	assigned = std::move(moved);
	EXPECT_TRUE(assigned.render("x", math::mode::inline_math).has_value());
}

TEST(temml, reports_a_moved_from_engine_as_an_error)
{
	auto engine = make_engine();
	auto const moved = std::move(engine);
	// NOLINTNEXTLINE(bugprone-use-after-move,clang-analyzer-cplusplus.Move)
	auto const result = engine.render("x", math::mode::inline_math);
	ASSERT_FALSE(result.has_value());
	EXPECT_EQ(result.error().message, "the engine was moved from");
	EXPECT_EQ(result.error().offset, std::nullopt);
}
