#include <blog_golden/golden_tree.hxx>

#include <algorithm>
#include <expected>
#include <filesystem>
#include <format>
#include <random>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

#include <blog/build.hxx>
#include <blog/error.hxx>
#include <golden/golden.hxx>
#include <highlight/hljs.hxx>
#include <math/temml.hxx>

namespace blog_golden
{
namespace fs = std::filesystem;

auto scratch_dir::create(fs::path const& base, std::string_view stem)
	-> std::expected<scratch_dir, std::string>
{
	std::error_code error;
	fs::create_directories(base, error);
	std::random_device random;
	constexpr auto attempts = 100;
	for (auto attempt = 0; attempt < attempts; ++attempt) {
		auto const path = base / std::format("{}.{:08x}{:08x}", stem, random(), random());
		if (fs::create_directory(path, error)) {
			fs::permissions(path, fs::perms::owner_all, fs::perm_options::replace, error);
			return scratch_dir{path};
		}
		if (error) {
			break;
		}
	}
	return std::unexpected(std::format("cannot make a new directory under {}", base.string()));
}

scratch_dir::scratch_dir(scratch_dir&& other) noexcept
	: path_{std::exchange(other.path_, fs::path{})}
{
}

scratch_dir::~scratch_dir()
{
	if (!path_.empty()) {
		std::error_code ignored;
		fs::remove_all(path_, ignored);
	}
}

auto build(fs::path const& fixture, fs::path const& output, math::temml& engine,
           highlight::hljs& highlighter) -> std::expected<void, std::string>
{
	auto const built =
		blog::build({.content = fixture / "content", .css = fixture / "main.css", .output = output},
		            engine, highlighter);
	if (built) {
		return {};
	}
	std::string message;
	for (auto const& err : built.error()) {
		message += blog::format_error(err.error, err.path.string());
		message += '\n';
	}
	return std::unexpected(std::move(message));
}

auto files_under(fs::path const& dir) -> std::vector<std::string>
{
	std::vector<std::string> found;
	std::error_code status_error;
	if (!fs::is_directory(dir, status_error)) {
		return found;
	}
	for (auto const& entry : fs::recursive_directory_iterator{dir}) {
		if (entry.is_regular_file()) {
			found.push_back(entry.path().lexically_relative(dir).generic_string());
		}
	}
	std::ranges::sort(found);
	return found;
}

auto compare(fs::path const& expected, fs::path const& actual) -> std::vector<std::string>
{
	auto const wanted = files_under(expected);
	auto const written = files_under(actual);
	std::vector<std::string> problems;
	for (auto const& name : wanted) {
		if (!std::ranges::binary_search(written, name)) {
			problems.push_back(std::format("{}: the build does not write it", name));
		}
	}
	for (auto const& name : written) {
		if (!std::ranges::binary_search(wanted, name)) {
			problems.push_back(
				std::format("{}: the build writes it and there is no expected file", name));
			continue;
		}
		auto const want = golden::read_file(expected / name);
		auto const got = golden::read_file(actual / name);
		if (!want || !got) {
			problems.push_back(std::format("{}: cannot be read", name));
		} else if (auto const diff = golden::first_difference(*want, *got)) {
			problems.push_back(std::format("{}: differs from the expected file\n{}", name, *diff));
		}
	}
	return problems;
}
}
