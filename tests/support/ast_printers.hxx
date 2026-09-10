#pragma once

#include <iosfwd>

#include <jimd/ast.hxx>

namespace jimd::ast
{
/**
 * PrintTo() - Print an AST value for GoogleTest, as support::dump() renders it.
 * @doc: The value to print.
 * @stream: The stream to print to.
 *
 * There is one overload for the document, each variant and each node type.
 */
auto PrintTo(document const& doc, std::ostream* stream) -> void;
auto PrintTo(block const& block, std::ostream* stream) -> void;
auto PrintTo(inline_node const& node, std::ostream* stream) -> void;
auto PrintTo(resources const& defs, std::ostream* stream) -> void;
/**
 * PrintTo() - Print an image definition as its target and optional width.
 * @def: The definition to print.
 * @stream: The stream to print to.
 */
auto PrintTo(image_def const& def, std::ostream* stream) -> void;

auto PrintTo(paragraph const& node, std::ostream* stream) -> void;
auto PrintTo(heading const& node, std::ostream* stream) -> void;
auto PrintTo(quote const& node, std::ostream* stream) -> void;
auto PrintTo(code_block const& node, std::ostream* stream) -> void;
auto PrintTo(list const& node, std::ostream* stream) -> void;
auto PrintTo(display_math const& node, std::ostream* stream) -> void;
auto PrintTo(aligned_math const& node, std::ostream* stream) -> void;

auto PrintTo(text const& node, std::ostream* stream) -> void;
auto PrintTo(soft_break const& node, std::ostream* stream) -> void;
auto PrintTo(code_span const& node, std::ostream* stream) -> void;
auto PrintTo(emphasis const& node, std::ostream* stream) -> void;
auto PrintTo(strong const& node, std::ostream* stream) -> void;
auto PrintTo(link const& node, std::ostream* stream) -> void;
auto PrintTo(image const& node, std::ostream* stream) -> void;
auto PrintTo(reference const& node, std::ostream* stream) -> void;
auto PrintTo(math const& node, std::ostream* stream) -> void;
}
