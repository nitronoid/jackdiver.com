#pragma once

#include <expected>
#include <filesystem>
#include <string>

#include <blog/error.hxx>

namespace blog
{
/**
 * read_file() - Read a whole file in binary mode.
 * @path: The file to read.
 *
 * Return: The contents, or errc::file_not_found or errc::read_failed.
 */
auto read_file(std::filesystem::path const& path) -> std::expected<std::string, error>;
}
