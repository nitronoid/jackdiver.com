#include <support/result_printers.hxx>

#include <expected>
#include <format>
#include <ostream>

#include <blog/error.hxx>
#include <jimd/position.hxx>
#include <jimd/result.hxx>

namespace jimd
{
auto PrintTo(position const& pos, std::ostream* stream) -> void
{
	*stream << std::format("{}:{}", pos.line, pos.column);
}

auto PrintTo(error const& err, std::ostream* stream) -> void
{
	*stream << std::format("{} at {}:{}", describe(err.code), err.line, err.column);
}

auto PrintTo(std::unexpected<error> const& err, std::ostream* stream) -> void
{
	PrintTo(err.error(), stream);
}
}

namespace blog
{
auto PrintTo(error const& err, std::ostream* stream) -> void
{
	*stream << format_error(err, "<input>");
}
}
