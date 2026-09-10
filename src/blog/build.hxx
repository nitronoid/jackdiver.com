#pragma once

#include <expected>
#include <filesystem>
#include <vector>

#include <blog/site.hxx>
#include <highlight/hljs.hxx>
#include <math/temml.hxx>

namespace blog
{
/**
 * struct build_options - Inputs and output of a blog build.
 */
struct build_options {
	/** @content: Directory holding posts/, pages/ and static/. */
	std::filesystem::path content;
	/** @css: Stylesheet copied to style/main.css. */
	std::filesystem::path css;
	/** @output: Directory to write; must be missing or empty. */
	std::filesystem::path output;
};

/**
 * build() - Build the whole blog.
 * @options: Where to read from and write to.
 * @engine: Maths engine used to render every post and page.
 * @highlighter: Highlighter used for the code blocks of every post and page.
 *
 * Nothing is written unless every input loads, parses and renders, and no static file
 * collides with a generated one.
 *
 * Return: Nothing on success, otherwise every error found, sorted by path.
 */
[[nodiscard]] auto build(build_options const& options, math::temml& engine,
                         highlight::hljs& highlighter)
	-> std::expected<void, std::vector<file_error>>;
}
