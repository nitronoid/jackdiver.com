#pragma once

#include <expected>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>

#include <blog/error.hxx>

namespace blog
{
/**
 * view_box_ratio() - Read the aspect ratio from an SVG's viewBox.
 * @svg: The SVG document.
 *
 * Return: "width/height" with the numbers normalised, e.g. "16/9.5", or
 * errc::image_size with the reason as detail.
 */
auto view_box_ratio(std::string_view svg) -> std::expected<std::string, error>;

/**
 * local_path_problem() - Check that a figure target is a safe site-root path.
 * @target: The target, such as "/images/a.svg".
 *
 * The target must start with a single "/" and have no empty, "." or ".."
 * segment, no "?", "#" or "%", no backslash and no control character.
 *
 * Return: std::nullopt if @target is fine, else a phrase saying what is wrong.
 */
auto local_path_problem(std::string_view target) -> std::optional<std::string_view>;

/**
 * svg_ratio() - Read an SVG file and return its viewBox ratio.
 * @path: The SVG file.
 *
 * Return: As view_box_ratio(), or the error from read_file().
 */
auto svg_ratio(std::filesystem::path const& path) -> std::expected<std::string, error>;
}
