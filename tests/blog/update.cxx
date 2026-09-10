/*
 * update - Regenerate tests/blog/expected/, the site that tests/blog/fixture/ builds to. Run it with
 * `bazel run //tests/blog:update`, then review the diff.
 *
 * It builds the fixture into a new temporary directory, then makes expected/ match it: it writes the
 * files that changed, and deletes the ones the build no longer writes and the directories that
 * leaves empty. It touches nothing outside expected/, and never writes through a link (it refuses
 * a link in place of expected/, and a file under a linked directory in it). If the fixture does not
 * build, it prints the errors and leaves everything as it was. It needs BUILD_WORKSPACE_DIRECTORY,
 * so it only runs under `bazel run`.
 */
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <exception>
#include <filesystem>
#include <print>
#include <ranges>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>

#include <blog_golden/golden_tree.hxx>
#include <golden/golden.hxx>
#include <highlight/hljs.hxx>
#include <math/temml.hxx>

namespace
{
namespace fs = std::filesystem;

auto through_link(fs::path const& root, fs::path const& target) -> bool
{
	auto here = root;
	std::error_code error;
	for (auto const& part : target.lexically_relative(root).parent_path()) {
		here /= part;
		if (fs::is_symlink(here, error)) {
			return true;
		}
	}
	return false;
}

auto write_changed(fs::path const& expected, fs::path const& actual) -> bool
{
	auto all_ok = true;
	for (auto const& name : blog_golden::files_under(actual)) {
		auto const target = expected / name;
		auto const content = golden::read_file(actual / name);
		if (!content) {
			std::println(stderr, "{}: cannot read the built file", name);
			all_ok = false;
			continue;
		}
		if (golden::read_file(target) == content) {
			continue;
		}
		if (through_link(expected, target)) {
			std::println(stderr, "{}: a directory on the way is a link; not written",
			             target.string());
			all_ok = false;
			continue;
		}
		auto const existed = fs::exists(target);
		if (golden::write_file(target, *content)) {
			std::println("{} {}", existed ? "updated" : "created", name);
		} else {
			std::println(stderr, "{}: could not write", target.string());
			all_ok = false;
		}
	}
	return all_ok;
}

auto delete_stale(fs::path const& expected, fs::path const& actual) -> bool
{
	auto all_ok = true;
	auto const built = blog_golden::files_under(actual);
	for (auto const& name : blog_golden::files_under(expected)) {
		if (std::ranges::binary_search(built, name)) {
			continue;
		}
		std::error_code error;
		fs::remove(expected / name, error);
		if (error) {
			std::println(stderr, "{}: could not delete", (expected / name).string());
			all_ok = false;
		} else {
			std::println("deleted {}", name);
		}
	}
	return all_ok;
}

auto remove_empty_directories(fs::path const& expected) -> void
{
	std::error_code error;
	if (!fs::is_directory(expected, error)) {
		return;
	}
	std::vector<fs::path> dirs;
	for (auto const& entry : fs::recursive_directory_iterator{expected}) {
		if (entry.is_directory() && !entry.is_symlink()) {
			dirs.push_back(entry.path());
		}
	}
	for (auto const& dir : dirs | std::views::reverse) {
		fs::remove(dir, error);
	}
}

auto sync(fs::path const& expected, fs::path const& actual) -> bool
{
	auto const deleted = delete_stale(expected, actual);
	remove_empty_directories(expected);
	auto const written = write_changed(expected, actual);
	return written && deleted;
}
}

auto main() -> int
try {
	// NOLINTNEXTLINE(concurrency-mt-unsafe)
	char const* const workspace = std::getenv("BUILD_WORKSPACE_DIRECTORY");
	if (workspace == nullptr || *workspace == '\0') {
		std::println(
			stderr,
			"BUILD_WORKSPACE_DIRECTORY is not set: run this with `bazel run //tests/blog:update`");
		return 1;
	}

	if (!fs::exists(fs::path{workspace} / "MODULE.bazel")) {
		std::println(stderr, "{} is not the repository root", workspace);
		return 1;
	}
	auto const blog = fs::path{workspace} / "tests" / "blog";
	auto const fixture = blog / "fixture";
	auto const expected = blog / "expected";
	if (!fs::is_directory(fixture / "content")) {
		std::println(stderr, "no fixture in {}", fixture.string());
		return 1;
	}

	if (fs::is_symlink(expected)) {
		std::println(stderr, "{} is a link; refusing to write through it", expected.string());
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

	auto const scratch =
		blog_golden::scratch_dir::create(fs::temp_directory_path(), "blog_golden_update");
	if (!scratch) {
		std::println(stderr, "{}", scratch.error());
		return 1;
	}
	auto const output = scratch->path() / "site";
	auto const built = blog_golden::build(fixture, output, *engine, *highlighter);
	if (!built) {
		std::print(stderr, "{}", built.error());
		return 1;
	}
	return sync(expected, output) ? 0 : 1;
} catch (std::exception const& ex) {
	std::fputs("fatal: ", stderr);
	std::fputs(ex.what(), stderr);
	std::fputc('\n', stderr);
	return 1;
} catch (...) {
	std::fputs("fatal: unknown exception\n", stderr);
	return 1;
}
