/*
 * golden_test - Build tests/blog/fixture/ (a small `content/` tree and a stand-in `main.css`, so the
 * test does not change with the site's stylesheet) into a temporary directory, and compare it with
 * tests/blog/expected/: the bytes of every file and the list of files. A file the build writes that
 * is not expected, or an expected one it no longer writes, fails. The update command is
 * `bazel run //tests/blog:update`.
 */
#include <cstdlib>
#include <filesystem>
#include <optional>
#include <string>
#include <gtest/gtest.h>

#include <blog_golden/golden_tree.hxx>
#include <highlight/hljs.hxx>
#include <math/temml.hxx>

namespace
{
namespace fs = std::filesystem;

constexpr char const* update_command = "bazel run //tests/blog:update";

auto blog_dir() -> std::optional<fs::path>
{
	// NOLINTBEGIN(concurrency-mt-unsafe)
	char const* const srcdir = std::getenv("TEST_SRCDIR");
	char const* const workspace = std::getenv("TEST_WORKSPACE");
	// NOLINTEND(concurrency-mt-unsafe)
	if (srcdir == nullptr || workspace == nullptr) {
		return std::nullopt;
	}
	return fs::path{srcdir} / workspace / "tests" / "blog";
}
}

TEST(blog_golden, the_fixture_builds_to_the_expected_tree)
{
	auto const dir = blog_dir();
	ASSERT_TRUE(dir.has_value()) << "TEST_SRCDIR and TEST_WORKSPACE are set by `bazel test`";
	ASSERT_FALSE(blog_golden::files_under(dir.value() / "fixture").empty())
	    << "no fixture in " << dir.value().string();

	auto engine = math::temml::create();
	ASSERT_TRUE(engine.has_value()) << engine.error().message;
	auto highlighter = highlight::hljs::create();
	ASSERT_TRUE(highlighter.has_value()) << highlighter.error().message;
	auto const scratch = blog_golden::scratch_dir::create(::testing::TempDir(), "blog_golden");
	ASSERT_TRUE(scratch.has_value()) << scratch.error();
	auto const output = scratch->path() / "site";
	auto const built = blog_golden::build(dir.value() / "fixture", output, *engine, *highlighter);
	ASSERT_TRUE(built.has_value()) << built.error();

	auto const problems = blog_golden::compare(dir.value() / "expected", output);
	for (auto const& problem : problems) {
		ADD_FAILURE() << problem;
	}
	if (!problems.empty()) {
		ADD_FAILURE() << "If the change is intended, run: " << update_command;
	}
}
