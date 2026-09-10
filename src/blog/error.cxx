#include <cstdio>
#include <format>
#include <print>
#include <string>
#include <string_view>
#include <utility>
#include <variant>

#include <blog/error.hxx>
#include <jimd/result.hxx>

namespace blog
{
auto describe(errc code) noexcept -> std::string_view
{
	switch (code) {
	case errc::file_not_found:
		return "file not found";
	case errc::read_failed:
		return "read failed";
	case errc::write_failed:
		return "write failed";
	case errc::image_size:
		return "no usable image size";
	case errc::invalid_math:
		return "invalid maths";
	case errc::invalid_code:
		return "cannot highlight code";
	case errc::image_not_local:
		return "figure target is not a local path";
	case errc::bad_post_name:
		return "bad post file name";
	case errc::bad_page_name:
		return "bad page file name";
	case errc::missing_title:
		return "missing title";
	case errc::unknown_metadata:
		return "unknown metadata key";
	case errc::page_collision:
		return "page collides with a generated path";
	case errc::usage:
		return "usage: blog --content <dir> --css <file> -o <dir>";
	case errc::output_not_empty:
		return "output directory is not empty";
	case errc::static_collision:
		return "static file collides with a file the site writes";
	}
	std::unreachable();
}

auto format_error(error const& err, std::string_view path) -> std::string
{
	auto const description =
		std::visit([](auto const code) -> std::string_view { return describe(code); }, err.code);
	auto const* const own = std::get_if<errc>(&err.code);
	bool const is_usage = own != nullptr && *own == errc::usage;
	std::string out;
	if (!is_usage && err.line != 0) {
		out = std::format("{}:{}:{}: ", path, err.line, err.column);
	} else if (!is_usage && !path.empty()) {
		out = std::format("{}: ", path);
	}
	out += description;
	if (!err.detail.empty()) {
		out += ": ";
		out += err.detail;
	}
	return out;
}

auto handle_error(error const& err, std::string_view path) -> void
{
	std::println(stderr, "{}", format_error(err, path));
}
}
