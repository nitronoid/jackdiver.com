#include <support/ast_printers.hxx>

#include <format>
#include <ostream>

#include <jimd/ast.hxx>
#include <support/dump.hxx>

namespace jimd::ast
{
auto PrintTo(document const& doc, std::ostream* stream) -> void
{
	*stream << support::dump(doc);
}

auto PrintTo(block const& block, std::ostream* stream) -> void
{
	*stream << support::dump(block);
}

auto PrintTo(inline_node const& node, std::ostream* stream) -> void
{
	*stream << support::dump(node);
}

auto PrintTo(resources const& defs, std::ostream* stream) -> void
{
	*stream << support::dump(defs);
}

auto PrintTo(image_def const& def, std::ostream* stream) -> void
{
	*stream << std::format("image_def target={:?}", def.target);
	if (def.width) {
		*stream << std::format(" width={:?}", *def.width);
	}
}

auto PrintTo(paragraph const& node, std::ostream* stream) -> void
{
	*stream << support::dump(block{node});
}

auto PrintTo(heading const& node, std::ostream* stream) -> void
{
	*stream << support::dump(block{node});
}

auto PrintTo(quote const& node, std::ostream* stream) -> void
{
	*stream << support::dump(block{node});
}

auto PrintTo(code_block const& node, std::ostream* stream) -> void
{
	*stream << support::dump(block{node});
}

auto PrintTo(list const& node, std::ostream* stream) -> void
{
	*stream << support::dump(block{node});
}

auto PrintTo(display_math const& node, std::ostream* stream) -> void
{
	*stream << support::dump(block{node});
}

auto PrintTo(aligned_math const& node, std::ostream* stream) -> void
{
	*stream << support::dump(block{node});
}

auto PrintTo(text const& node, std::ostream* stream) -> void
{
	*stream << support::dump(inline_node{node});
}

auto PrintTo(soft_break const& node, std::ostream* stream) -> void
{
	*stream << support::dump(inline_node{node});
}

auto PrintTo(code_span const& node, std::ostream* stream) -> void
{
	*stream << support::dump(inline_node{node});
}

auto PrintTo(emphasis const& node, std::ostream* stream) -> void
{
	*stream << support::dump(inline_node{node});
}

auto PrintTo(strong const& node, std::ostream* stream) -> void
{
	*stream << support::dump(inline_node{node});
}

auto PrintTo(link const& node, std::ostream* stream) -> void
{
	*stream << support::dump(inline_node{node});
}

auto PrintTo(image const& node, std::ostream* stream) -> void
{
	*stream << support::dump(inline_node{node});
}

auto PrintTo(reference const& node, std::ostream* stream) -> void
{
	*stream << support::dump(inline_node{node});
}

auto PrintTo(math const& node, std::ostream* stream) -> void
{
	*stream << support::dump(inline_node{node});
}
}
