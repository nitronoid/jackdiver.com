#pragma once

#include <cstddef>
#include <string>

#include <jimd/ast.hxx>

namespace support
{
/**
 * dump() - Print a document's syntax tree, one node per line.
 * @doc: The document, with its metadata and definitions.
 *
 * Children are indented two spaces deeper than their parent.
 *
 * Return: The text, ending in a newline.
 */
auto dump(jimd::ast::document const& doc) -> std::string;

/**
 * dump() - Print a block and its children.
 * @block: The block.
 * @depth: Indentation of the first line, in steps of two spaces.
 *
 * Return: The text, ending in a newline.
 */
auto dump(jimd::ast::block const& block, std::size_t depth = 0) -> std::string;

/**
 * dump() - Print an inline node and its children.
 * @node: The inline node.
 * @depth: Indentation of the first line, in steps of two spaces.
 *
 * Return: The text, ending in a newline.
 */
auto dump(jimd::ast::inline_node const& node, std::size_t depth = 0) -> std::string;

/**
 * dump() - Print a document's link, image and footnote definitions.
 * @defs: The definitions.
 * @depth: Indentation of the first line, in steps of two spaces.
 *
 * Return: The text, ending in a newline.
 */
auto dump(jimd::ast::resources const& defs, std::size_t depth = 0) -> std::string;
}
