#pragma once

#include <cstddef>
#include <flat_map>
#include <optional>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

namespace jimd::ast
{
struct inline_node;
/**
 * typedef inlines - A run of inline nodes, in source order.
 */
using inlines = std::vector<inline_node>;

/**
 * struct text - Plain text, or one escaped character.
 */
struct text {
	/** @value: The text as written; smart punctuation is applied when rendering. */
	std::string_view value;
	auto operator==(text const&) const -> bool = default;
};

/**
 * struct soft_break - A line break inside a paragraph.
 */
struct soft_break {
	auto operator==(soft_break const&) const -> bool = default;
};

/**
 * struct code_span - Inline code between backticks.
 */
struct code_span {
	/** @value: The code, without the backticks. */
	std::string_view value;
	auto operator==(code_span const&) const -> bool = default;
};

/**
 * struct emphasis - Text between underscores.
 */
struct emphasis {
	/** @children: The emphasised content. */
	inlines children;
	auto operator==(emphasis const&) const -> bool = default;
};

/**
 * struct strong - Text between asterisks.
 */
struct strong {
	/** @children: The emboldened content. */
	inlines children;
	auto operator==(strong const&) const -> bool = default;
};

/**
 * struct link - An inline link ``[@text](target)``, or a reference ``[@key]`` to a definition.
 */
struct link {
	/** @key: The text between ``[@`` and ``]``. */
	std::string_view key;
	/** @text: The text to show, which is @key. */
	std::string_view text;
	/** @target: The inline target, or nullopt to look @key up in resources::links. */
	std::optional<std::string_view> target;
	auto operator==(link const&) const -> bool = default;
};

/**
 * struct image - An inline image ``[!alt](target)``, or a reference ``[!key]`` to a definition.
 */
struct image {
	/** @key: The text between ``[!`` and ``]``. */
	std::string_view key;
	/** @alt: The alternative text, which is @key; may be empty on an inline image. */
	std::string_view alt;
	/** @target: The inline target, or nullopt to look @key up in resources::images. */
	std::optional<std::string_view> target;
	/** @width: The inline target's trailing width, such as ``50%``, if it has one. */
	std::optional<std::string_view> width;
	auto operator==(image const&) const -> bool = default;
};

/**
 * struct reference - An inline note ``[^key](content)``, or a reference ``[^key]`` to one.
 */
struct reference {
	/** @key: The text between ``[^`` and ``]``; only decorative, and may be empty, inline. */
	std::string_view key;
	/** @content: The inline note, or nullopt to look @key up in resources::footnotes. */
	std::optional<inlines> content;
	auto operator==(reference const&) const -> bool = default;
};

/**
 * struct math - Inline maths between dollars.
 */
struct math {
	/** @tex: The TeX, without the dollars. */
	std::string_view tex;
	auto operator==(math const&) const -> bool = default;
};

/**
 * struct inline_node - Any inline node.
 */
struct inline_node
    : std::variant<text, soft_break, code_span, emphasis, strong, link, image, reference, math> {
	using variant::variant;
	auto operator==(inline_node const&) const -> bool = default;
};

struct block;

/**
 * struct paragraph - A run of text lines.
 */
struct paragraph {
	/** @children: The content, with soft_break between lines. */
	inlines children;
	auto operator==(paragraph const&) const -> bool = default;
};

/**
 * struct heading - A ``#`` heading.
 */
struct heading {
	/** @level: The number of ``#`` characters; rendering clamps it to 1 to 6. */
	std::size_t level{};
	/** @children: The heading text. */
	inlines children;
	auto operator==(heading const&) const -> bool = default;
};

/**
 * struct quote - A block quote between a ``>`` line and a ``<`` line.
 */
struct quote {
	/** @children: The quoted blocks. */
	std::vector<block> children;
	auto operator==(quote const&) const -> bool = default;
};

/**
 * struct code_block - A fenced code block.
 */
struct code_block {
	/** @info: The text after the opening fence, or empty; its first word names the language. */
	std::string_view info;
	/** @body: The lines between the fences, with line endings as written. */
	std::string_view body;
	auto operator==(code_block const&) const -> bool = default;
};

/**
 * struct list - A bullet list of ``- `` lines.
 */
struct list {
	/** @items: The content of each item. */
	std::vector<inlines> items;
	auto operator==(list const&) const -> bool = default;
};

/**
 * struct display_math - A ``$$`` block holding one equation.
 */
struct display_math {
	/** @tex: The lines between the fences, with line endings as written. */
	std::string_view tex;
	auto operator==(display_math const&) const -> bool = default;
};

/**
 * struct aligned_math - A ``$$$`` block of aligned equations, one row per line.
 */
struct aligned_math {
	/** @rows: Each line without its ending; leading and trailing blank lines are dropped. */
	std::vector<std::string_view> rows;
	auto operator==(aligned_math const&) const -> bool = default;
};

/**
 * struct block - Any block node.
 */
struct block
    : std::variant<paragraph, heading, quote, code_block, list, display_math, aligned_math> {
	using variant::variant;
	auto operator==(block const&) const -> bool = default;
};

/**
 * struct image_def - Where an image is, and how wide to show it.
 */
struct image_def {
	/** @target: The image path or URL. */
	std::string_view target;
	/** @width: A width such as ``50%``, or nullopt for the natural width. */
	std::optional<std::string_view> width;
	auto operator==(image_def const&) const -> bool = default;
};

/**
 * struct resources - The definitions in a document, by key.
 */
struct resources {
	/** @links: Link targets, from ``[@key]: target`` lines. */
	std::flat_map<std::string_view, std::string_view> links;
	/** @images: Images, from ``[!key]: target`` lines. */
	std::flat_map<std::string_view, image_def> images;
	/** @footnotes: Note content, from ``[^key]: content`` lines. */
	std::flat_map<std::string_view, inlines> footnotes;
	auto operator==(resources const&) const -> bool = default;
};

/**
 * struct document - A parsed jimd document.
 */
struct document {
	/** @source: The text that was parsed; every view in the document points into it. */
	std::string_view source;
	/** @metadata: The ``[%key]: value`` lines before the first block. */
	std::flat_map<std::string_view, std::string_view> metadata;
	/** @blocks: The top-level blocks, without definitions. */
	std::vector<block> blocks;
	/** @defs: The definitions. */
	resources defs;
	auto operator==(document const&) const -> bool = default;
};

/**
 * math_tex() - Get the TeX to render for inline maths.
 * @node: The maths.
 *
 * Return: @node's TeX, unchanged.
 */
auto math_tex(math const& node) -> std::string;

/**
 * math_tex() - Get the TeX to render for a display equation.
 * @node: The equation.
 *
 * Return: @node's TeX, with CRLF line endings turned into LF.
 */
auto math_tex(display_math const& node) -> std::string;

/**
 * math_tex() - Get the TeX to render for aligned equations.
 * @node: The equations.
 *
 * Return: @node's rows joined by ``\\`` in an ``align*`` environment.
 */
auto math_tex(aligned_math const& node) -> std::string;

/**
 * plain_text() - Get the text of inline nodes without their markup.
 * @nodes: The nodes.
 *
 * Soft breaks become spaces, images give their alternative text and notes are dropped.
 *
 * Return: The text.
 */
auto plain_text(inlines const& nodes) -> std::string;
}
