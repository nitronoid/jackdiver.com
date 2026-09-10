/*
 * golden_test - Render every `.jimd` in tests/golden/ and compare the bytes with its `.html`. A
 * mismatch reports the first differing line. A `.jimd` with no `.html`, or an `.html` with no
 * `.jimd`, also fails. The update command is `bazel run //tests/golden:update`.
 */
#include <gtest/gtest.h>

#include <cstdlib>
#include <filesystem>
#include <optional>
#include <string>

#include <golden/golden.hxx>
#include <highlight/hljs.hxx>
#include <math/temml.hxx>

namespace
{
auto golden_dir() -> std::optional<std::filesystem::path>
{
	// NOLINTBEGIN(concurrency-mt-unsafe)
	char const* const srcdir = std::getenv("TEST_SRCDIR");
	char const* const workspace = std::getenv("TEST_WORKSPACE");
	// NOLINTEND(concurrency-mt-unsafe)
	if (srcdir == nullptr || workspace == nullptr) {
		return std::nullopt;
	}
	return std::filesystem::path{srcdir} / workspace / "tests" / "golden";
}

constexpr char const* update_command = "bazel run //tests/golden:update";

void check(std::filesystem::path const& input, math::temml& engine, highlight::hljs& highlighter)
{
	auto const name = input.filename().string();
	auto const expected_file = golden::expected_path(input);
	auto const expected = golden::read_file(expected_file);
	if (!expected) {
		ADD_FAILURE() << name << ": no " << expected_file.filename().string()
		              << " to compare with. To create it, run: " << update_command;
		return;
	}

	auto const actual = golden::render(input, engine, highlighter);
	if (!actual) {
		ADD_FAILURE() << actual.error();
		return;
	}

	if (auto const diff = golden::first_difference(*expected, *actual)) {
		ADD_FAILURE()
		    << name << ": the output differs from " << expected_file.filename().string() << "\n"
		    << *diff << "\nIf the change is intended, run: " << update_command;
	}
}
}

TEST(golden, every_input_matches_its_file)
{
	auto const dir = golden_dir();
	ASSERT_TRUE(dir.has_value()) << "TEST_SRCDIR and TEST_WORKSPACE are set by `bazel test`";

	auto const inputs = golden::inputs(dir.value());
	ASSERT_FALSE(inputs.empty()) << "no .jimd files in " << dir.value().string();

	auto engine = math::temml::create();
	ASSERT_TRUE(engine.has_value()) << engine.error().message;
	auto highlighter = highlight::hljs::create();
	ASSERT_TRUE(highlighter.has_value()) << highlighter.error().message;
	for (auto const& input : inputs) {
		check(input, engine.value(), highlighter.value());
	}
}

TEST(golden, every_html_file_has_an_input)
{
	auto const dir = golden_dir();
	ASSERT_TRUE(dir.has_value()) << "TEST_SRCDIR and TEST_WORKSPACE are set by `bazel test`";

	for (auto const& orphan : golden::orphans(dir.value())) {
		ADD_FAILURE() << orphan.filename().string() << " has no .jimd file next to it. Delete it";
	}
}

TEST(golden, the_crlf_inputs_have_crlf_line_ends)
{
	auto const dir = golden_dir();
	ASSERT_TRUE(dir.has_value()) << "TEST_SRCDIR and TEST_WORKSPACE are set by `bazel test`";

	for (auto const* const name : {"crlf.jimd", "code_crlf.jimd"}) {
		auto const source = golden::read_file(dir.value() / name);
		if (!source) {
			ADD_FAILURE() << name << " is missing";
			continue;
		}
		EXPECT_NE(source.value().find("\r\n"), std::string::npos) << name;
	}
}
