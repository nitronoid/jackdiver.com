#include <blog/file.hxx>

#include <array>
#include <cstdio>
#include <expected>
#include <filesystem>
#include <memory>
#include <string>
#include <system_error>

#include <blog/error.hxx>

namespace
{
struct file_closer {
	auto operator()(std::FILE* file) const noexcept -> void
	{
		// NOLINTNEXTLINE(cppcoreguidelines-owning-memory)
		std::fclose(file);
	}
};
}

namespace blog
{
auto read_file(std::filesystem::path const& path) -> std::expected<std::string, error>
{
	std::error_code status_error;
	if (std::filesystem::status(path, status_error).type() ==
	    std::filesystem::file_type::not_found) {
		return std::unexpected(errc::file_not_found);
	}

	/*
	 * stdio rather than an ifstream: an istream reports a failed read, such as reading
	 * a directory, as end of file, while ferror() tells the two apart.
	 */
	// NOLINTNEXTLINE(cppcoreguidelines-owning-memory)
	std::unique_ptr<std::FILE, file_closer> const file{std::fopen(path.c_str(), "rb")};
	if (!file) {
		return std::unexpected(errc::read_failed);
	}

	std::string content;
	std::array<char, BUFSIZ> buffer{};
	while (true) {
		auto const count = std::fread(buffer.data(), 1, buffer.size(), file.get());
		content.append(buffer.data(), count);
		if (count < buffer.size()) {
			break;
		}
	}
	if (std::ferror(file.get()) != 0) {
		return std::unexpected(errc::read_failed);
	}
	return content;
}
}
