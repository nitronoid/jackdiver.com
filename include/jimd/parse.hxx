#pragma once

#include <string_view>

#include <jimd/ast.hxx>
#include <jimd/result.hxx>

namespace jimd
{
/**
 * parse() - Parse a jimd document.
 * @source: The document text, which must outlive the result.
 *
 * Checks the syntax, then that every bare reference names a definition.
 *
 * Return: The document, whose views all point into @source, or the first error.
 */
auto parse(std::string_view source [[clang::lifetimebound]]) -> result<ast::document>;
}
