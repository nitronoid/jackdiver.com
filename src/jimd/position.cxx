#include <cstddef>
#include <string_view>

#include <jimd/detail/position.hxx>
#include <jimd/position.hxx>

namespace jimd
{
auto position_of(std::string_view source, std::string_view part) noexcept -> position
{
	if (!detail::contains(source, part)) {
		return position{};
	}

	auto const* const src_begin = source.data();
	// NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic)
	auto const offset = static_cast<std::size_t>(part.data() - src_begin);
	std::string_view const prefix{src_begin, offset};

	std::size_t line = 1;
	std::size_t column = 1;
	for (char const chr : prefix) {
		if (chr == '\n') {
			++line;
			column = 1;
		} else {
			++column;
		}
	}

	return position{.line = line, .column = column};
}
}
