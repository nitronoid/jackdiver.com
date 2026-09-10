#include <jimd/ast.hxx>
#include <jimd/detail/align.hxx>
#include <jimd/detail/newline.hxx>
#include <jimd/detail/overloaded.hxx>

#include <string>
#include <variant>

namespace jimd::ast
{
namespace
{
auto append_plain(std::string& out, inlines const& nodes) -> void
{
	auto const visitor = detail::overloaded{
		[&out](text const& item) -> void { out += item.value; },
		[&out](soft_break const& /*item*/) -> void { out += ' '; },
		[&out](code_span const& item) -> void { out += item.value; },
		[&out](emphasis const& item) -> void { append_plain(out, item.children); },
		[&out](strong const& item) -> void { append_plain(out, item.children); },
		[&out](link const& item) -> void { out += item.text; },
		[&out](image const& item) -> void { out += item.alt; },
		[&out](math const& item) -> void { out += item.tex; },
		[](reference const& /*item*/) -> void {},
	};
	for (auto const& node : nodes) {
		std::visit(visitor, node);
	}
}
}

auto math_tex(math const& node) -> std::string
{
	return std::string{node.tex};
}

auto math_tex(display_math const& node) -> std::string
{
	return detail::crlf_to_lf(node.tex);
}

auto math_tex(aligned_math const& node) -> std::string
{
	std::string out{detail::align_begin};
	std::string_view joiner;
	for (auto const& row : node.rows) {
		out += joiner;
		out += row;
		joiner = detail::align_join;
	}
	out += detail::align_end;
	return out;
}

auto plain_text(inlines const& nodes) -> std::string
{
	std::string out;
	append_plain(out, nodes);
	return out;
}
}
