#include <math/temml.hxx>

#include <expected>
#include <string>
#include <string_view>
#include <utility>

#include <js/engine.hxx>
#include <temml_bytecode.h>

namespace math
{
namespace
{
auto from_js(js::error err) -> error
{
	return {.message = std::move(err.message), .offset = err.offset};
}
}

temml::temml(js::engine engine) noexcept
	: engine_{std::move(engine)}
{
}

auto temml::create() -> std::expected<temml, error>
{
	auto engine = js::engine::create(temml_bytecode());
	if (!engine) {
		return std::unexpected(from_js(std::move(engine).error()));
	}
	return temml{*std::move(engine)};
}

auto temml::render(std::string_view tex, mode style) -> std::expected<std::string, error>
{
	auto markup = engine_.render(tex, style == mode::display ? "display" : "inline");
	if (!markup) {
		return std::unexpected(from_js(std::move(markup).error()));
	}
	return *std::move(markup);
}
}
