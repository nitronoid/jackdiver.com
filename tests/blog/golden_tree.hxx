#pragma once

#include <expected>
#include <filesystem>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <highlight/hljs.hxx>
#include <math/temml.hxx>

namespace blog_golden
{
/**
 * class scratch_dir - A uniquely named directory that is removed with its contents when destroyed.
 */
class scratch_dir {
public:
	/**
	 * create() - Make a new owner-only directory named ``<stem>.<random hex>``.
	 * @base: The parent directory, created if missing.
	 * @stem: The start of the directory name.
	 *
	 * Return: The directory, or an error message if none could be made.
	 */
	static auto create(std::filesystem::path const& base, std::string_view stem)
		-> std::expected<scratch_dir, std::string>;

	/**
	 * scratch_dir() - Take ownership of @other's directory, leaving @other empty.
	 * @other: The directory to move from.
	 */
	scratch_dir(scratch_dir&& other) noexcept;
	scratch_dir(scratch_dir const&) = delete;
	auto operator=(scratch_dir const&) -> scratch_dir& = delete;
	auto operator=(scratch_dir&&) -> scratch_dir& = delete;
	~scratch_dir();

	/**
	 * path() - Get the directory's path.
	 *
	 * Return: The path, or an empty path if moved from.
	 */
	[[nodiscard]] auto path() const -> std::filesystem::path const&
	{
		return path_;
	}

private:
	explicit scratch_dir(std::filesystem::path path)
		: path_{std::move(path)}
	{
	}

	std::filesystem::path path_;
};

/**
 * build() - Build the blog in a fixture with blog::build().
 * @fixture: The fixture holding ``content/`` and ``main.css``.
 * @output: The directory to write the site to.
 * @engine: The maths engine.
 * @highlighter: The code highlighter.
 *
 * Return: Nothing on success, else every formatted error, one per line.
 */
auto build(std::filesystem::path const& fixture, std::filesystem::path const& output,
           math::temml& engine, highlight::hljs& highlighter) -> std::expected<void, std::string>;

/**
 * files_under() - List the regular files under a directory, recursively.
 * @dir: The directory to search.
 *
 * Return: Sorted generic paths relative to @dir; empty if @dir is not a directory.
 */
auto files_under(std::filesystem::path const& dir) -> std::vector<std::string>;

/**
 * compare() - Compare an output tree with an expected tree.
 * @expected: The expected tree.
 * @actual: The tree the build wrote.
 *
 * Return: One message per missing, extra, unreadable or differing file; empty if they match.
 */
auto compare(std::filesystem::path const& expected, std::filesystem::path const& actual)
	-> std::vector<std::string>;
}
