#pragma once

#include <expected>
#include <filesystem>
#include <string>

#include <blog/error.hxx>
#include <highlight/hljs.hxx>
#include <jimd/ast.hxx>
#include <math/temml.hxx>

namespace blog
{
/**
 * struct render_context - What rendering needs beyond the document.
 */
struct render_context {
	/** @engine: Renders the document's maths. */
	// NOLINTNEXTLINE(cppcoreguidelines-avoid-const-or-ref-data-members)
	math::temml& engine;

	/** @highlighter: Highlights the document's code blocks that have a language. */
	// NOLINTNEXTLINE(cppcoreguidelines-avoid-const-or-ref-data-members)
	highlight::hljs& highlighter;

	/** @static_root: Directory that local SVG figure paths resolve against. */
	std::filesystem::path static_root;
};

/**
 * render_fragment() - Render a document to an HTML fragment.
 * @doc: The parsed document.
 * @context: The maths engine, highlighter and static root.
 *
 * Renders each distinct formula, and highlights each distinct code block that
 * has a language, once, and reads each SVG figure's aspect ratio from under
 * @context.static_root.
 *
 * Return: The HTML, or the first maths, code, figure or render error,
 * positioned in @doc's source.
 */
auto render_fragment(jimd::ast::document const& doc, render_context const& context)
	-> std::expected<std::string, error>;
}
