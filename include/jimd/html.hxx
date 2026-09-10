#pragma once

#include <cstdint>
#include <flat_map>
#include <functional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <jimd/ast.hxx>
#include <jimd/result.hxx>

namespace jimd::html
{
/**
 * escape_text() - Escape text for HTML element content.
 * @input: The text.
 * @out: The string to append to.
 *
 * Escapes ``&``, ``<`` and ``>``.
 */
auto escape_text(std::string_view input, std::string& out) -> void;

/**
 * escape_attr() - Escape text for a double-quoted HTML attribute.
 * @input: The text.
 * @out: The string to append to.
 *
 * Escapes ``&``, ``<``, ``>`` and ``"``.
 */
auto escape_attr(std::string_view input, std::string& out) -> void;

/**
 * slugify() - Make a heading id from its text.
 * @nodes: The heading content.
 *
 * Keeps ASCII letters, lower-cased, and digits, drops apostrophes, and joins the
 * words with single hyphens.
 *
 * Return: The slug, or "section" if no characters are left.
 */
auto slugify(ast::inlines const& nodes) -> std::string;

/**
 * smarten() - Apply smart punctuation.
 * @text: UTF-8 text.
 * @before: The character before @text, which decides whether a quote opens; ``\n`` for none.
 *
 * Turns ``...`` into an ellipsis, ``--`` into an em dash, and straight quotes into curly ones.
 *
 * Return: The smartened text.
 */
auto smarten(std::string_view text, char32_t before = U'\n') -> std::string;

/**
 * enum math_mode - How maths is laid out.
 */
enum class math_mode : std::uint8_t {
	/** @inline_math: Within a line of text. */
	inline_math,
	/** @display: As its own block. */
	display,
};

/**
 * struct math_request - Maths that a document needs rendered.
 */
struct math_request {
	/** @mode: The layout. */
	math_mode mode = math_mode::inline_math;
	/** @tex: The TeX to render, from ast::math_tex(). */
	std::string tex;
	/** @origin: The source text @tex was made from. */
	std::string_view origin;
	/** @rows: The rows of aligned maths, or empty for other maths. */
	std::vector<std::string_view> rows;

	/**
	 * locate() - Map an offset in @tex back to the source.
	 * @offset: A byte offset into @tex, such as one from a renderer's error.
	 *
	 * Return: An empty view into the source at that place.
	 */
	[[nodiscard]] auto locate(std::size_t offset) const -> std::string_view;
};

/**
 * math_requests() - List the maths in a document.
 * @doc: The document.
 *
 * Maths in note definitions is placed by its position in the source. A request whose
 * @origin is outside doc.source, as in a tree not built by parse(), goes after the others, blocks
 * before note definitions.
 *
 * Return: One request per maths node, duplicates included, in source order.
 */
auto math_requests(ast::document const& doc) -> std::vector<math_request>;

/**
 * typedef math_table - Rendered maths, keyed by layout and TeX.
 */
using math_table = std::flat_map<std::pair<math_mode, std::string>, std::string, std::less<>>;

/**
 * struct code_request - A code block that a document may have highlighted.
 */
struct code_request {
	/** @language: The first word of the info string, the ``language-`` class; a view into it. */
	std::string_view language;
	/** @body: The block's body with CRLF turned into LF, as render() writes it. */
	std::string body;
};

/**
 * code_requests() - List the code blocks in a document that have a language.
 * @doc: The document.
 *
 * Looks in top-level blocks and quotes, which is where code blocks can be. A block with no info
 * string is not listed.
 *
 * Return: One request per code block, duplicates included, in source order.
 */
auto code_requests(ast::document const& doc) -> std::vector<code_request>;

/**
 * typedef code_table - Highlighted code as HTML, keyed by language and body.
 *
 * The HTML is already escaped, and render() writes it as it is.
 */
using code_table = std::flat_map<std::pair<std::string, std::string>, std::string, std::less<>>;

/**
 * is_figure() - Check whether a paragraph is a figure.
 * @node: The paragraph.
 *
 * Return: True if @node holds one image and nothing else but blanks.
 */
auto is_figure(ast::paragraph const& node) -> bool;

/**
 * struct svg_figure - A figure whose image is an SVG.
 */
struct svg_figure {
	/** @target: The image path or URL. */
	std::string_view target;
	/** @key: The image's key, for error messages. */
	std::string_view key;
};

/**
 * svg_figures() - List the SVG figures in a document.
 * @doc: The document.
 *
 * Looks in top-level paragraphs and quotes, and skips images that do not resolve.
 *
 * Return: The figures, in source order.
 */
auto svg_figures(ast::document const& doc) -> std::vector<svg_figure>;

/**
 * struct render_options - What render() needs beyond the document.
 */
struct render_options {
	/** @math: The rendered maths, or nullptr if there is none. */
	math_table const* math = nullptr;
	/** @code: The highlighted code, or nullptr if there is none. */
	code_table const* code = nullptr;
	/** @image_ratios: CSS aspect ratios of SVG figures by target, or nullptr if none. */
	std::flat_map<std::string, std::string, std::less<>> const* image_ratios = nullptr;
};

/**
 * render() - Render a document as an HTML fragment.
 * @doc: The document.
 * @options: The rendered maths, highlighted code and SVG figure ratios.
 *
 * Each note becomes an ``<aside>`` after the top-level block that cites it. Maths
 * missing from @options is an error, as is an SVG figure with no ratio. A code block missing
 * from @options is written as plain text, not an error.
 *
 * Return: The HTML, or the first error.
 */
auto render(ast::document const& doc, render_options const& options = {}) -> result<std::string>;
}
