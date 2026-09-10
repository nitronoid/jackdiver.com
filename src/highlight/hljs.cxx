#include <highlight/hljs.hxx>

#include <expected>
#include <string>
#include <string_view>
#include <utility>

#include <hljs_bytecode.h>
#include <js/engine.hxx>

namespace highlight
{
namespace
{
auto from_js(js::error err) -> error
{
	return {.message = std::move(err.message)};
}
}

hljs::hljs(js::engine engine) noexcept
	: engine_{std::move(engine)}
{
}

auto hljs::create() -> std::expected<hljs, error>
{
	auto engine = js::engine::create(hljs_bytecode());
	if (!engine) {
		return std::unexpected(from_js(std::move(engine).error()));
	}
	return hljs{*std::move(engine)};
}

auto hljs::render(std::string_view language, std::string_view code)
	-> std::expected<std::string, error>
{
	// The engine's render(input, arg) is the shim's render(code, language).
	auto html = engine_.render(code, language);
	if (!html) {
		return std::unexpected(from_js(std::move(html).error()));
	}
	return *std::move(html);
}
}
