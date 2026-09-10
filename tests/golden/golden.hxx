#pragma once

#include <expected>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include <highlight/hljs.hxx>
#include <math/temml.hxx>

namespace golden
{
/**
 * inputs() - List the ``.jimd`` files in a directory.
 * @dir: The directory to search, not recursively.
 *
 * Return: The paths, sorted.
 */
auto inputs(std::filesystem::path const& dir) -> std::vector<std::filesystem::path>;

/**
 * orphans() - List the ``.html`` files in a directory that have no ``.jimd`` input.
 * @dir: The directory to search, not recursively.
 *
 * Return: The paths, sorted.
 */
auto orphans(std::filesystem::path const& dir) -> std::vector<std::filesystem::path>;

/**
 * expected_path() - Name the expected output for an input.
 * @input: The ``.jimd`` file.
 *
 * Return: @input with its extension replaced by ``.html``.
 */
auto expected_path(std::filesystem::path const& input) -> std::filesystem::path;

/**
 * read_file() - Read a whole file as bytes.
 * @path: The file to read.
 *
 * Return: The contents, or nullopt if @path is not a readable regular file.
 */
auto read_file(std::filesystem::path const& path) -> std::optional<std::string>;

/**
 * write_file() - Write a file through a ``.tmp`` sibling, creating its directories.
 * @path: The file to write.
 * @content: The bytes to write.
 *
 * Return: True on success. On failure the temporary file is removed.
 */
auto write_file(std::filesystem::path const& path, std::string_view content) -> bool;

/**
 * render() - Render a ``.jimd`` file to an HTML fragment.
 * @input: The file to render. Its directory is the static root.
 * @engine: The maths engine.
 * @highlighter: The code highlighter.
 *
 * Return: The fragment, or a formatted error message.
 */
auto render(std::filesystem::path const& input, math::temml& engine, highlight::hljs& highlighter)
	-> std::expected<std::string, std::string>;

/**
 * first_difference() - Describe where two texts first differ.
 * @expected: The wanted text.
 * @actual: The text produced.
 *
 * Return: nullopt if they are equal, else the line, column and byte of the first difference,
 * the line from each text and both lengths.
 */
auto first_difference(std::string_view expected, std::string_view actual)
	-> std::optional<std::string>;
}
