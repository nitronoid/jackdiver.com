/*
 * update - Regenerate the expected output of tests/golden/. Run it with
 * `bazel run //tests/golden:update`, then review the diff.
 *
 * For each `name.jimd` in the checkout (BUILD_WORKSPACE_DIRECTORY) it writes `name.html`, the HTML
 * fragment of the body with no page around it, and prints `created` or `updated` for each file that
 * changed. An input that does not parse is reported and nothing is written for it, and the exit
 * code is 1. It never deletes: an `.html` with no `.jimd` is reported and left alone, and golden_test
 * fails until it is deleted by hand. To add a case, write the `.jimd` and run this.
 */
#include <cstdio>
#include <cstdlib>
#include <exception>
#include <filesystem>
#include <print>
#include <string>

#include <golden/golden.hxx>
#include <highlight/hljs.hxx>
#include <math/temml.hxx>

namespace
{
auto update(std::filesystem::path const& input, math::temml& engine, highlight::hljs& highlighter)
	-> bool
{
	auto const fragment = golden::render(input, engine, highlighter);
	if (!fragment) {
		std::println(stderr, "{}", fragment.error());
		return false;
	}

	auto const target = golden::expected_path(input);
	auto const before = golden::read_file(target);
	if (before == *fragment) {
		return true;
	}
	auto const existed = std::filesystem::exists(target);
	if (!golden::write_file(target, *fragment)) {
		std::println(stderr, "{}: could not write", target.string());
		return false;
	}
	std::println("{} {}", existed ? "updated" : "created", target.filename().string());
	return true;
}
}

auto main() -> int
try {
	// NOLINTNEXTLINE(concurrency-mt-unsafe)
	char const* const workspace = std::getenv("BUILD_WORKSPACE_DIRECTORY");
	if (workspace == nullptr || *workspace == '\0') {
		std::println(
			stderr,
			"BUILD_WORKSPACE_DIRECTORY is not set: run this with `bazel run //tests/golden:update`");
		return 1;
	}

	auto const dir = std::filesystem::path{workspace} / "tests" / "golden";
	auto const inputs = golden::inputs(dir);
	if (inputs.empty()) {
		std::println(stderr, "no .jimd files in {}", dir.string());
		return 1;
	}

	auto engine = math::temml::create();
	if (!engine) {
		std::println(stderr, "cannot start the maths engine: {}", engine.error().message);
		return 1;
	}

	auto highlighter = highlight::hljs::create();
	if (!highlighter) {
		std::println(stderr, "cannot start the highlighter: {}", highlighter.error().message);
		return 1;
	}

	auto all_ok = true;
	for (auto const& input : inputs) {
		all_ok = update(input, *engine, *highlighter) && all_ok;
	}
	for (auto const& orphan : golden::orphans(dir)) {
		std::println(stderr, "{} has no .jimd file next to it (left alone)",
		             orphan.filename().string());
	}
	return all_ok ? 0 : 1;
} catch (std::exception const& ex) {
	std::fputs("fatal: ", stderr);
	std::fputs(ex.what(), stderr);
	std::fputc('\n', stderr);
	return 1;
} catch (...) {
	std::fputs("fatal: unknown exception\n", stderr);
	return 1;
}
