#include <golden/golden.hxx>

#include <algorithm>
#include <cstddef>
#include <expected>
#include <filesystem>
#include <format>
#include <fstream>
#include <iterator>
#include <optional>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

#include <blog/error.hxx>
#include <blog/render.hxx>
#include <highlight/hljs.hxx>
#include <jimd/parse.hxx>
#include <math/temml.hxx>

namespace golden
{
namespace
{
auto files_with_extension(std::filesystem::path const& dir, std::string_view extension)
	-> std::vector<std::filesystem::path>
{
	std::vector<std::filesystem::path> found;
	for (auto const& entry : std::filesystem::directory_iterator{dir}) {
		if (entry.is_regular_file() && entry.path().extension() == extension) {
			found.push_back(entry.path());
		}
	}
	std::ranges::sort(found);
	return found;
}

constexpr unsigned char delete_code = 0x7f;

auto escape(std::string_view text) -> std::string
{
	std::string out;
	for (char const byte : text) {
		switch (byte) {
		case '\n':
			out += "\\n";
			break;
		case '\r':
			out += "\\r";
			break;
		case '\t':
			out += "\\t";
			break;
		case '"':
			out += "\\\"";
			break;
		case '\\':
			out += "\\\\";
			break;
		default:
			if (auto const code = static_cast<unsigned char>(byte);
			    code < ' ' || code == delete_code) {
				out += std::format("\\x{:02x}", static_cast<unsigned>(code));
			} else {
				out += byte;
			}
		}
	}
	return out;
}

auto describe_line(std::string_view text, std::size_t start) -> std::string
{
	if (start >= text.size()) {
		return "<end of file>";
	}
	auto const end = text.find('\n', start);
	auto const length = end == std::string_view::npos ? std::string_view::npos : end - start + 1;
	return std::format("\"{}\"", escape(text.substr(start, length)));
}
}

auto inputs(std::filesystem::path const& dir) -> std::vector<std::filesystem::path>
{
	return files_with_extension(dir, ".jimd");
}

auto orphans(std::filesystem::path const& dir) -> std::vector<std::filesystem::path>
{
	auto found = files_with_extension(dir, ".html");
	std::erase_if(found, [](std::filesystem::path const& html) -> bool {
		return std::filesystem::is_regular_file(
			std::filesystem::path{html}.replace_extension(".jimd"));
	});
	return found;
}

auto expected_path(std::filesystem::path const& input) -> std::filesystem::path
{
	return std::filesystem::path{input}.replace_extension(".html");
}

auto read_file(std::filesystem::path const& path) -> std::optional<std::string>
{
	if (!std::filesystem::is_regular_file(path)) {
		return std::nullopt;
	}
	std::ifstream file{path, std::ios::binary};
	if (!file.is_open()) {
		return std::nullopt;
	}
	std::string content{std::istreambuf_iterator<char>{file}, {}};
	if (file.bad()) {
		return std::nullopt;
	}
	return content;
}

auto write_file(std::filesystem::path const& path, std::string_view content) -> bool
{
	std::error_code error;
	std::filesystem::create_directories(path.parent_path(), error);
	if (error) {
		return false;
	}
	auto temporary = path;
	temporary += ".tmp";
	std::filesystem::remove(temporary, error);

	std::ofstream file{temporary, std::ios::binary};
	file.write(content.data(), static_cast<std::streamsize>(content.size()));
	file.close();

	if (file) {
		std::filesystem::rename(temporary, path, error);
	}
	if (!file || error) {
		std::filesystem::remove(temporary, error);
		return false;
	}
	return true;
}

auto render(std::filesystem::path const& input, math::temml& engine, highlight::hljs& highlighter)
	-> std::expected<std::string, std::string>
{
	auto const path = input.string();
	auto const source = read_file(input);
	if (!source) {
		return std::unexpected(blog::format_error(blog::errc::read_failed, path));
	}

	auto const tree = jimd::parse(*source);
	if (!tree) {
		return std::unexpected(blog::format_error(tree.error(), path));
	}

	blog::render_context const context{
		.engine = engine, .highlighter = highlighter, .static_root = input.parent_path()};
	auto fragment = blog::render_fragment(*tree, context);
	if (!fragment) {
		return std::unexpected(blog::format_error(fragment.error(), path));
	}
	return *std::move(fragment);
}

auto first_difference(std::string_view expected, std::string_view actual)
	-> std::optional<std::string>
{
	if (expected == actual) {
		return std::nullopt;
	}

	auto const same = std::ranges::mismatch(expected, actual);
	auto const offset = static_cast<std::size_t>(same.in1 - expected.begin());
	auto const line = 1 + static_cast<std::size_t>(std::count(expected.begin(), same.in1, '\n'));
	auto const line_start = expected.substr(0, offset).rfind('\n') + 1;
	return std::format("first difference at line {}, column {} (byte {})\n  expected: {}\n"
	                   "  actual:   {}\n  (lengths: expected {}, actual {})",
	                   line, offset - line_start + 1, offset, describe_line(expected, line_start),
	                   describe_line(actual, line_start), expected.size(), actual.size());
}
}
